/* GStreamer
 *
 * Copyright (C) 2014-2015 Sebastian Dröge <sebastian@centricular.com>
 * Copyright (C) 2015 Brijesh Singh <brijesh.ksingh@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

/**
 * SECTION:gstplayer
 * @short_description: GStreamer Player API
 *
 */

/* TODO:
 *
 * - external subtitles
 * - Visualization
 * - Playback rate
 * - volume/mute change notification
 * - Equalizer
 * - Gapless playback
 * - Frame stepping
 * - Subtitle font, connection speed
 * - Color balance, deinterlacing
 * - Buffering control (-> progressive downloading)
 * - Playlist/queue object
 * - Custom video sink (e.g. embed in GL scene)
 *
 */

#include "gstplayer.h"
#include "gstplayer-media-info-private.h"

#include <gst/gst.h>
#include <gst/video/video.h>
#include <gst/tag/tag.h>
#include <gst/pbutils/descriptions.h>

GST_DEBUG_CATEGORY_STATIC (gst_player_debug);
#define GST_CAT_DEFAULT gst_player_debug

GQuark
gst_player_error_quark (void)
{
  static GQuark quark;

  if (!quark)
    quark = g_quark_from_static_string ("gst-player-error-quark");

  return quark;
}

enum
{
  PROP_0,
  PROP_DISPATCH_TO_MAIN_CONTEXT,
  PROP_URI,
  PROP_POSITION,
  PROP_DURATION,
  PROP_MEDIA_INFO,
  PROP_CURRENT_AUDIO_TRACK,
  PROP_CURRENT_VIDEO_TRACK,
  PROP_CURRENT_SUBTITLE_TRACK,
  PROP_VOLUME,
  PROP_MUTE,
  PROP_WINDOW_HANDLE,
  PROP_PIPELINE,
  PROP_LAST
};

enum
{
  SIGNAL_POSITION_UPDATED,
  SIGNAL_DURATION_CHANGED,
  SIGNAL_STATE_CHANGED,
  SIGNAL_BUFFERING,
  SIGNAL_END_OF_STREAM,
  SIGNAL_ERROR,
  SIGNAL_VIDEO_DIMENSIONS_CHANGED,
  SIGNAL_MEDIA_INFO_UPDATED,
  SIGNAL_LAST
};

enum
{
  GST_PLAY_FLAG_VIDEO = (1 << 0),
  GST_PLAY_FLAG_AUDIO = (1 << 1),
  GST_PLAY_FLAG_SUBTITLE = (1 << 2)
};

struct _GstPlayer
{
  GstObject parent;

  gboolean dispatch_to_main_context;
  GMainContext *application_context;

  gchar *uri;

  GThread *thread;
  GMutex lock;
  GCond cond;
  GMainContext *context;
  GMainLoop *loop;

  guintptr window_handle;

  GstElement *playbin;
  GstBus *bus;
  GstState target_state, current_state;
  gboolean is_live, is_eos;
  GSource *tick_source, *ready_timeout_source;

  GstPlayerState app_state;
  gint buffering;

  GstTagList *global_tags;
  GstPlayerMediaInfo *media_info;

  /* Protected by lock */
  gboolean seek_pending;        /* Only set from main context */
  GstClockTime last_seek_time;  /* Only set from main context */
  GSource *seek_source;
  GstClockTime seek_position;
};

struct _GstPlayerClass
{
  GstObjectClass parent_class;
};

#define parent_class gst_player_parent_class
G_DEFINE_TYPE (GstPlayer, gst_player, GST_TYPE_OBJECT);

static guint signals[SIGNAL_LAST] = { 0, };
static GParamSpec *param_specs[PROP_LAST] = { NULL, };

static void gst_player_finalize (GObject * object);
static void gst_player_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_player_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gpointer gst_player_main (gpointer data);

static void gst_player_seek_internal_locked (GstPlayer * self);
static gboolean gst_player_stop_internal (gpointer user_data);
static gboolean gst_player_pause_internal (gpointer user_data);
static gboolean gst_player_play_internal (gpointer user_data);
static void change_state (GstPlayer * self, GstPlayerState state);

static GstPlayerMediaInfo *gst_player_media_info_create (GstPlayer * self);

static void gst_player_streams_info_create (GstPlayer * self,
    GstPlayerMediaInfo * media_info, const gchar * prop, GType type);
static void gst_player_stream_info_update (GstPlayer * self,
    GstPlayerStreamInfo * s);
static void gst_player_stream_info_update_tags_and_caps (GstPlayer * self,
    GstPlayerStreamInfo * s);
static GstPlayerStreamInfo *gst_player_stream_info_find (GstPlayer * self,
    GstPlayerMediaInfo * media_info, GType type, gint stream_index);
static GstPlayerStreamInfo *gst_player_stream_info_get_current (GstPlayer *
    self, const gchar * prop, GType type);

static void gst_player_video_info_update (GstPlayer * self,
    GstPlayerStreamInfo * stream_info);
static void gst_player_audio_info_update (GstPlayer * self,
    GstPlayerStreamInfo * stream_info);
static void gst_player_subtitle_info_update (GstPlayer * self,
    GstPlayerStreamInfo * stream_info);

static void emit_media_info_updated_signal (GstPlayer * self);

static void *get_title (GstTagList * tags);
static void *get_container_format (GstTagList * tags);
static void *get_from_tags (GstPlayer * self, GstPlayerMediaInfo * media_info,
    void *(*func) (GstTagList *));
static void *get_cover_sample (GstTagList * tags);

static void
gst_player_init (GstPlayer * self)
{
  GST_TRACE_OBJECT (self, "Initializing");

  self = gst_player_get_instance_private (self);

  g_mutex_init (&self->lock);
  g_cond_init (&self->cond);

  self->seek_pending = FALSE;
  self->seek_position = GST_CLOCK_TIME_NONE;
  self->last_seek_time = GST_CLOCK_TIME_NONE;

  g_mutex_lock (&self->lock);
  self->thread = g_thread_new ("GstPlayer", gst_player_main, self);
  while (!self->loop || !g_main_loop_is_running (self->loop))
    g_cond_wait (&self->cond, &self->lock);
  g_mutex_unlock (&self->lock);
  GST_TRACE_OBJECT (self, "Initialized");
}

static void
gst_player_class_init (GstPlayerClass * klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->set_property = gst_player_set_property;
  gobject_class->get_property = gst_player_get_property;
  gobject_class->finalize = gst_player_finalize;

  param_specs[PROP_DISPATCH_TO_MAIN_CONTEXT] =
      g_param_spec_boolean ("dispatch-to-main-context",
      "Dispatch to main context", "Dispatch to the thread default main context",
      FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_URI] = g_param_spec_string ("uri", "URI", "Current URI",
      NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_POSITION] =
      g_param_spec_uint64 ("position", "Position", "Current Position",
      0, G_MAXUINT64, 0, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_MEDIA_INFO] =
      g_param_spec_object ("media-info", "Media Info",
      "Current media information", GST_TYPE_PLAYER_MEDIA_INFO,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_CURRENT_AUDIO_TRACK] =
      g_param_spec_object ("current-audio-track", "Current Audio Track",
      "Current audio track information", GST_TYPE_PLAYER_AUDIO_INFO,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_CURRENT_VIDEO_TRACK] =
      g_param_spec_object ("current-video-track", "Current Video Track",
      "Current video track information", GST_TYPE_PLAYER_VIDEO_INFO,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_CURRENT_SUBTITLE_TRACK] =
      g_param_spec_object ("current-subtitle-track", "Current Subtitle Track",
      "Current audio subtitle information", GST_TYPE_PLAYER_SUBTITLE_INFO,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_DURATION] =
      g_param_spec_uint64 ("duration", "Duration", "Duration",
      0, G_MAXUINT64, 0, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_VOLUME] =
      g_param_spec_double ("volume", "Volume", "Volume",
      0, 10.0, 1.0, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_MUTE] =
      g_param_spec_boolean ("mute", "Mute", "Mute",
      FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_WINDOW_HANDLE] =
      g_param_spec_pointer ("window-handle", "Window Handle",
      "Window handle into which the video should be rendered",
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_PIPELINE] =
      g_param_spec_object ("pipeline", "Pipeline",
      "GStreamer pipeline that is used",
      GST_TYPE_ELEMENT, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);

  signals[SIGNAL_POSITION_UPDATED] =
      g_signal_new ("position-updated", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 1, GST_TYPE_CLOCK_TIME);

  signals[SIGNAL_DURATION_CHANGED] =
      g_signal_new ("duration-changed", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 1, GST_TYPE_CLOCK_TIME);

  signals[SIGNAL_STATE_CHANGED] =
      g_signal_new ("state-changed", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 1, GST_TYPE_PLAYER_STATE);

  signals[SIGNAL_BUFFERING] =
      g_signal_new ("buffering", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 1, G_TYPE_INT);

  signals[SIGNAL_END_OF_STREAM] =
      g_signal_new ("end-of-stream", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 0, G_TYPE_INVALID);

  signals[SIGNAL_ERROR] =
      g_signal_new ("error", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 1, G_TYPE_ERROR);

  signals[SIGNAL_VIDEO_DIMENSIONS_CHANGED] =
      g_signal_new ("video-dimensions-changed", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_INT);

  signals[SIGNAL_MEDIA_INFO_UPDATED] =
      g_signal_new ("media-info-updated", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 1, GST_TYPE_PLAYER_MEDIA_INFO);
}

static void
gst_player_finalize (GObject * object)
{
  GstPlayer *self = GST_PLAYER (object);

  GST_TRACE_OBJECT (self, "Stopping main thread");
  g_main_loop_quit (self->loop);
  g_thread_join (self->thread);

  GST_TRACE_OBJECT (self, "Finalizing");

  g_free (self->uri);
  if (self->global_tags)
    gst_tag_list_unref (self->global_tags);
  if (self->application_context)
    g_main_context_unref (self->application_context);

  g_mutex_clear (&self->lock);
  g_cond_clear (&self->cond);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
gst_player_set_uri_internal (gpointer user_data)
{
  GstPlayer *self = user_data;

  gst_player_stop_internal (self);

  g_mutex_lock (&self->lock);

  GST_DEBUG_OBJECT (self, "Changing URI to '%s'", GST_STR_NULL (self->uri));

  g_object_set (self->playbin, "uri", self->uri, NULL);

  g_mutex_unlock (&self->lock);

  return G_SOURCE_REMOVE;
}

static void
gst_player_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstPlayer *self = GST_PLAYER (object);

  switch (prop_id) {
    case PROP_DISPATCH_TO_MAIN_CONTEXT:
      self->dispatch_to_main_context = g_value_get_boolean (value);
      self->application_context = g_main_context_ref_thread_default ();
      break;
    case PROP_URI:{
      g_mutex_lock (&self->lock);
      if (self->uri)
        g_free (self->uri);

      self->uri = g_value_dup_string (value);
      GST_DEBUG_OBJECT (self, "Set uri=%s", self->uri);
      g_mutex_unlock (&self->lock);

      g_main_context_invoke (self->context, gst_player_set_uri_internal, self);
      break;
    }
    case PROP_VOLUME:
      GST_DEBUG_OBJECT (self, "Set volume=%lf", g_value_get_double (value));
      g_object_set_property (G_OBJECT (self->playbin), "volume", value);
      break;
    case PROP_MUTE:
      GST_DEBUG_OBJECT (self, "Set mute=%d", g_value_get_boolean (value));
      g_object_set_property (G_OBJECT (self->playbin), "mute", value);
      break;
    case PROP_WINDOW_HANDLE:
      GST_DEBUG_OBJECT (self, "Set window handle from %p to %p",
          (gpointer) self->window_handle, g_value_get_pointer (value));
      self->window_handle = (guintptr) g_value_get_pointer (value);
      gst_video_overlay_set_window_handle (GST_VIDEO_OVERLAY (self->playbin),
          self->window_handle);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_player_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstPlayer *self = GST_PLAYER (object);

  switch (prop_id) {
    case PROP_URI:
      g_mutex_lock (&self->lock);
      g_value_set_string (value, self->uri);
      g_mutex_unlock (&self->lock);
      break;
      GST_TRACE_OBJECT (self, "Returning is-playing=%d",
          g_value_get_boolean (value));
      break;
    case PROP_POSITION:{
      gint64 position;

      gst_element_query_position (self->playbin, GST_FORMAT_TIME, &position);
      g_value_set_uint64 (value, position);
      GST_TRACE_OBJECT (self, "Returning position=%" GST_TIME_FORMAT,
          GST_TIME_ARGS (g_value_get_uint64 (value)));
      break;
    }
    case PROP_DURATION:{
      gint64 duration;

      gst_element_query_duration (self->playbin, GST_FORMAT_TIME, &duration);
      g_value_set_uint64 (value, duration);
      GST_TRACE_OBJECT (self, "Returning duration=%" GST_TIME_FORMAT,
          GST_TIME_ARGS (g_value_get_uint64 (value)));
      break;
    }
    case PROP_MEDIA_INFO:{
      GstPlayerMediaInfo *media_info = gst_player_get_media_info (self);
      g_value_set_object (value, media_info);
      g_object_unref (media_info);
      break;
    }
    case PROP_CURRENT_AUDIO_TRACK:{
      GstPlayerAudioInfo *audio_info =
          gst_player_get_current_audio_track (self);
      g_value_set_object (value, audio_info);
      g_object_unref (audio_info);
      break;
    }
    case PROP_CURRENT_VIDEO_TRACK:{
      GstPlayerVideoInfo *video_info =
          gst_player_get_current_video_track (self);
      g_value_set_object (value, video_info);
      g_object_unref (video_info);
      break;
    }
    case PROP_CURRENT_SUBTITLE_TRACK:{
      GstPlayerSubtitleInfo *subtitle_info =
          gst_player_get_current_subtitle_track (self);
      g_value_set_object (value, subtitle_info);
      g_object_unref (subtitle_info);
      break;
    }
    case PROP_VOLUME:
      g_object_get_property (G_OBJECT (self->playbin), "volume", value);
      GST_TRACE_OBJECT (self, "Returning volume=%lf",
          g_value_get_double (value));
      break;
    case PROP_MUTE:
      g_object_get_property (G_OBJECT (self->playbin), "mute", value);
      GST_TRACE_OBJECT (self, "Returning mute=%d", g_value_get_boolean (value));
      break;
    case PROP_WINDOW_HANDLE:
      g_value_set_pointer (value, (gpointer) self->window_handle);
      GST_TRACE_OBJECT (self, "Returning window-handle=%p",
          g_value_get_pointer (value));
      break;
    case PROP_PIPELINE:
      g_value_set_object (value, self->playbin);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
main_loop_running_cb (gpointer user_data)
{
  GstPlayer *self = GST_PLAYER (user_data);

  GST_TRACE_OBJECT (self, "Main loop running now");

  g_mutex_lock (&self->lock);
  g_cond_signal (&self->cond);
  g_mutex_unlock (&self->lock);

  return G_SOURCE_REMOVE;
}

typedef struct
{
  GstPlayer *player;
  GstPlayerState state;
} StateChangedSignalData;

static gboolean
state_changed_dispatch (gpointer user_data)
{
  StateChangedSignalData *data = user_data;

  g_signal_emit (data->player, signals[SIGNAL_STATE_CHANGED], 0, data->state);

  return G_SOURCE_REMOVE;
}

static void
change_state (GstPlayer * self, GstPlayerState state)
{
  if (state == self->app_state)
    return;

  GST_DEBUG_OBJECT (self, "Changing app state from %s to %s",
      gst_player_state_get_name (self->app_state),
      gst_player_state_get_name (state));
  self->app_state = state;

  if (self->dispatch_to_main_context
      && g_signal_handler_find (self, G_SIGNAL_MATCH_ID,
          signals[SIGNAL_STATE_CHANGED], 0, NULL, NULL, NULL) != 0) {
    StateChangedSignalData *data = g_new (StateChangedSignalData, 1);

    data->player = self;
    data->state = state;
    g_main_context_invoke_full (self->application_context,
        G_PRIORITY_DEFAULT, state_changed_dispatch, data,
        (GDestroyNotify) g_free);
  } else {
    g_signal_emit (self, signals[SIGNAL_STATE_CHANGED], 0, state);
  }
}

typedef struct
{
  GstPlayer *player;
  GstClockTime position;
} PositionUpdatedSignalData;

static gboolean
position_updated_dispatch (gpointer user_data)
{
  PositionUpdatedSignalData *data = user_data;

  g_signal_emit (data->player, signals[SIGNAL_POSITION_UPDATED], 0,
      data->position);
  g_object_notify_by_pspec (G_OBJECT (data->player),
      param_specs[PROP_POSITION]);

  return G_SOURCE_REMOVE;
}

static gboolean
tick_cb (gpointer user_data)
{
  GstPlayer *self = GST_PLAYER (user_data);
  gint64 position;

  if (gst_element_query_position (self->playbin, GST_FORMAT_TIME, &position)) {
    GST_LOG_OBJECT (self, "Position %" GST_TIME_FORMAT,
        GST_TIME_ARGS (position));

    if (self->dispatch_to_main_context
        && g_signal_handler_find (self, G_SIGNAL_MATCH_ID,
            signals[SIGNAL_POSITION_UPDATED], 0, NULL, NULL, NULL) != 0) {
      PositionUpdatedSignalData *data = g_new (PositionUpdatedSignalData, 1);

      data->player = self;
      data->position = position;
      g_main_context_invoke_full (self->application_context,
          G_PRIORITY_DEFAULT, position_updated_dispatch, data,
          (GDestroyNotify) g_free);
    } else {
      g_signal_emit (self, signals[SIGNAL_POSITION_UPDATED], 0, position);
      g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_POSITION]);
    }
  }

  return G_SOURCE_CONTINUE;
}

static void
add_tick_source (GstPlayer * self)
{
  if (self->tick_source)
    return;

  self->tick_source = g_timeout_source_new (100);
  g_source_set_callback (self->tick_source, (GSourceFunc) tick_cb, self, NULL);
  g_source_attach (self->tick_source, self->context);
}

static void
remove_tick_source (GstPlayer * self)
{
  if (!self->tick_source)
    return;

  g_source_destroy (self->tick_source);
  g_source_unref (self->tick_source);
  self->tick_source = NULL;
}

static gboolean
ready_timeout_cb (gpointer user_data)
{
  GstPlayer *self = user_data;

  if (self->target_state <= GST_STATE_READY) {
    GST_DEBUG_OBJECT (self, "Setting pipeline to NULL state");
    self->target_state = GST_STATE_NULL;
    self->current_state = GST_STATE_NULL;
    gst_element_set_state (self->playbin, GST_STATE_NULL);
  }

  return G_SOURCE_REMOVE;
}

static void
add_ready_timeout_source (GstPlayer * self)
{
  if (self->ready_timeout_source)
    return;

  self->ready_timeout_source = g_timeout_source_new_seconds (60);
  g_source_set_callback (self->ready_timeout_source,
      (GSourceFunc) ready_timeout_cb, self, NULL);
  g_source_attach (self->ready_timeout_source, self->context);
}

static void
remove_ready_timeout_source (GstPlayer * self)
{
  if (!self->ready_timeout_source)
    return;

  g_source_destroy (self->ready_timeout_source);
  g_source_unref (self->ready_timeout_source);
  self->ready_timeout_source = NULL;
}

typedef struct
{
  GstPlayer *player;
  GError *err;
} ErrorSignalData;

static gboolean
error_dispatch (gpointer user_data)
{
  ErrorSignalData *data = user_data;

  g_signal_emit (data->player, signals[SIGNAL_ERROR], 0, data->err);

  return G_SOURCE_REMOVE;
}

static void
free_error_signal_data (ErrorSignalData * data)
{
  g_clear_error (&data->err);
  g_free (data);
}

static void
emit_error (GstPlayer * self, GError * err)
{
  GST_ERROR_OBJECT (self, "Error: %s (%s, %d)", err->message,
      g_quark_to_string (err->domain), err->code);

  if (self->dispatch_to_main_context
      && g_signal_handler_find (self, G_SIGNAL_MATCH_ID,
          signals[SIGNAL_ERROR], 0, NULL, NULL, NULL) != 0) {
    ErrorSignalData *data = g_new (ErrorSignalData, 1);

    data->player = self;
    data->err = g_error_copy (err);
    g_main_context_invoke_full (self->application_context,
        G_PRIORITY_DEFAULT, error_dispatch, data,
        (GDestroyNotify) free_error_signal_data);
  } else {
    g_signal_emit (self, signals[SIGNAL_ERROR], 0, err);
  }

  g_error_free (err);

  remove_tick_source (self);
  remove_ready_timeout_source (self);

  self->target_state = GST_STATE_NULL;
  self->current_state = GST_STATE_NULL;
  self->is_live = FALSE;
  self->is_eos = FALSE;
  gst_element_set_state (self->playbin, GST_STATE_NULL);
  change_state (self, GST_PLAYER_STATE_STOPPED);
  self->buffering = 100;

  g_mutex_lock (&self->lock);
  if (self->media_info) {
    g_object_unref (self->media_info);
    self->media_info = NULL;
  }

  if (self->global_tags) {
    gst_tag_list_unref (self->global_tags);
    self->global_tags = NULL;
  }

  self->seek_pending = FALSE;
  if (self->seek_source) {
    g_source_destroy (self->seek_source);
    g_source_unref (self->seek_source);
    self->seek_source = NULL;
  }
  self->seek_position = GST_CLOCK_TIME_NONE;
  self->last_seek_time = GST_CLOCK_TIME_NONE;
  g_mutex_unlock (&self->lock);
}

static void
dump_dot_file (GstPlayer * self, const gchar * name)
{
  gchar *full_name;

  full_name = g_strdup_printf ("gst-player.%p.%s", self, name);

  GST_DEBUG_BIN_TO_DOT_FILE_WITH_TS (GST_BIN (self->playbin),
      GST_DEBUG_GRAPH_SHOW_ALL, full_name);

  g_free (full_name);
}

static void
error_cb (GstBus * bus, GstMessage * msg, gpointer user_data)
{
  GstPlayer *self = GST_PLAYER (user_data);
  GError *err, *player_err;
  gchar *name, *debug, *message, *full_message;

  dump_dot_file (self, "error");

  gst_message_parse_error (msg, &err, &debug);

  name = gst_object_get_path_string (msg->src);
  message = gst_error_get_message (err->domain, err->code);

  if (debug)
    full_message =
        g_strdup_printf ("Error from element %s: %s\n%s\n%s", name, message,
        err->message, debug);
  else
    full_message =
        g_strdup_printf ("Error from element %s: %s\n%s", name, message,
        err->message);

  GST_ERROR_OBJECT (self, "ERROR: from element %s: %s\n", name, err->message);
  if (debug != NULL)
    GST_ERROR_OBJECT (self, "Additional debug info:\n%s\n", debug);

  player_err =
      g_error_new_literal (GST_PLAYER_ERROR, GST_PLAYER_ERROR_FAILED,
      full_message);
  emit_error (self, player_err);

  g_clear_error (&err);
  g_free (debug);
  g_free (name);
  g_free (full_message);
  g_free (message);
}

static void
warning_cb (GstBus * bus, GstMessage * msg, gpointer user_data)
{
  GstPlayer *self = GST_PLAYER (user_data);
  GError *err;
  gchar *name, *debug, *message, *full_message;

  dump_dot_file (self, "warning");

  gst_message_parse_warning (msg, &err, &debug);

  name = gst_object_get_path_string (msg->src);
  message = gst_error_get_message (err->domain, err->code);

  if (debug)
    full_message =
        g_strdup_printf ("Warning from element %s: %s\n%s\n%s", name, message,
        err->message, debug);
  else
    full_message =
        g_strdup_printf ("Warning from element %s: %s\n%s", name, message,
        err->message);

  GST_WARNING_OBJECT (self, "WARNING: from element %s: %s\n", name,
      err->message);
  if (debug != NULL)
    GST_WARNING_OBJECT (self, "Additional debug info:\n%s\n", debug);

  g_clear_error (&err);
  g_free (debug);
  g_free (name);
  g_free (full_message);
  g_free (message);
}

static gboolean
eos_dispatch (gpointer user_data)
{
  g_signal_emit (user_data, signals[SIGNAL_END_OF_STREAM], 0);

  return G_SOURCE_REMOVE;
}

static void
eos_cb (GstBus * bus, GstMessage * msg, gpointer user_data)
{
  GstPlayer *self = GST_PLAYER (user_data);

  GST_DEBUG_OBJECT (self, "End of stream");

  tick_cb (self);
  remove_tick_source (self);

  if (self->dispatch_to_main_context
      && g_signal_handler_find (self, G_SIGNAL_MATCH_ID,
          signals[SIGNAL_END_OF_STREAM], 0, NULL, NULL, NULL) != 0) {
    g_main_context_invoke (self->application_context, eos_dispatch, self);
  } else {
    g_signal_emit (self, signals[SIGNAL_END_OF_STREAM], 0);
  }
  change_state (self, GST_PLAYER_STATE_STOPPED);
  self->buffering = 100;
  self->is_eos = TRUE;
}

typedef struct
{
  GstPlayer *player;
  gint percent;
} BufferingSignalData;

static gboolean
buffering_dispatch (gpointer user_data)
{
  BufferingSignalData *data = user_data;

  g_signal_emit (data->player, signals[SIGNAL_BUFFERING], 0, data->percent);

  return G_SOURCE_REMOVE;
}

static void
buffering_cb (GstBus * bus, GstMessage * msg, gpointer user_data)
{
  GstPlayer *self = GST_PLAYER (user_data);
  gint percent;

  if (self->is_live)
    return;

  gst_message_parse_buffering (msg, &percent);
  GST_LOG_OBJECT (self, "Buffering %d%%", percent);

  if (percent < 100 && self->target_state >= GST_STATE_PAUSED) {
    GstStateChangeReturn state_ret;

    GST_DEBUG_OBJECT (self, "Waiting for buffering to finish");
    state_ret = gst_element_set_state (self->playbin, GST_STATE_PAUSED);

    if (state_ret == GST_STATE_CHANGE_FAILURE) {
      emit_error (self, g_error_new (GST_PLAYER_ERROR, GST_PLAYER_ERROR_FAILED,
              "Failed to handle buffering"));
      return;
    }

    change_state (self, GST_PLAYER_STATE_BUFFERING);
  }

  if (self->buffering != percent) {
    if (self->dispatch_to_main_context
        && g_signal_handler_find (self, G_SIGNAL_MATCH_ID,
            signals[SIGNAL_BUFFERING], 0, NULL, NULL, NULL) != 0) {
      BufferingSignalData *data = g_new (BufferingSignalData, 1);

      data->player = self;
      data->percent = percent;
      g_main_context_invoke_full (self->application_context,
          G_PRIORITY_DEFAULT, buffering_dispatch, data,
          (GDestroyNotify) g_free);
    } else {
      g_signal_emit (self, signals[SIGNAL_BUFFERING], 0, percent);
    }

    self->buffering = percent;
  }


  g_mutex_lock (&self->lock);
  if (percent == 100 && (self->seek_position != GST_CLOCK_TIME_NONE ||
          self->seek_pending)) {
    g_mutex_unlock (&self->lock);

    GST_DEBUG_OBJECT (self, "Buffering finished - seek pending");
  } else if (percent == 100 && self->target_state >= GST_STATE_PLAYING
      && self->current_state >= GST_STATE_PAUSED) {
    GstStateChangeReturn state_ret;

    g_mutex_unlock (&self->lock);

    GST_DEBUG_OBJECT (self, "Buffering finished - going to PLAYING");
    state_ret = gst_element_set_state (self->playbin, GST_STATE_PLAYING);
    /* Application state change is happening when the state change happened */
    if (state_ret == GST_STATE_CHANGE_FAILURE)
      emit_error (self, g_error_new (GST_PLAYER_ERROR, GST_PLAYER_ERROR_FAILED,
              "Failed to handle buffering"));
  } else if (percent == 100 && self->target_state >= GST_STATE_PAUSED) {
    g_mutex_unlock (&self->lock);

    GST_DEBUG_OBJECT (self, "Buffering finished - staying PAUSED");
    change_state (self, GST_PLAYER_STATE_PAUSED);
  } else {
    g_mutex_unlock (&self->lock);
  }
}

static void
clock_lost_cb (GstBus * bus, GstMessage * msg, gpointer user_data)
{
  GstPlayer *self = GST_PLAYER (user_data);
  GstStateChangeReturn state_ret;

  GST_DEBUG_OBJECT (self, "Clock lost");
  if (self->target_state >= GST_STATE_PLAYING) {
    state_ret = gst_element_set_state (self->playbin, GST_STATE_PAUSED);
    if (state_ret != GST_STATE_CHANGE_FAILURE)
      state_ret = gst_element_set_state (self->playbin, GST_STATE_PLAYING);

    if (state_ret == GST_STATE_CHANGE_FAILURE)
      emit_error (self, g_error_new (GST_PLAYER_ERROR, GST_PLAYER_ERROR_FAILED,
              "Failed to handle clock loss"));
  }
}

typedef struct
{
  GstPlayer *player;
  gint width, height;
} VideoDimensionsChangedSignalData;

static gboolean
video_dimensions_changed_dispatch (gpointer user_data)
{
  VideoDimensionsChangedSignalData *data = user_data;

  g_signal_emit (data->player, signals[SIGNAL_VIDEO_DIMENSIONS_CHANGED], 0,
      data->width, data->height);

  return G_SOURCE_REMOVE;
}

static void
check_video_dimensions_changed (GstPlayer * self)
{
  GstElement *video_sink;
  GstPad *video_sink_pad;
  GstCaps *caps;
  GstVideoInfo info;
  gint width = 0, height = 0;

  g_object_get (self->playbin, "video-sink", &video_sink, NULL);
  if (!video_sink)
    goto out;

  video_sink_pad = gst_element_get_static_pad (video_sink, "sink");
  if (!video_sink_pad) {
    gst_object_unref (video_sink);
    goto out;
  }

  caps = gst_pad_get_current_caps (video_sink_pad);

  if (caps) {
    if (gst_video_info_from_caps (&info, caps)) {
      info.width = info.width * info.par_n / info.par_d;

      GST_DEBUG_OBJECT (self, "Video dimensions changed: %dx%d", info.width,
          info.height);
      width = info.width;
      height = info.height;
    }

    gst_caps_unref (caps);
  }
  gst_object_unref (video_sink_pad);
  gst_object_unref (video_sink);

out:
  if (self->dispatch_to_main_context
      && g_signal_handler_find (self, G_SIGNAL_MATCH_ID,
          signals[SIGNAL_VIDEO_DIMENSIONS_CHANGED], 0, NULL, NULL, NULL) != 0) {
    VideoDimensionsChangedSignalData *data =
        g_new (VideoDimensionsChangedSignalData, 1);

    data->player = self;
    data->width = width;
    data->height = height;
    g_main_context_invoke_full (self->application_context,
        G_PRIORITY_DEFAULT, video_dimensions_changed_dispatch, data,
        (GDestroyNotify) g_free);
  } else {
    g_signal_emit (self, signals[SIGNAL_VIDEO_DIMENSIONS_CHANGED], 0,
        width, height);
  }
}

static void
notify_caps_cb (GObject * object, GParamSpec * pspec, gpointer user_data)
{
  GstPlayer *self = GST_PLAYER (user_data);

  check_video_dimensions_changed (self);
}

typedef struct
{
  GstPlayer *player;
  GstClockTime duration;
} DurationChangedSignalData;

static gboolean
duration_changed_dispatch (gpointer user_data)
{
  DurationChangedSignalData *data = user_data;

  g_signal_emit (data->player, signals[SIGNAL_DURATION_CHANGED], 0,
      data->duration);
  g_object_notify_by_pspec (G_OBJECT (data->player),
      param_specs[PROP_DURATION]);

  return G_SOURCE_REMOVE;
}

static void
emit_duration_changed (GstPlayer * self, GstClockTime duration)
{
  GST_DEBUG_OBJECT (self, "Duration changed %" GST_TIME_FORMAT,
      GST_TIME_ARGS (duration));

  if (self->dispatch_to_main_context
      && g_signal_handler_find (self, G_SIGNAL_MATCH_ID,
          signals[SIGNAL_DURATION_CHANGED], 0, NULL, NULL, NULL) != 0) {
    DurationChangedSignalData *data = g_new (DurationChangedSignalData, 1);

    data->player = self;
    data->duration = duration;
    g_main_context_invoke_full (self->application_context,
        G_PRIORITY_DEFAULT, duration_changed_dispatch, data,
        (GDestroyNotify) g_free);
  } else {
    g_signal_emit (self, signals[SIGNAL_DURATION_CHANGED], 0, duration);
    g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_DURATION]);
  }
}

static void
state_changed_cb (GstBus * bus, GstMessage * msg, gpointer user_data)
{
  GstPlayer *self = GST_PLAYER (user_data);
  GstState old_state, new_state, pending_state;

  gst_message_parse_state_changed (msg, &old_state, &new_state, &pending_state);

  if (GST_MESSAGE_SRC (msg) == GST_OBJECT (self->playbin)) {
    gchar *transition_name;

    GST_DEBUG_OBJECT (self, "Changed state old: %s new: %s pending: %s",
        gst_element_state_get_name (old_state),
        gst_element_state_get_name (new_state),
        gst_element_state_get_name (pending_state));

    transition_name = g_strdup_printf ("%s_%s",
        gst_element_state_get_name (old_state),
        gst_element_state_get_name (new_state));
    dump_dot_file (self, transition_name);
    g_free (transition_name);

    self->current_state = new_state;

    if (old_state == GST_STATE_READY && new_state == GST_STATE_PAUSED
        && pending_state == GST_STATE_VOID_PENDING) {
      GstElement *video_sink;
      GstPad *video_sink_pad;
      gint64 duration = -1;

      GST_DEBUG_OBJECT (self, "Initial PAUSED - pre-rolled");

      g_mutex_lock (&self->lock);
      if (self->media_info)
        g_object_unref (self->media_info);
      self->media_info = gst_player_media_info_create (self);
      g_mutex_unlock (&self->lock);
      emit_media_info_updated_signal (self);

      g_object_get (self->playbin, "video-sink", &video_sink, NULL);

      if (video_sink) {
        video_sink_pad = gst_element_get_static_pad (video_sink, "sink");

        if (video_sink_pad) {
          g_signal_connect (video_sink_pad, "notify::caps",
              (GCallback) notify_caps_cb, self);
          gst_object_unref (video_sink_pad);
        }
        gst_object_unref (video_sink);
      }

      check_video_dimensions_changed (self);
      gst_element_query_duration (self->playbin, GST_FORMAT_TIME, &duration);
      emit_duration_changed (self, duration);
    }

    if (new_state == GST_STATE_PAUSED
        && pending_state == GST_STATE_VOID_PENDING) {
      remove_tick_source (self);

      g_mutex_lock (&self->lock);
      if (self->seek_pending) {
        self->seek_pending = FALSE;

        if (!self->media_info->seekable) {
          GST_DEBUG_OBJECT (self, "Media is not seekable");
          if (self->seek_source) {
            g_source_destroy (self->seek_source);
            g_source_unref (self->seek_source);
            self->seek_source = NULL;
          }
          self->seek_position = GST_CLOCK_TIME_NONE;
          self->last_seek_time = GST_CLOCK_TIME_NONE;
        } else if (self->seek_source) {
          GST_DEBUG_OBJECT (self, "Seek finished but new seek is pending");
          gst_player_seek_internal_locked (self);
        } else {
          GST_DEBUG_OBJECT (self, "Seek finished");
        }
      }

      if (self->seek_position != GST_CLOCK_TIME_NONE) {
        GST_DEBUG_OBJECT (self, "Seeking now that we reached PAUSED state");
        gst_player_seek_internal_locked (self);
        g_mutex_unlock (&self->lock);
      } else if (!self->seek_pending) {
        g_mutex_unlock (&self->lock);

        tick_cb (self);

        if (self->target_state >= GST_STATE_PLAYING && self->buffering == 100) {
          GstStateChangeReturn state_ret;

          state_ret = gst_element_set_state (self->playbin, GST_STATE_PLAYING);
          if (state_ret == GST_STATE_CHANGE_FAILURE)
            emit_error (self, g_error_new (GST_PLAYER_ERROR,
                    GST_PLAYER_ERROR_FAILED, "Failed to play"));
        } else if (self->buffering == 100) {
          change_state (self, GST_PLAYER_STATE_PAUSED);
        }
      } else {
        g_mutex_unlock (&self->lock);
      }
    } else if (new_state == GST_STATE_PLAYING
        && pending_state == GST_STATE_VOID_PENDING) {

      /* If no seek is currently pending, add the tick source. This can happen
       * if we seeked already but the state-change message was still queued up */
      if (!self->seek_pending) {
        add_tick_source (self);
        change_state (self, GST_PLAYER_STATE_PLAYING);
      }
    } else if (new_state == GST_STATE_READY && old_state > GST_STATE_READY) {
      change_state (self, GST_PLAYER_STATE_STOPPED);
    } else {
      /* Otherwise we neither reached PLAYING nor PAUSED, so must
       * wait for something to happen... i.e. are BUFFERING now */
      change_state (self, GST_PLAYER_STATE_BUFFERING);
    }
  }
}

static void
duration_changed_cb (GstBus * bus, GstMessage * msg, gpointer user_data)
{
  GstPlayer *self = GST_PLAYER (user_data);
  gint64 duration;

  if (gst_element_query_duration (self->playbin, GST_FORMAT_TIME, &duration)) {
    emit_duration_changed (self, duration);
  }
}

static void
latency_cb (GstBus * bus, GstMessage * msg, gpointer user_data)
{
  GstPlayer *self = GST_PLAYER (user_data);

  GST_DEBUG_OBJECT (self, "Latency changed");

  gst_bin_recalculate_latency (GST_BIN (self->playbin));
}

static void
request_state_cb (GstBus * bus, GstMessage * msg, gpointer user_data)
{
  GstPlayer *self = GST_PLAYER (user_data);
  GstState state;
  GstStateChangeReturn state_ret;

  gst_message_parse_request_state (msg, &state);

  GST_DEBUG_OBJECT (self, "State %s requested",
      gst_element_state_get_name (state));

  self->target_state = state;
  state_ret = gst_element_set_state (self->playbin, state);
  if (state_ret == GST_STATE_CHANGE_FAILURE)
    emit_error (self, g_error_new (GST_PLAYER_ERROR, GST_PLAYER_ERROR_FAILED,
            "Failed to change to requested state %s",
            gst_element_state_get_name (state)));
}

static void
media_info_update (GstPlayer * self, GstPlayerMediaInfo * info)
{
  if (info->title)
    g_free (info->title);
  info->title = get_from_tags (self, info, get_title);

  if (info->container)
    g_free (info->container);
  info->container = get_from_tags (self, info, get_container_format);

  if (info->image_sample)
    gst_sample_unref (info->image_sample);
  info->image_sample = get_from_tags (self, info, get_cover_sample);

  GST_DEBUG_OBJECT (self, "title: %s, container: %s "
      "image_sample: %p", info->title, info->container, info->image_sample);
}

static void
tags_cb (GstBus * bus, GstMessage * msg, gpointer user_data)
{
  GstPlayer *self = GST_PLAYER (user_data);
  GstTagList *tags = NULL;

  gst_message_parse_tag (msg, &tags);

  /*
   * NOTE: Inorder to get global tag you must apply the following patches in
   * your gstreamer build.
   *
   * http://cgit.freedesktop.org/gstreamer/gst-plugins-good/commit/?id=9119fbd774093e3ae762c8652acd80d54b2c3b45
   * http://cgit.freedesktop.org/gstreamer/gstreamer/commit/?id=18b058100940bdcaed86fa412e3582a02871f995
   */
  GST_DEBUG_OBJECT (self, "recieved %s tags",
      gst_tag_list_get_scope (tags) ==
      GST_TAG_SCOPE_GLOBAL ? "global" : "stream");

  if (gst_tag_list_get_scope (tags) == GST_TAG_SCOPE_GLOBAL) {
    g_mutex_lock (&self->lock);
    if (self->media_info) {
      if (self->media_info->tags)
        gst_tag_list_unref (self->media_info->tags);
      self->media_info->tags = gst_tag_list_ref (tags);
      media_info_update (self, self->media_info);
      g_mutex_unlock (&self->lock);
      emit_media_info_updated_signal (self);
    } else {
      if (self->global_tags)
        gst_tag_list_unref (self->global_tags);
      self->global_tags = gst_tag_list_ref (tags);
      g_mutex_unlock (&self->lock);
    }
  }

  gst_tag_list_unref (tags);
}

static void
element_cb (GstBus * bus, GstMessage * msg, gpointer user_data)
{
  GstPlayer *self = GST_PLAYER (user_data);
  const GstStructure *s;

  s = gst_message_get_structure (msg);
  if (gst_structure_has_name (s, "redirect")) {
    const gchar *new_location;

    new_location = gst_structure_get_string (s, "new-location");
    if (!new_location) {
      const GValue *locations_list, *location_val;
      guint i, size;

      locations_list = gst_structure_get_value (s, "locations");
      size = gst_value_list_get_size (locations_list);
      for (i = 0; i < size; ++i) {
        const GstStructure *location_s;

        location_val = gst_value_list_get_value (locations_list, i);
        if (!GST_VALUE_HOLDS_STRUCTURE (location_val))
          continue;

        location_s = (const GstStructure *) g_value_get_boxed (location_val);
        if (!gst_structure_has_name (location_s, "redirect"))
          continue;

        new_location = gst_structure_get_string (location_s, "new-location");
        if (new_location)
          break;
      }
    }

    if (new_location) {
      GstState target_state;

      GST_DEBUG_OBJECT (self, "Redirect to '%s'", new_location);

      /* Remember target state and restore after setting the URI */
      target_state = self->target_state;

      g_mutex_lock (&self->lock);
      if (self->uri)
        g_free (self->uri);

      self->uri = g_strdup (new_location);
      g_mutex_unlock (&self->lock);

      gst_player_set_uri_internal (self);

      if (target_state == GST_STATE_PAUSED)
        gst_player_pause_internal (self);
      else if (target_state == GST_STATE_PLAYING)
        gst_player_play_internal (self);
    }
  }
}

static void
player_set_flag (GstPlayer * self, gint pos)
{
  gint flags;

  g_object_get (self->playbin, "flags", &flags, NULL);
  flags |= pos;
  g_object_set (self->playbin, "flags", flags, NULL);

  GST_DEBUG_OBJECT (self, "setting flags=%#x", flags);
}

static void
player_clear_flag (GstPlayer * self, gint pos)
{
  gint flags;

  g_object_get (self->playbin, "flags", &flags, NULL);
  flags &= ~pos;
  g_object_set (self->playbin, "flags", flags, NULL);

  GST_DEBUG_OBJECT (self, "setting flags=%#x", flags);
}

typedef struct
{
  GstPlayer *player;
  GstPlayerMediaInfo *info;
} MediaInfoUpdatedSignalData;

static gboolean
media_info_updated_dispatch (gpointer user_data)
{
  MediaInfoUpdatedSignalData *data = user_data;

  g_signal_emit (data->player, signals[SIGNAL_MEDIA_INFO_UPDATED], 0,
      data->info);

  return FALSE;
}

static void
free_media_info_updated_signal_data (MediaInfoUpdatedSignalData * data)
{
  g_object_unref (data->info);
  g_free (data);
}

/*
 * emit_media_info_updated_signal:
 *
 * create a new copy of self->media_info object and emits the newly created
 * copy to user application. The newly created media_info will be unref'ed
 * as part of signal finalize method.
 */
static void
emit_media_info_updated_signal (GstPlayer * self)
{
  if (self->dispatch_to_main_context) {
    MediaInfoUpdatedSignalData *data = g_new (MediaInfoUpdatedSignalData, 1);
    data->player = self;
    g_mutex_lock (&self->lock);
    data->info = gst_player_media_info_copy (self->media_info);
    g_mutex_unlock (&self->lock);

    g_main_context_invoke_full (self->application_context,
        G_PRIORITY_DEFAULT, media_info_updated_dispatch,
        data, (GDestroyNotify) free_media_info_updated_signal_data);
  } else {
    GstPlayerMediaInfo *info;

    g_mutex_lock (&self->lock);
    info = gst_player_media_info_copy (self->media_info);
    g_mutex_unlock (&self->lock);

    g_signal_emit (self, signals[SIGNAL_MEDIA_INFO_UPDATED], 0, info);
    g_object_unref (info);
  }
}

static GstCaps *
get_caps (GstPlayer * self, gint stream_index, GType type)
{
  GstPad *pad = NULL;
  GstCaps *caps = NULL;

  if (type == GST_TYPE_PLAYER_VIDEO_INFO)
    g_signal_emit_by_name (G_OBJECT (self->playbin),
        "get-video-pad", stream_index, &pad);
  else if (type == GST_TYPE_PLAYER_AUDIO_INFO)
    g_signal_emit_by_name (G_OBJECT (self->playbin),
        "get-audio-pad", stream_index, &pad);
  else
    g_signal_emit_by_name (G_OBJECT (self->playbin),
        "get-text-pad", stream_index, &pad);

  if (pad) {
    caps = gst_pad_get_current_caps (pad);
    gst_object_unref (pad);
  }

  return caps;
}

static void
gst_player_subtitle_info_update (GstPlayer * self,
    GstPlayerStreamInfo * stream_info)
{
  GstPlayerSubtitleInfo *info = (GstPlayerSubtitleInfo *) stream_info;

  if (stream_info->tags) {

    /* free the old language info */
    if (info->language) {
      g_free (info->language);
      info->language = NULL;
    }

    /* First try to get the language full name from tag, if name is not
     * available then try language code. If we find the language code
     * then use gstreamer api to translate code to full name.
     */
    gst_tag_list_get_string (stream_info->tags, GST_TAG_LANGUAGE_NAME,
        &info->language);
    if (!info->language) {
      gchar *lang_code = NULL;

      gst_tag_list_get_string (stream_info->tags, GST_TAG_LANGUAGE_CODE,
          &lang_code);
      if (lang_code) {
        info->language = g_strdup (gst_tag_get_language_name (lang_code));
        g_free (lang_code);
      }
    }
  } else {
    if (info->language) {
      g_free (info->language);
      info->language = NULL;
    }
  }

  GST_DEBUG_OBJECT (self, "language=%s", info->language);
}

static void
gst_player_video_info_update (GstPlayer * self,
    GstPlayerStreamInfo * stream_info)
{
  GstPlayerVideoInfo *info = (GstPlayerVideoInfo *) stream_info;

  if (stream_info->caps) {
    GstStructure *s;

    s = gst_caps_get_structure (stream_info->caps, 0);
    if (s) {
      gint width, height;
      gint fps_n, fps_d;
      gint par_n, par_d;

      if (gst_structure_get_int (s, "width", &width))
        info->width = width;
      else
        info->width = -1;

      if (gst_structure_get_int (s, "height", &height))
        info->height = height;
      else
        info->height = -1;

      if (gst_structure_get_fraction (s, "framerate", &fps_n, &fps_d)) {
        info->framerate_num = fps_n;
        info->framerate_denom = fps_d;
      } else {
        info->framerate_num = 0;
        info->framerate_denom = 1;
      }


      if (gst_structure_get_fraction (s, "pixel-aspect-ratio", &par_n, &par_d)) {
        info->par_num = par_n;
        info->par_denom = par_d;
      } else {
        info->par_num = 1;
        info->par_denom = 1;
      }
    }
  } else {
    info->width = info->height = -1;
    info->par_num = info->par_denom = 1;
    info->framerate_num = 0;
    info->framerate_denom = 1;
  }

  if (stream_info->tags) {
    guint bitrate, max_bitrate;

    if (gst_tag_list_get_uint (stream_info->tags, GST_TAG_BITRATE, &bitrate))
      info->bitrate = bitrate;
    else
      info->bitrate = -1;

    if (gst_tag_list_get_uint (stream_info->tags, GST_TAG_MAXIMUM_BITRATE,
            &max_bitrate) || gst_tag_list_get_uint (stream_info->tags,
            GST_TAG_NOMINAL_BITRATE, &max_bitrate))
      info->max_bitrate = max_bitrate;
    else
      info->max_bitrate = -1;
  } else {
    info->bitrate = info->max_bitrate = -1;
  }

  GST_DEBUG_OBJECT (self, "width=%d height=%d fps=%.2f par=%d:%d "
      "bitrate=%d max_bitrate=%d", info->width, info->height,
      (gdouble) info->framerate_num / info->framerate_denom,
      info->par_num, info->par_denom, info->bitrate, info->max_bitrate);
}

static void
gst_player_audio_info_update (GstPlayer * self,
    GstPlayerStreamInfo * stream_info)
{
  GstPlayerAudioInfo *info = (GstPlayerAudioInfo *) stream_info;

  if (stream_info->caps) {
    GstStructure *s;

    s = gst_caps_get_structure (stream_info->caps, 0);
    if (s) {
      gint rate, channels;

      if (gst_structure_get_int (s, "rate", &rate))
        info->sample_rate = rate;
      else
        info->sample_rate = -1;

      if (gst_structure_get_int (s, "channels", &channels))
        info->channels = channels;
      else
        info->channels = 0;
    }
  } else {
    info->sample_rate = -1;
    info->channels = 0;
  }

  if (stream_info->tags) {
    guint bitrate, max_bitrate;

    if (gst_tag_list_get_uint (stream_info->tags, GST_TAG_BITRATE, &bitrate))
      info->bitrate = bitrate;
    else
      info->bitrate = -1;

    if (gst_tag_list_get_uint (stream_info->tags, GST_TAG_MAXIMUM_BITRATE,
            &max_bitrate) || gst_tag_list_get_uint (stream_info->tags,
            GST_TAG_NOMINAL_BITRATE, &max_bitrate))
      info->max_bitrate = max_bitrate;
    else
      info->max_bitrate = -1;

    /* if we have old language the free it */
    if (info->language) {
      g_free (info->language);
      info->language = NULL;
    }

    /* First try to get the language full name from tag, if name is not
     * available then try language code. If we find the language code
     * then use gstreamer api to translate code to full name.
     */
    gst_tag_list_get_string (stream_info->tags, GST_TAG_LANGUAGE_NAME,
        &info->language);
    if (!info->language) {
      gchar *lang_code = NULL;

      gst_tag_list_get_string (stream_info->tags, GST_TAG_LANGUAGE_CODE,
          &lang_code);
      if (lang_code) {
        info->language = g_strdup (gst_tag_get_language_name (lang_code));
        g_free (lang_code);
      }
    }
  } else {
    if (info->language) {
      g_free (info->language);
      info->language = NULL;
    }
    info->max_bitrate = info->bitrate = -1;
  }

  GST_DEBUG_OBJECT (self, "language=%s rate=%d channels=%d bitrate=%d "
      "max_bitrate=%d", info->language, info->sample_rate, info->channels,
      info->bitrate, info->bitrate);
}

static GstPlayerStreamInfo *
gst_player_stream_info_find (GstPlayer * self, GstPlayerMediaInfo * media_info,
    GType type, gint stream_index)
{
  GList *list, *l;
  GstPlayerStreamInfo *info = NULL;

  if (!media_info)
    return NULL;

  list = gst_player_media_info_get_stream_list (media_info);
  for (l = list; l != NULL; l = l->next) {
    info = (GstPlayerStreamInfo *) l->data;
    if ((G_OBJECT_TYPE (info) == type) && (info->stream_index == stream_index)) {
      return info;
    }
  }

  return NULL;
}

static gboolean
is_track_enabled (GstPlayer * self, gint pos)
{
  gint flags;

  g_object_get (G_OBJECT (self->playbin), "flags", &flags, NULL);

  if ((flags & pos))
    return TRUE;

  return FALSE;
}

static GstPlayerStreamInfo *
gst_player_stream_info_get_current (GstPlayer * self, const gchar * prop,
    GType type)
{
  gint current;
  GstPlayerStreamInfo *info;

  if (!self->media_info)
    return NULL;

  g_object_get (G_OBJECT (self->playbin), prop, &current, NULL);
  g_mutex_lock (&self->lock);
  info = gst_player_stream_info_find (self, self->media_info, type, current);
  if (info)
    info = gst_player_stream_info_copy (info);
  g_mutex_unlock (&self->lock);

  return info;
}

static void
gst_player_stream_info_update (GstPlayer * self, GstPlayerStreamInfo * s)
{
  if (GST_IS_PLAYER_VIDEO_INFO (s))
    gst_player_video_info_update (self, s);
  else if (GST_IS_PLAYER_AUDIO_INFO (s))
    gst_player_audio_info_update (self, s);
  else
    gst_player_subtitle_info_update (self, s);
}

static gchar *
stream_info_get_codec (GstPlayerStreamInfo * s)
{
  const gchar *type;
  GstTagList *tags;
  gchar *codec = NULL;

  if (GST_IS_PLAYER_VIDEO_INFO (s))
    type = GST_TAG_VIDEO_CODEC;
  else if (GST_IS_PLAYER_AUDIO_INFO (s))
    type = GST_TAG_AUDIO_CODEC;
  else
    type = GST_TAG_SUBTITLE_CODEC;

  tags = gst_player_stream_info_get_tags (s);
  if (tags) {
    gst_tag_list_get_string (tags, type, &codec);
    if (!codec)
      gst_tag_list_get_string (tags, GST_TAG_CODEC, &codec);
  }

  if (!codec) {
    GstCaps *caps;
    caps = gst_player_stream_info_get_caps (s);
    if (caps) {
      codec = gst_pb_utils_get_codec_description (caps);
    }
  }

  return codec;
}

static void
gst_player_stream_info_update_tags_and_caps (GstPlayer * self,
    GstPlayerStreamInfo * s)
{
  GstTagList *tags;
  gint stream_index;

  stream_index = gst_player_stream_info_get_index (s);

  if (GST_IS_PLAYER_VIDEO_INFO (s))
    g_signal_emit_by_name (self->playbin, "get-video-tags",
        stream_index, &tags);
  else if (GST_IS_PLAYER_AUDIO_INFO (s))
    g_signal_emit_by_name (self->playbin, "get-audio-tags",
        stream_index, &tags);
  else
    g_signal_emit_by_name (self->playbin, "get-text-tags", stream_index, &tags);

  if (s->tags)
    gst_tag_list_unref (s->tags);
  s->tags = tags;

  if (s->caps)
    gst_caps_unref (s->caps);
  s->caps = get_caps (self, stream_index, G_OBJECT_TYPE (s));

  if (s->codec)
    g_free (s->codec);
  s->codec = stream_info_get_codec (s);

  GST_DEBUG_OBJECT (self, "%s index: %d tags: %p caps: %p",
      gst_player_stream_info_get_stream_type (s), stream_index,
      s->tags, s->caps);

  gst_player_stream_info_update (self, s);
}

static void
gst_player_streams_info_create (GstPlayer * self,
    GstPlayerMediaInfo * media_info, const gchar * prop, GType type)
{
  gint i;
  gint total = -1;
  GstPlayerStreamInfo *s;

  if (!media_info)
    return;

  g_object_get (G_OBJECT (self->playbin), prop, &total, NULL);

  GST_DEBUG_OBJECT (self, "%s: %d", prop, total);

  for (i = 0; i < total; i++) {
    /* check if stream already exist in the list */
    s = gst_player_stream_info_find (self, media_info, type, i);

    if (!s) {
      /* create a new stream info instance */
      s = gst_player_stream_info_new (i, type);

      /* add the object in stream list */
      media_info->stream_list = g_list_append (media_info->stream_list, s);

      /* based on type, add the object in its corresponding stream_ list */
      if (GST_IS_PLAYER_AUDIO_INFO (s))
        media_info->audio_stream_list = g_list_append
            (media_info->audio_stream_list, s);
      else if (GST_IS_PLAYER_VIDEO_INFO (s))
        media_info->video_stream_list = g_list_append
            (media_info->video_stream_list, s);
      else
        media_info->subtitle_stream_list = g_list_append
            (media_info->subtitle_stream_list, s);

      GST_DEBUG_OBJECT (self, "create %s stream stream_index: %d",
          gst_player_stream_info_get_stream_type (s), i);
    }

    gst_player_stream_info_update_tags_and_caps (self, s);
  }
}

static void
video_changed_cb (GObject * object, gpointer user_data)
{
  GstPlayer *self = GST_PLAYER (user_data);

  g_mutex_lock (&self->lock);
  gst_player_streams_info_create (self, self->media_info,
      "n-video", GST_TYPE_PLAYER_VIDEO_INFO);
  g_mutex_unlock (&self->lock);
}

static void
audio_changed_cb (GObject * object, gpointer user_data)
{
  GstPlayer *self = GST_PLAYER (user_data);

  g_mutex_lock (&self->lock);
  gst_player_streams_info_create (self, self->media_info,
      "n-audio", GST_TYPE_PLAYER_AUDIO_INFO);
  g_mutex_unlock (&self->lock);
}

static void
subtitle_changed_cb (GObject * object, gpointer user_data)
{
  GstPlayer *self = GST_PLAYER (user_data);

  g_mutex_lock (&self->lock);
  gst_player_streams_info_create (self, self->media_info,
      "n-text", GST_TYPE_PLAYER_SUBTITLE_INFO);
  g_mutex_unlock (&self->lock);
}

static void *
get_title (GstTagList * tags)
{
  gchar *title = NULL;

  gst_tag_list_get_string (tags, GST_TAG_TITLE, &title);
  if (!title)
    gst_tag_list_get_string (tags, GST_TAG_TITLE_SORTNAME, &title);

  return title;
}

static void *
get_container_format (GstTagList * tags)
{
  gchar *container = NULL;

  gst_tag_list_get_string (tags, GST_TAG_CONTAINER_FORMAT, &container);

  /* TODO: If container is not available then maybe consider
   * parsing caps or file extension to guess the container format.
   */

  return container;
}

static void *
get_from_tags (GstPlayer * self, GstPlayerMediaInfo * media_info,
    void *(*func) (GstTagList *))
{
  GList *l;
  void *ret = NULL;

  if (media_info->tags) {
    ret = func (media_info->tags);
    if (ret)
      return ret;
  }

  /* if global tag does not exit then try video and audio streams */
  GST_DEBUG_OBJECT (self, "trying video tags");
  for (l = gst_player_get_video_streams (media_info); l != NULL; l = l->next) {
    GstTagList *tags;

    tags = gst_player_stream_info_get_tags ((GstPlayerStreamInfo *) l->data);
    if (tags)
      ret = func (tags);

    if (ret)
      return ret;
  }

  GST_DEBUG_OBJECT (self, "trying audio tags");
  for (l = gst_player_get_audio_streams (media_info); l != NULL; l = l->next) {
    GstTagList *tags;

    tags = gst_player_stream_info_get_tags ((GstPlayerStreamInfo *) l->data);
    if (tags)
      ret = func (tags);

    if (ret)
      return ret;
  }

  GST_DEBUG_OBJECT (self, "failed to get the information from tags");
  return NULL;
}

static void *
get_cover_sample (GstTagList * tags)
{
  GstSample *cover_sample = NULL;

  gst_tag_list_get_sample (tags, GST_TAG_IMAGE, &cover_sample);
  if (!cover_sample)
    gst_tag_list_get_sample (tags, GST_TAG_PREVIEW_IMAGE, &cover_sample);

  return cover_sample;
}

static GstPlayerMediaInfo *
gst_player_media_info_create (GstPlayer * self)
{
  GstPlayerMediaInfo *media_info;
  GstQuery *query;

  GST_DEBUG_OBJECT (self, "begin");
  media_info = gst_player_media_info_new (self->uri);
  media_info->duration = gst_player_get_duration (self);
  media_info->tags = self->global_tags;
  self->global_tags = NULL;

  query = gst_query_new_seeking (GST_FORMAT_TIME);
  if (gst_element_query (self->playbin, query))
    gst_query_parse_seeking (query, NULL, &media_info->seekable, NULL, NULL);
  gst_query_unref (query);

  /* create audio/video/sub streams */
  gst_player_streams_info_create (self, media_info, "n-video",
      GST_TYPE_PLAYER_VIDEO_INFO);
  gst_player_streams_info_create (self, media_info, "n-audio",
      GST_TYPE_PLAYER_AUDIO_INFO);
  gst_player_streams_info_create (self, media_info, "n-text",
      GST_TYPE_PLAYER_SUBTITLE_INFO);

  media_info->title = get_from_tags (self, media_info, get_title);
  media_info->container =
      get_from_tags (self, media_info, get_container_format);
  media_info->image_sample = get_from_tags (self, media_info, get_cover_sample);

  GST_DEBUG_OBJECT (self, "uri: %s title: %s duration: %" GST_TIME_FORMAT
      " seekable: %s container: %s image_sample %p",
      media_info->uri, media_info->title, GST_TIME_ARGS (media_info->duration),
      media_info->seekable ? "yes" : "no", media_info->container,
      media_info->image_sample);

  GST_DEBUG_OBJECT (self, "end");
  return media_info;
}

static void
tags_changed_cb (GstPlayer * self, gint stream_index, GType type)
{
  GstPlayerStreamInfo *s;

  if (!self->media_info)
    return;

  /* update the stream information */
  g_mutex_lock (&self->lock);
  s = gst_player_stream_info_find (self, self->media_info, type, stream_index);
  gst_player_stream_info_update_tags_and_caps (self, s);
  g_mutex_unlock (&self->lock);

  emit_media_info_updated_signal (self);
}

static void
video_tags_changed_cb (GstElement * playbin, gint stream_index,
    gpointer user_data)
{
  tags_changed_cb (GST_PLAYER (user_data), stream_index,
      GST_TYPE_PLAYER_VIDEO_INFO);
}

static void
audio_tags_changed_cb (GstElement * playbin, gint stream_index,
    gpointer user_data)
{
  tags_changed_cb (GST_PLAYER (user_data), stream_index,
      GST_TYPE_PLAYER_AUDIO_INFO);
}

static void
subtitle_tags_changed_cb (GstElement * playbin, gint stream_index,
    gpointer user_data)
{
  tags_changed_cb (GST_PLAYER (user_data), stream_index,
      GST_TYPE_PLAYER_SUBTITLE_INFO);
}

static gpointer
gst_player_main (gpointer data)
{
  GstPlayer *self = GST_PLAYER (data);
  GstBus *bus;
  GSource *source;
  GSource *bus_source;

  GST_TRACE_OBJECT (self, "Starting main thread");

  self->context = g_main_context_new ();
  g_main_context_push_thread_default (self->context);

  self->loop = g_main_loop_new (self->context, FALSE);

  source = g_idle_source_new ();
  g_source_set_callback (source, (GSourceFunc) main_loop_running_cb, self,
      NULL);
  g_source_attach (source, self->context);
  g_source_unref (source);

  self->playbin = gst_element_factory_make ("playbin", "playbin");

  self->bus = bus = gst_element_get_bus (self->playbin);
  bus_source = gst_bus_create_watch (bus);
  g_source_set_callback (bus_source, (GSourceFunc) gst_bus_async_signal_func,
      NULL, NULL);
  g_source_attach (bus_source, self->context);

  g_signal_connect (G_OBJECT (bus), "message::error", G_CALLBACK (error_cb),
      self);
  g_signal_connect (G_OBJECT (bus), "message::warning", G_CALLBACK (warning_cb),
      self);
  g_signal_connect (G_OBJECT (bus), "message::eos", G_CALLBACK (eos_cb), self);
  g_signal_connect (G_OBJECT (bus), "message::state-changed",
      G_CALLBACK (state_changed_cb), self);
  g_signal_connect (G_OBJECT (bus), "message::buffering",
      G_CALLBACK (buffering_cb), self);
  g_signal_connect (G_OBJECT (bus), "message::clock-lost",
      G_CALLBACK (clock_lost_cb), self);
  g_signal_connect (G_OBJECT (bus), "message::duration-changed",
      G_CALLBACK (duration_changed_cb), self);
  g_signal_connect (G_OBJECT (bus), "message::latency",
      G_CALLBACK (latency_cb), self);
  g_signal_connect (G_OBJECT (bus), "message::request-state",
      G_CALLBACK (request_state_cb), self);
  g_signal_connect (G_OBJECT (bus), "message::element",
      G_CALLBACK (element_cb), self);
  g_signal_connect (G_OBJECT (bus), "message::tag", G_CALLBACK (tags_cb), self);

  g_signal_connect (self->playbin, "video-changed",
      G_CALLBACK (video_changed_cb), self);
  g_signal_connect (self->playbin, "audio-changed",
      G_CALLBACK (audio_changed_cb), self);
  g_signal_connect (self->playbin, "text-changed",
      G_CALLBACK (subtitle_changed_cb), self);

  g_signal_connect (self->playbin, "video-tags-changed",
      G_CALLBACK (video_tags_changed_cb), self);
  g_signal_connect (self->playbin, "audio-tags-changed",
      G_CALLBACK (audio_tags_changed_cb), self);
  g_signal_connect (self->playbin, "text-tags-changed",
      G_CALLBACK (subtitle_tags_changed_cb), self);

  self->target_state = GST_STATE_NULL;
  self->current_state = GST_STATE_NULL;
  change_state (self, GST_PLAYER_STATE_STOPPED);
  self->buffering = 100;
  self->is_eos = FALSE;
  self->is_live = FALSE;

  GST_TRACE_OBJECT (self, "Starting main loop");
  g_main_loop_run (self->loop);
  GST_TRACE_OBJECT (self, "Stopped main loop");

  g_main_loop_unref (self->loop);
  self->loop = NULL;

  g_source_destroy (bus_source);
  g_source_unref (bus_source);
  gst_object_unref (bus);

  remove_tick_source (self);
  remove_ready_timeout_source (self);

  g_mutex_lock (&self->lock);
  if (self->media_info) {
    g_object_unref (self->media_info);
    self->media_info = NULL;
  }

  if (self->seek_source)
    g_source_unref (self->seek_source);
  self->seek_source = NULL;
  g_mutex_unlock (&self->lock);

  g_main_context_pop_thread_default (self->context);
  g_main_context_unref (self->context);
  self->context = NULL;

  self->target_state = GST_STATE_NULL;
  self->current_state = GST_STATE_NULL;
  if (self->playbin) {
    gst_element_set_state (self->playbin, GST_STATE_NULL);
    gst_object_unref (self->playbin);
    self->playbin = NULL;
  }

  GST_TRACE_OBJECT (self, "Stopped main thread");

  return NULL;
}

static gpointer
gst_player_init_once (gpointer user_data)
{
  gst_init (NULL, NULL);

  GST_DEBUG_CATEGORY_INIT (gst_player_debug, "gst-player", 0, "GstPlayer");
  gst_player_error_quark ();

  return NULL;
}

GstPlayer *
gst_player_new (void)
{
  static GOnce once = G_ONCE_INIT;

  g_once (&once, gst_player_init_once, NULL);

  return g_object_new (GST_TYPE_PLAYER, NULL);
}

static gboolean
gst_player_play_internal (gpointer user_data)
{
  GstPlayer *self = GST_PLAYER (user_data);
  GstStateChangeReturn state_ret;

  GST_DEBUG_OBJECT (self, "Play");

  g_mutex_lock (&self->lock);
  if (!self->uri) {
    g_mutex_unlock (&self->lock);
    return G_SOURCE_REMOVE;
  }
  g_mutex_unlock (&self->lock);

  remove_ready_timeout_source (self);
  self->target_state = GST_STATE_PLAYING;

  if (self->current_state < GST_STATE_PAUSED)
    change_state (self, GST_PLAYER_STATE_BUFFERING);

  if (self->current_state >= GST_STATE_PAUSED && !self->is_eos) {
    state_ret = gst_element_set_state (self->playbin, GST_STATE_PLAYING);
  } else {
    state_ret = gst_element_set_state (self->playbin, GST_STATE_PAUSED);
  }

  if (state_ret == GST_STATE_CHANGE_NO_PREROLL) {
    self->is_live = TRUE;
    GST_DEBUG_OBJECT (self, "Pipeline is live");
  }

  if (state_ret == GST_STATE_CHANGE_FAILURE) {
    emit_error (self, g_error_new (GST_PLAYER_ERROR, GST_PLAYER_ERROR_FAILED,
            "Failed to play"));
    return G_SOURCE_REMOVE;
  } else if (state_ret == GST_STATE_CHANGE_NO_PREROLL) {
    self->is_live = TRUE;
    GST_DEBUG_OBJECT (self, "Pipeline is live");
  }

  if (self->is_eos) {
    gboolean ret;

    GST_DEBUG_OBJECT (self, "Was EOS, seeking to beginning");
    self->is_eos = FALSE;
    ret =
        gst_element_seek_simple (self->playbin, GST_FORMAT_TIME,
        GST_SEEK_FLAG_FLUSH, 0);
    if (!ret) {
      GST_ERROR_OBJECT (self, "Seek to beginning failed");
      gst_element_set_state (self->playbin, GST_STATE_READY);
      gst_player_play_internal (self);
    }
  }

  return G_SOURCE_REMOVE;
}

void
gst_player_play (GstPlayer * self)
{
  g_return_if_fail (GST_IS_PLAYER (self));

  g_main_context_invoke (self->context, gst_player_play_internal, self);
}

static gboolean
gst_player_pause_internal (gpointer user_data)
{
  GstPlayer *self = GST_PLAYER (user_data);
  GstStateChangeReturn state_ret;

  GST_DEBUG_OBJECT (self, "Pause");

  g_mutex_lock (&self->lock);
  if (!self->uri) {
    g_mutex_unlock (&self->lock);
    return G_SOURCE_REMOVE;
  }
  g_mutex_unlock (&self->lock);

  tick_cb (self);
  remove_tick_source (self);
  remove_ready_timeout_source (self);

  self->target_state = GST_STATE_PAUSED;

  if (self->current_state < GST_STATE_PAUSED)
    change_state (self, GST_PLAYER_STATE_BUFFERING);

  state_ret = gst_element_set_state (self->playbin, GST_STATE_PAUSED);
  if (state_ret == GST_STATE_CHANGE_FAILURE) {
    emit_error (self, g_error_new (GST_PLAYER_ERROR, GST_PLAYER_ERROR_FAILED,
            "Failed to pause"));
    return G_SOURCE_REMOVE;
  } else if (state_ret == GST_STATE_CHANGE_NO_PREROLL) {
    self->is_live = TRUE;
    GST_DEBUG_OBJECT (self, "Pipeline is live");
  }

  if (self->is_eos) {
    gboolean ret;

    GST_DEBUG_OBJECT (self, "Was EOS, seeking to beginning");
    self->is_eos = FALSE;
    ret =
        gst_element_seek_simple (self->playbin, GST_FORMAT_TIME,
        GST_SEEK_FLAG_FLUSH, 0);
    if (!ret) {
      GST_ERROR_OBJECT (self, "Seek to beginning failed");
      gst_element_set_state (self->playbin, GST_STATE_READY);
      gst_player_pause_internal (self);
    }
  }

  return G_SOURCE_REMOVE;
}

void
gst_player_pause (GstPlayer * self)
{
  g_return_if_fail (GST_IS_PLAYER (self));

  g_main_context_invoke (self->context, gst_player_pause_internal, self);
}

static gboolean
gst_player_stop_internal (gpointer user_data)
{
  GstPlayer *self = GST_PLAYER (user_data);

  GST_DEBUG_OBJECT (self, "Stop");

  tick_cb (self);
  remove_tick_source (self);

  add_ready_timeout_source (self);

  self->target_state = GST_STATE_NULL;
  self->current_state = GST_STATE_READY;
  self->is_live = FALSE;
  self->is_eos = FALSE;
  gst_bus_set_flushing (self->bus, TRUE);
  gst_element_set_state (self->playbin, GST_STATE_READY);
  gst_bus_set_flushing (self->bus, FALSE);
  change_state (self, GST_PLAYER_STATE_STOPPED);
  self->buffering = 100;
  g_mutex_lock (&self->lock);
  if (self->media_info) {
    g_object_unref (self->media_info);
    self->media_info = NULL;
  }
  if (self->global_tags) {
    gst_tag_list_unref (self->global_tags);
    self->global_tags = NULL;
  }
  self->seek_pending = FALSE;
  if (self->seek_source) {
    g_source_destroy (self->seek_source);
    g_source_unref (self->seek_source);
    self->seek_source = NULL;
  }
  self->seek_position = GST_CLOCK_TIME_NONE;
  self->last_seek_time = GST_CLOCK_TIME_NONE;
  g_mutex_unlock (&self->lock);

  return G_SOURCE_REMOVE;
}

void
gst_player_stop (GstPlayer * self)
{
  g_return_if_fail (GST_IS_PLAYER (self));

  g_main_context_invoke (self->context, gst_player_stop_internal, self);
}

/* Must be called with lock from main context, releases lock! */
static void
gst_player_seek_internal_locked (GstPlayer * self)
{
  GstClockTime position;
  gboolean ret;
  GstStateChangeReturn state_ret;

  if (self->seek_source) {
    g_source_destroy (self->seek_source);
    g_source_unref (self->seek_source);
    self->seek_source = NULL;
  }

  /* Only seek in PAUSED */
  if (self->current_state < GST_STATE_PAUSED) {
    return;
  } else if (self->current_state != GST_STATE_PAUSED) {
    g_mutex_unlock (&self->lock);
    state_ret = gst_element_set_state (self->playbin, GST_STATE_PAUSED);
    if (state_ret == GST_STATE_CHANGE_FAILURE) {
      emit_error (self, g_error_new (GST_PLAYER_ERROR, GST_PLAYER_ERROR_FAILED,
              "Failed to seek"));
      g_mutex_lock (&self->lock);
      return;
    }
    g_mutex_lock (&self->lock);
    return;
  }

  self->last_seek_time = gst_util_get_timestamp ();
  position = self->seek_position;
  self->seek_position = GST_CLOCK_TIME_NONE;
  self->seek_pending = TRUE;
  g_mutex_unlock (&self->lock);

  GST_DEBUG_OBJECT (self, "Seek to %" GST_TIME_FORMAT,
      GST_TIME_ARGS (position));

  remove_tick_source (self);
  self->is_eos = FALSE;

  ret =
      gst_element_seek_simple (self->playbin, GST_FORMAT_TIME,
      GST_SEEK_FLAG_FLUSH, position);

  if (!ret)
    emit_error (self, g_error_new (GST_PLAYER_ERROR, GST_PLAYER_ERROR_FAILED,
            "Failed to seek to %" GST_TIME_FORMAT, GST_TIME_ARGS (position)));

  g_mutex_lock (&self->lock);
}

static gboolean
gst_player_seek_internal (gpointer user_data)
{
  GstPlayer *self = GST_PLAYER (user_data);

  g_mutex_lock (&self->lock);
  gst_player_seek_internal_locked (self);
  g_mutex_unlock (&self->lock);

  return G_SOURCE_REMOVE;
}

void
gst_player_seek (GstPlayer * self, GstClockTime position)
{
  g_return_if_fail (GST_IS_PLAYER (self));
  g_return_if_fail (GST_CLOCK_TIME_IS_VALID (position));

  g_mutex_lock (&self->lock);
  if (self->media_info && !self->media_info->seekable) {
    GST_DEBUG_OBJECT (self, "Media is not seekable");
    g_mutex_unlock (&self->lock);
    return;
  }

  self->seek_position = position;

  /* If there is no seek being dispatch to the main context currently do that,
   * otherwise we just updated the seek position so that it will be taken by
   * the seek handler from the main context instead of the old one.
   */
  if (!self->seek_source) {
    GstClockTime now = gst_util_get_timestamp ();

    /* If no seek is pending or it was started more than 250 mseconds ago seek
     * immediately, otherwise wait until the 250 mseconds have passed */
    if (!self->seek_pending || (now - self->last_seek_time > 250 * GST_MSECOND)) {
      self->seek_source = g_idle_source_new ();
      g_source_set_callback (self->seek_source,
          (GSourceFunc) gst_player_seek_internal, self, NULL);
      GST_TRACE_OBJECT (self, "Dispatching seek to position %" GST_TIME_FORMAT,
          GST_TIME_ARGS (position));
      g_source_attach (self->seek_source, self->context);
    } else {
      guint delay = 250000 - (now - self->last_seek_time) / 1000;

      /* Note that last_seek_time must be set to something at this point and
       * it must be smaller than 250 mseconds */
      self->seek_source = g_timeout_source_new (delay);
      g_source_set_callback (self->seek_source,
          (GSourceFunc) gst_player_seek_internal, self, NULL);

      GST_TRACE_OBJECT (self,
          "Delaying seek to position %" GST_TIME_FORMAT " by %u us",
          GST_TIME_ARGS (position), delay);
      g_source_attach (self->seek_source, self->context);
    }
  }
  g_mutex_unlock (&self->lock);
}

gboolean
gst_player_get_dispatch_to_main_context (GstPlayer * self)
{
  gboolean val;

  g_return_val_if_fail (GST_IS_PLAYER (self), FALSE);

  g_object_get (self, "dispatch-to-main-context", &val, NULL);

  return val;
}

void
gst_player_set_dispatch_to_main_context (GstPlayer * self, gboolean val)
{
  g_return_if_fail (GST_IS_PLAYER (self));

  g_object_set (self, "dispatch-to-main-context", val, NULL);
}

gchar *
gst_player_get_uri (GstPlayer * self)
{
  gchar *val;

  g_return_val_if_fail (GST_IS_PLAYER (self), NULL);

  g_object_get (self, "uri", &val, NULL);

  return val;
}

void
gst_player_set_uri (GstPlayer * self, const gchar * val)
{
  g_return_if_fail (GST_IS_PLAYER (self));

  g_object_set (self, "uri", val, NULL);
}

GstClockTime
gst_player_get_position (GstPlayer * self)
{
  GstClockTime val;

  g_return_val_if_fail (GST_IS_PLAYER (self), GST_CLOCK_TIME_NONE);

  g_object_get (self, "position", &val, NULL);

  return val;
}

GstClockTime
gst_player_get_duration (GstPlayer * self)
{
  GstClockTime val;

  g_return_val_if_fail (GST_IS_PLAYER (self), GST_CLOCK_TIME_NONE);

  g_object_get (self, "duration", &val, NULL);

  return val;
}

gdouble
gst_player_get_volume (GstPlayer * self)
{
  gdouble val;

  g_return_val_if_fail (GST_IS_PLAYER (self), 1.0);

  g_object_get (self, "volume", &val, NULL);

  return val;
}

void
gst_player_set_volume (GstPlayer * self, gdouble val)
{
  g_return_if_fail (GST_IS_PLAYER (self));

  g_object_set (self, "volume", val, NULL);
}

gboolean
gst_player_get_mute (GstPlayer * self)
{
  gboolean val;

  g_return_val_if_fail (GST_IS_PLAYER (self), FALSE);

  g_object_get (self, "mute", &val, NULL);

  return val;
}

void
gst_player_set_mute (GstPlayer * self, gboolean val)
{
  g_return_if_fail (GST_IS_PLAYER (self));

  g_object_set (self, "mute", val, NULL);
}

/**
 * gst_player_get_window_handle:
 * @player: #GstPlayer instance
 *
 * Returns: (transfer none): The currently set, platform specific window
 * handle
 */
gpointer
gst_player_get_window_handle (GstPlayer * self)
{
  gpointer val;

  g_return_val_if_fail (GST_IS_PLAYER (self), NULL);

  g_object_get (self, "window-handle", &val, NULL);

  return val;
}

void
gst_player_set_window_handle (GstPlayer * self, gpointer val)
{
  g_return_if_fail (GST_IS_PLAYER (self));

  g_object_set (self, "window-handle", val, NULL);
}

/**
 * gst_player_get_pipeline:
 * @player: #GstPlayer instance
 *
 * Returns: (transfer full): The internal playbin instance
 */
GstElement *
gst_player_get_pipeline (GstPlayer * self)
{
  GstElement *val;

  g_return_val_if_fail (GST_IS_PLAYER (self), NULL);

  g_object_get (self, "pipeline", &val, NULL);

  return val;
}

/**
 * gst_player_get_media_info:
 * @player: #GstPlayer instance
 *
 * A Function to get the current media info #GstPlayerMediaInfo instance.
 *
 * Returns: (transfer full): media info instance.
 *
 * The caller should free it with g_object_unref()
 */
GstPlayerMediaInfo *
gst_player_get_media_info (GstPlayer * self)
{
  GstPlayerMediaInfo *info;

  g_return_val_if_fail (GST_IS_PLAYER (self), NULL);

  if (!self->media_info)
    return NULL;

  g_mutex_lock (&self->lock);
  info = gst_player_media_info_copy (self->media_info);
  g_mutex_unlock (&self->lock);

  return info;
}

/**
 * gst_player_get_current_audio_track:
 * @player: #GstPlayer instance
 *
 * A Function to get current audio #GstPlayerAudioInfo instance.
 *
 * Returns: (transfer full): current audio track.
 *
 * The caller should free it with g_object_unref()
 */
GstPlayerAudioInfo *
gst_player_get_current_audio_track (GstPlayer * self)
{
  GstPlayerAudioInfo *info;

  g_return_val_if_fail (GST_IS_PLAYER (self), NULL);

  if (!is_track_enabled (self, GST_PLAY_FLAG_AUDIO))
    return NULL;

  info = (GstPlayerAudioInfo *) gst_player_stream_info_get_current (self,
      "current-audio", GST_TYPE_PLAYER_AUDIO_INFO);
  return info;
}

/**
 * gst_player_get_current_video_track:
 * @player: #GstPlayer instance
 *
 * A Function to get current video #GstPlayerVideoInfo instance.
 *
 * Returns: (transfer full): current video track.
 *
 * The caller should free it with g_object_unref()
 */
GstPlayerVideoInfo *
gst_player_get_current_video_track (GstPlayer * self)
{
  GstPlayerVideoInfo *info;

  g_return_val_if_fail (GST_IS_PLAYER (self), NULL);

  if (!is_track_enabled (self, GST_PLAY_FLAG_VIDEO))
    return NULL;

  info = (GstPlayerVideoInfo *) gst_player_stream_info_get_current (self,
      "current-video", GST_TYPE_PLAYER_VIDEO_INFO);
  return info;
}

/**
 * gst_player_get_current_subtitle_track:
 * @player: #GstPlayer instance
 *
 * A Function to get current subtitle #GstPlayerSubtitleInfo instance.
 *
 * Returns: (transfer none): current subtitle track.
 *
 * The caller should free it with g_object_unref()
 */
GstPlayerSubtitleInfo *
gst_player_get_current_subtitle_track (GstPlayer * self)
{
  GstPlayerSubtitleInfo *info;

  g_return_val_if_fail (GST_IS_PLAYER (self), NULL);

  if (!is_track_enabled (self, GST_PLAY_FLAG_SUBTITLE))
    return NULL;

  info = (GstPlayerSubtitleInfo *) gst_player_stream_info_get_current (self,
      "current-text", GST_TYPE_PLAYER_SUBTITLE_INFO);
  return info;
}

/**
 * gst_player_set_audio_track:
 * @player: #GstPlayer instance
 * @stream_index: stream index
 */
gboolean
gst_player_set_audio_track (GstPlayer * self, gint stream_index)
{
  GstPlayerStreamInfo *info;

  g_return_val_if_fail (GST_IS_PLAYER (self), 0);

  g_mutex_lock (&self->lock);
  info = gst_player_stream_info_find (self, self->media_info,
      GST_TYPE_PLAYER_AUDIO_INFO, stream_index);
  g_mutex_unlock (&self->lock);
  if (!info) {
    GST_ERROR_OBJECT (self, "invalid audio stream index %d", stream_index);
    return FALSE;
  }

  g_object_set (G_OBJECT (self->playbin), "current-audio", stream_index, NULL);
  return TRUE;
}

/**
 * gst_player_set_video_track:
 * @player: #GstPlayer instance
 * @stream_index: stream index
 */
gboolean
gst_player_set_video_track (GstPlayer * self, gint stream_index)
{
  GstPlayerStreamInfo *info;

  g_return_val_if_fail (GST_IS_PLAYER (self), 0);

  /* check if stream_index exist in our internal media_info list */
  g_mutex_lock (&self->lock);
  info = gst_player_stream_info_find (self, self->media_info,
      GST_TYPE_PLAYER_VIDEO_INFO, stream_index);
  g_mutex_unlock (&self->lock);
  if (!info) {
    GST_ERROR_OBJECT (self, "invalid video stream index %d", stream_index);
    return FALSE;
  }

  g_object_set (G_OBJECT (self->playbin), "current-video", stream_index, NULL);
  return TRUE;
}

/**
 * gst_player_set_subtitle_track:
 * @player: #GstPlayer instance
 * @stream_index: stream index
 */
gboolean
gst_player_set_subtitle_track (GstPlayer * self, gint stream_index)
{
  GstPlayerStreamInfo *info;

  g_return_val_if_fail (GST_IS_PLAYER (self), 0);

  g_mutex_lock (&self->lock);
  info = gst_player_stream_info_find (self, self->media_info,
      GST_TYPE_PLAYER_SUBTITLE_INFO, stream_index);
  g_mutex_unlock (&self->lock);
  if (!info) {
    GST_ERROR_OBJECT (self, "invalid subtitle stream index %d", stream_index);
    return FALSE;
  }

  g_object_set (G_OBJECT (self->playbin), "current-text", stream_index, NULL);
  return TRUE;
}

/*
 * gst_player_set_audio_enabled:
 * @player: #GstPlayer instance
 * @enabled: TRUE or FALSE
 *
 * Enable or disable the current audio track.
 */
void
gst_player_set_audio_track_enabled (GstPlayer * self, gboolean enabled)
{
  g_return_if_fail (GST_IS_PLAYER (self));

  if (enabled)
    player_set_flag (self, GST_PLAY_FLAG_AUDIO);
  else
    player_clear_flag (self, GST_PLAY_FLAG_AUDIO);
}

/*
 * gst_player_set_video_enabled:
 * @player: #GstPlayer instance
 * @enabled: TRUE or FALSE
 *
 * Enable or disable the current video track.
 */
void
gst_player_set_video_track_enabled (GstPlayer * self, gboolean enabled)
{
  g_return_if_fail (GST_IS_PLAYER (self));

  if (enabled)
    player_set_flag (self, GST_PLAY_FLAG_VIDEO);
  else
    player_clear_flag (self, GST_PLAY_FLAG_VIDEO);
}

/*
 * gst_player_set_subtitle_enabled:
 * @player: #GstPlayer instance
 * @enabled: TRUE or FALSE
 *
 * Enable or disable the current subtitle track.
 */
void
gst_player_set_subtitle_track_enabled (GstPlayer * self, gboolean enabled)
{
  g_return_if_fail (GST_IS_PLAYER (self));

  if (enabled)
    player_set_flag (self, GST_PLAY_FLAG_SUBTITLE);
  else
    player_clear_flag (self, GST_PLAY_FLAG_SUBTITLE);
}

#define C_ENUM(v) ((gint) v)
#define C_FLAGS(v) ((guint) v)

GType
gst_player_state_get_type (void)
{
  static gsize id = 0;
  static const GEnumValue values[] = {
    {C_ENUM (GST_PLAYER_STATE_STOPPED), "GST_PLAYER_STATE_STOPPED", "stopped"},
    {C_ENUM (GST_PLAYER_STATE_BUFFERING), "GST_PLAYER_STATE_BUFFERING",
        "buffering"},
    {C_ENUM (GST_PLAYER_STATE_PAUSED), "GST_PLAYER_STATE_PAUSED", "paused"},
    {C_ENUM (GST_PLAYER_STATE_PLAYING), "GST_PLAYER_STATE_PLAYING", "playing"},
    {0, NULL, NULL}
  };

  if (g_once_init_enter (&id)) {
    GType tmp = g_enum_register_static ("GstPlayerState", values);
    g_once_init_leave (&id, tmp);
  }

  return (GType) id;
}

const gchar *
gst_player_state_get_name (GstPlayerState state)
{
  switch (state) {
    case GST_PLAYER_STATE_STOPPED:
      return "stopped";
    case GST_PLAYER_STATE_BUFFERING:
      return "buffering";
    case GST_PLAYER_STATE_PAUSED:
      return "paused";
    case GST_PLAYER_STATE_PLAYING:
      return "playing";
  }

  g_assert_not_reached ();
  return NULL;
}

GType
gst_player_error_get_type (void)
{
  static gsize id = 0;
  static const GEnumValue values[] = {
    {C_ENUM (GST_PLAYER_ERROR_FAILED), "GST_PLAYER_ERROR_FAILED", "failed"},
    {0, NULL, NULL}
  };

  if (g_once_init_enter (&id)) {
    GType tmp = g_enum_register_static ("GstPlayerError", values);
    g_once_init_leave (&id, tmp);
  }

  return (GType) id;
}

const gchar *
gst_player_error_get_name (GstPlayerError error)
{
  switch (error) {
    case GST_PLAYER_ERROR_FAILED:
      return "failed";
  }

  g_assert_not_reached ();
  return NULL;
}
