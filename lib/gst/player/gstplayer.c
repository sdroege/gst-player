/* GStreamer
 *
 * Copyright (C) 2014-2015 Sebastian Dröge <sebastian@centricular.com>
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
 * - Media info, tags
 * - Audio track selection
 * - Subtitle track selection, external subs, disable
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

#include <gst/gst.h>
#include <gst/video/video.h>

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
  SIGNAL_LAST
};

struct _GstPlayerPrivate
{
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

  /* Protected by lock */
  gboolean seek_pending;        /* Only set from main context */
  GstClockTime last_seek_time;  /* Only set from main context */
  GSource *seek_source;
  GstClockTime seek_position;
};

#define parent_class gst_player_parent_class
G_DEFINE_TYPE_WITH_PRIVATE (GstPlayer, gst_player, GST_TYPE_OBJECT);

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

static void
gst_player_init (GstPlayer * self)
{
  GST_TRACE_OBJECT (self, "Initializing");

  self->priv = gst_player_get_instance_private (self);

  g_mutex_init (&self->priv->lock);
  g_cond_init (&self->priv->cond);

  self->priv->seek_pending = FALSE;
  self->priv->seek_position = GST_CLOCK_TIME_NONE;
  self->priv->last_seek_time = GST_CLOCK_TIME_NONE;

  g_mutex_lock (&self->priv->lock);
  self->priv->thread = g_thread_new ("GstPlayer", gst_player_main, self);
  while (!self->priv->loop || !g_main_loop_is_running (self->priv->loop))
    g_cond_wait (&self->priv->cond, &self->priv->lock);
  g_mutex_unlock (&self->priv->lock);
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
}

static void
gst_player_finalize (GObject * object)
{
  GstPlayer *self = GST_PLAYER (object);

  GST_TRACE_OBJECT (self, "Stopping main thread");
  g_main_loop_quit (self->priv->loop);
  g_thread_join (self->priv->thread);

  GST_TRACE_OBJECT (self, "Finalizing");

  g_free (self->priv->uri);
  if (self->priv->application_context)
    g_main_context_unref (self->priv->application_context);

  g_mutex_clear (&self->priv->lock);
  g_cond_clear (&self->priv->cond);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gboolean
gst_player_set_uri_internal (gpointer user_data)
{
  GstPlayer *self = user_data;

  gst_player_stop_internal (self);

  g_mutex_lock (&self->priv->lock);

  GST_DEBUG_OBJECT (self, "Changing URI to '%s'",
      GST_STR_NULL (self->priv->uri));

  g_object_set (self->priv->playbin, "uri", self->priv->uri, NULL);

  g_mutex_unlock (&self->priv->lock);

  return G_SOURCE_REMOVE;
}

static void
gst_player_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstPlayer *self = GST_PLAYER (object);

  switch (prop_id) {
    case PROP_DISPATCH_TO_MAIN_CONTEXT:
      self->priv->dispatch_to_main_context = g_value_get_boolean (value);
      self->priv->application_context = g_main_context_ref_thread_default ();
      break;
    case PROP_URI:{
      g_mutex_lock (&self->priv->lock);
      if (self->priv->uri)
        g_free (self->priv->uri);

      self->priv->uri = g_value_dup_string (value);
      GST_DEBUG_OBJECT (self, "Set uri=%s", self->priv->uri);
      g_mutex_unlock (&self->priv->lock);

      g_main_context_invoke (self->priv->context, gst_player_set_uri_internal,
          self);
      break;
    }
    case PROP_VOLUME:
      GST_DEBUG_OBJECT (self, "Set volume=%lf", g_value_get_double (value));
      g_object_set_property (G_OBJECT (self->priv->playbin), "volume", value);
      break;
    case PROP_MUTE:
      GST_DEBUG_OBJECT (self, "Set mute=%d", g_value_get_boolean (value));
      g_object_set_property (G_OBJECT (self->priv->playbin), "mute", value);
      break;
    case PROP_WINDOW_HANDLE:
      GST_DEBUG_OBJECT (self, "Set window handle from %p to %p",
          (gpointer) self->priv->window_handle, g_value_get_pointer (value));
      self->priv->window_handle = (guintptr) g_value_get_pointer (value);
      gst_video_overlay_set_window_handle (GST_VIDEO_OVERLAY (self->
              priv->playbin), self->priv->window_handle);
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
      g_mutex_lock (&self->priv->lock);
      g_value_set_string (value, self->priv->uri);
      g_mutex_unlock (&self->priv->lock);
      break;
      GST_TRACE_OBJECT (self, "Returning is-playing=%d",
          g_value_get_boolean (value));
      break;
    case PROP_POSITION:{
      gint64 position;

      gst_element_query_position (self->priv->playbin, GST_FORMAT_TIME,
          &position);
      g_value_set_uint64 (value, position);
      GST_TRACE_OBJECT (self, "Returning position=%" GST_TIME_FORMAT,
          GST_TIME_ARGS (g_value_get_uint64 (value)));
      break;
    }
    case PROP_DURATION:{
      gint64 duration;

      gst_element_query_duration (self->priv->playbin, GST_FORMAT_TIME,
          &duration);
      g_value_set_uint64 (value, duration);
      GST_TRACE_OBJECT (self, "Returning duration=%" GST_TIME_FORMAT,
          GST_TIME_ARGS (g_value_get_uint64 (value)));
      break;
    }
    case PROP_VOLUME:
      g_object_get_property (G_OBJECT (self->priv->playbin), "volume", value);
      GST_TRACE_OBJECT (self, "Returning volume=%lf",
          g_value_get_double (value));
      break;
    case PROP_MUTE:
      g_object_get_property (G_OBJECT (self->priv->playbin), "mute", value);
      GST_TRACE_OBJECT (self, "Returning mute=%d", g_value_get_boolean (value));
      break;
    case PROP_WINDOW_HANDLE:
      g_value_set_pointer (value, (gpointer) self->priv->window_handle);
      GST_TRACE_OBJECT (self, "Returning window-handle=%p",
          g_value_get_pointer (value));
      break;
    case PROP_PIPELINE:
      g_value_set_object (value, self->priv->playbin);
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

  g_mutex_lock (&self->priv->lock);
  g_cond_signal (&self->priv->cond);
  g_mutex_unlock (&self->priv->lock);

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
  if (state == self->priv->app_state)
    return;

  GST_DEBUG_OBJECT (self, "Changing app state from %s to %s",
      gst_player_state_get_name (self->priv->app_state),
      gst_player_state_get_name (state));
  self->priv->app_state = state;

  if (self->priv->dispatch_to_main_context
      && g_signal_handler_find (self, G_SIGNAL_MATCH_ID,
          signals[SIGNAL_STATE_CHANGED], 0, NULL, NULL, NULL) != 0) {
    StateChangedSignalData *data = g_new (StateChangedSignalData, 1);

    data->player = self;
    data->state = state;
    g_main_context_invoke_full (self->priv->application_context,
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

  if (gst_element_query_position (self->priv->playbin, GST_FORMAT_TIME,
          &position)) {
    GST_LOG_OBJECT (self, "Position %" GST_TIME_FORMAT,
        GST_TIME_ARGS (position));

    if (self->priv->dispatch_to_main_context
        && g_signal_handler_find (self, G_SIGNAL_MATCH_ID,
            signals[SIGNAL_POSITION_UPDATED], 0, NULL, NULL, NULL) != 0) {
      PositionUpdatedSignalData *data = g_new (PositionUpdatedSignalData, 1);

      data->player = self;
      data->position = position;
      g_main_context_invoke_full (self->priv->application_context,
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
  if (self->priv->tick_source)
    return;

  self->priv->tick_source = g_timeout_source_new (100);
  g_source_set_callback (self->priv->tick_source, (GSourceFunc) tick_cb, self,
      NULL);
  g_source_attach (self->priv->tick_source, self->priv->context);
}

static void
remove_tick_source (GstPlayer * self)
{
  if (!self->priv->tick_source)
    return;

  g_source_destroy (self->priv->tick_source);
  g_source_unref (self->priv->tick_source);
  self->priv->tick_source = NULL;
}

static gboolean
ready_timeout_cb (gpointer user_data)
{
  GstPlayer *self = user_data;

  if (self->priv->target_state <= GST_STATE_READY) {
    GST_DEBUG_OBJECT (self, "Setting pipeline to NULL state");
    self->priv->target_state = GST_STATE_NULL;
    self->priv->current_state = GST_STATE_NULL;
    gst_element_set_state (self->priv->playbin, GST_STATE_NULL);
  }

  return G_SOURCE_REMOVE;
}

static void
add_ready_timeout_source (GstPlayer * self)
{
  if (self->priv->ready_timeout_source)
    return;

  self->priv->ready_timeout_source = g_timeout_source_new_seconds (60);
  g_source_set_callback (self->priv->ready_timeout_source,
      (GSourceFunc) ready_timeout_cb, self, NULL);
  g_source_attach (self->priv->ready_timeout_source, self->priv->context);
}

static void
remove_ready_timeout_source (GstPlayer * self)
{
  if (!self->priv->ready_timeout_source)
    return;

  g_source_destroy (self->priv->ready_timeout_source);
  g_source_unref (self->priv->ready_timeout_source);
  self->priv->ready_timeout_source = NULL;
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

  if (self->priv->dispatch_to_main_context
      && g_signal_handler_find (self, G_SIGNAL_MATCH_ID,
          signals[SIGNAL_ERROR], 0, NULL, NULL, NULL) != 0) {
    ErrorSignalData *data = g_new (ErrorSignalData, 1);

    data->player = self;
    data->err = g_error_copy (err);
    g_main_context_invoke_full (self->priv->application_context,
        G_PRIORITY_DEFAULT, error_dispatch, data,
        (GDestroyNotify) free_error_signal_data);
  } else {
    g_signal_emit (self, signals[SIGNAL_ERROR], 0, err);
  }

  g_error_free (err);

  remove_tick_source (self);
  remove_ready_timeout_source (self);

  self->priv->target_state = GST_STATE_NULL;
  self->priv->current_state = GST_STATE_NULL;
  self->priv->is_live = FALSE;
  self->priv->is_eos = FALSE;
  gst_element_set_state (self->priv->playbin, GST_STATE_NULL);
  change_state (self, GST_PLAYER_STATE_STOPPED);
  self->priv->buffering = 100;

  g_mutex_lock (&self->priv->lock);
  self->priv->seek_pending = FALSE;
  if (self->priv->seek_source) {
    g_source_destroy (self->priv->seek_source);
    g_source_unref (self->priv->seek_source);
    self->priv->seek_source = NULL;
  }
  self->priv->seek_position = GST_CLOCK_TIME_NONE;
  self->priv->last_seek_time = GST_CLOCK_TIME_NONE;
  g_mutex_unlock (&self->priv->lock);
}

static void
error_cb (GstBus * bus, GstMessage * msg, gpointer user_data)
{
  GstPlayer *self = GST_PLAYER (user_data);
  GError *err, *player_err;
  gchar *name, *debug, *message, *full_message;

  gst_message_parse_error (msg, &err, &debug);

  name = gst_object_get_path_string (msg->src);
  gst_message_parse_error (msg, &err, &debug);

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

  if (self->priv->dispatch_to_main_context
      && g_signal_handler_find (self, G_SIGNAL_MATCH_ID,
          signals[SIGNAL_END_OF_STREAM], 0, NULL, NULL, NULL) != 0) {
    g_main_context_invoke (self->priv->application_context, eos_dispatch, self);
  } else {
    g_signal_emit (self, signals[SIGNAL_END_OF_STREAM], 0);
  }
  change_state (self, GST_PLAYER_STATE_STOPPED);
  self->priv->buffering = 100;
  self->priv->is_eos = TRUE;
}

void gst_player_set_playback_rate (GstPlayer * self, gdouble rate)
{
  g_return_if_fail (GST_IS_PLAYER (self));
  self->priv->rate = rate;
  gst_player_seek (self, 0);
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

  if (self->priv->is_live)
    return;

  gst_message_parse_buffering (msg, &percent);
  GST_LOG_OBJECT (self, "Buffering %d%%", percent);

  if (percent < 100 && self->priv->target_state >= GST_STATE_PAUSED) {
    GstStateChangeReturn state_ret;

    GST_DEBUG_OBJECT (self, "Waiting for buffering to finish");
    state_ret = gst_element_set_state (self->priv->playbin, GST_STATE_PAUSED);

    if (state_ret == GST_STATE_CHANGE_FAILURE) {
      emit_error (self, g_error_new (GST_PLAYER_ERROR, GST_PLAYER_ERROR_FAILED,
              "Failed to handle buffering"));
      return;
    }

    change_state (self, GST_PLAYER_STATE_BUFFERING);
  }

  if (self->priv->buffering != percent) {
    if (self->priv->dispatch_to_main_context
        && g_signal_handler_find (self, G_SIGNAL_MATCH_ID,
            signals[SIGNAL_BUFFERING], 0, NULL, NULL, NULL) != 0) {
      BufferingSignalData *data = g_new (BufferingSignalData, 1);

      data->player = self;
      data->percent = percent;
      g_main_context_invoke_full (self->priv->application_context,
          G_PRIORITY_DEFAULT, buffering_dispatch, data,
          (GDestroyNotify) g_free);
    } else {
      g_signal_emit (self, signals[SIGNAL_BUFFERING], 0, percent);
    }

    self->priv->buffering = percent;
  }


  g_mutex_lock (&self->priv->lock);
  if (percent == 100 && (self->priv->seek_position != GST_CLOCK_TIME_NONE ||
          self->priv->seek_pending)) {
    g_mutex_unlock (&self->priv->lock);

    GST_DEBUG_OBJECT (self, "Buffering finished - seek pending");
  } else if (percent == 100 && self->priv->target_state >= GST_STATE_PLAYING) {
    GstStateChangeReturn state_ret;

    g_mutex_unlock (&self->priv->lock);

    GST_DEBUG_OBJECT (self, "Buffering finished - going to PLAYING");
    state_ret = gst_element_set_state (self->priv->playbin, GST_STATE_PLAYING);
    /* Application state change is happening when the state change happened */
    if (state_ret == GST_STATE_CHANGE_FAILURE)
      emit_error (self, g_error_new (GST_PLAYER_ERROR, GST_PLAYER_ERROR_FAILED,
              "Failed to handle buffering"));
  } else if (percent == 100 && self->priv->target_state >= GST_STATE_PAUSED) {
    g_mutex_unlock (&self->priv->lock);

    GST_DEBUG_OBJECT (self, "Buffering finished - staying PAUSED");
    change_state (self, GST_PLAYER_STATE_PAUSED);
  } else {
    g_mutex_unlock (&self->priv->lock);
  }
}

static void
clock_lost_cb (GstBus * bus, GstMessage * msg, gpointer user_data)
{
  GstPlayer *self = GST_PLAYER (user_data);
  GstStateChangeReturn state_ret;

  GST_DEBUG_OBJECT (self, "Clock lost");
  if (self->priv->target_state >= GST_STATE_PLAYING) {
    state_ret = gst_element_set_state (self->priv->playbin, GST_STATE_PAUSED);
    if (state_ret != GST_STATE_CHANGE_FAILURE)
      state_ret =
          gst_element_set_state (self->priv->playbin, GST_STATE_PLAYING);

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

  g_object_get (self->priv->playbin, "video-sink", &video_sink, NULL);
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
  if (self->priv->dispatch_to_main_context
      && g_signal_handler_find (self, G_SIGNAL_MATCH_ID,
          signals[SIGNAL_VIDEO_DIMENSIONS_CHANGED], 0, NULL, NULL, NULL) != 0) {
    VideoDimensionsChangedSignalData *data =
        g_new (VideoDimensionsChangedSignalData, 1);

    data->player = self;
    data->width = width;
    data->height = height;
    g_main_context_invoke_full (self->priv->application_context,
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

  if (self->priv->dispatch_to_main_context
      && g_signal_handler_find (self, G_SIGNAL_MATCH_ID,
          signals[SIGNAL_DURATION_CHANGED], 0, NULL, NULL, NULL) != 0) {
    DurationChangedSignalData *data = g_new (DurationChangedSignalData, 1);

    data->player = self;
    data->duration = duration;
    g_main_context_invoke_full (self->priv->application_context,
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

  if (GST_MESSAGE_SRC (msg) == GST_OBJECT (self->priv->playbin)) {
    GST_DEBUG_OBJECT (self, "Changed state old: %s new: %s pending: %s",
        gst_element_state_get_name (old_state),
        gst_element_state_get_name (new_state),
        gst_element_state_get_name (pending_state));

    self->priv->current_state = new_state;

    if (old_state == GST_STATE_READY && new_state == GST_STATE_PAUSED
        && pending_state == GST_STATE_VOID_PENDING) {
      GstElement *video_sink;
      GstPad *video_sink_pad;
      gint64 duration = -1;

      GST_DEBUG_OBJECT (self, "Initial PAUSED - pre-rolled");

      g_object_get (self->priv->playbin, "video-sink", &video_sink, NULL);

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
      gst_element_query_duration (self->priv->playbin, GST_FORMAT_TIME,
          &duration);
      emit_duration_changed (self, duration);
    }

    if (new_state == GST_STATE_PAUSED
        && pending_state == GST_STATE_VOID_PENDING) {
      remove_tick_source (self);

      g_mutex_lock (&self->priv->lock);
      if (self->priv->seek_pending) {
        self->priv->seek_pending = FALSE;

        /* A new seek is pending */
        if (self->priv->seek_source) {
          GST_DEBUG_OBJECT (self, "Seek finished but new seek is pending");
          gst_player_seek_internal_locked (self);
        } else {
          GST_DEBUG_OBJECT (self, "Seek finished");
        }
      }

      if (self->priv->seek_position != GST_CLOCK_TIME_NONE) {
        GST_DEBUG_OBJECT (self, "Seeking now that we reached PAUSED state");
        gst_player_seek_internal_locked (self);
        g_mutex_unlock (&self->priv->lock);
      } else if (!self->priv->seek_pending) {
        g_mutex_unlock (&self->priv->lock);

        tick_cb (self);

        if (self->priv->target_state >= GST_STATE_PLAYING
            && self->priv->buffering == 100) {
          GstStateChangeReturn state_ret;

          state_ret =
              gst_element_set_state (self->priv->playbin, GST_STATE_PLAYING);
          if (state_ret == GST_STATE_CHANGE_FAILURE)
            emit_error (self, g_error_new (GST_PLAYER_ERROR,
                    GST_PLAYER_ERROR_FAILED, "Failed to play"));
        } else if (self->priv->buffering == 100) {
          change_state (self, GST_PLAYER_STATE_PAUSED);
        }
      } else {
        g_mutex_unlock (&self->priv->lock);
      }
    } else if (new_state == GST_STATE_PLAYING
        && pending_state == GST_STATE_VOID_PENDING) {

      /* If no seek is currently pending, add the tick source. This can happen
       * if we seeked already but the state-change message was still queued up */
      if (!self->priv->seek_pending) {
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

  if (gst_element_query_duration (self->priv->playbin, GST_FORMAT_TIME,
          &duration)) {
    emit_duration_changed (self, duration);
  }
}

static void
latency_cb (GstBus * bus, GstMessage * msg, gpointer user_data)
{
  GstPlayer *self = GST_PLAYER (user_data);

  GST_DEBUG_OBJECT (self, "Latency changed");

  gst_bin_recalculate_latency (GST_BIN (self->priv->playbin));
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

  self->priv->target_state = state;
  state_ret = gst_element_set_state (self->priv->playbin, state);
  if (state_ret == GST_STATE_CHANGE_FAILURE)
    emit_error (self, g_error_new (GST_PLAYER_ERROR, GST_PLAYER_ERROR_FAILED,
            "Failed to change to requested state %s",
            gst_element_state_get_name (state)));
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
      target_state = self->priv->target_state;

      g_mutex_lock (&self->priv->lock);
      if (self->priv->uri)
        g_free (self->priv->uri);

      self->priv->uri = g_strdup (new_location);
      g_mutex_unlock (&self->priv->lock);

      gst_player_set_uri_internal (self);

      if (target_state == GST_STATE_PAUSED)
        gst_player_pause_internal (self);
      else if (target_state == GST_STATE_PLAYING)
        gst_player_play_internal (self);
    }
  }
}

static gpointer
gst_player_main (gpointer data)
{
  GstPlayer *self = GST_PLAYER (data);
  GstBus *bus;
  GSource *source;
  GSource *bus_source;

  GST_TRACE_OBJECT (self, "Starting main thread");

  self->priv->context = g_main_context_new ();
  g_main_context_push_thread_default (self->priv->context);

  self->priv->loop = g_main_loop_new (self->priv->context, FALSE);

  source = g_idle_source_new ();
  g_source_set_callback (source, (GSourceFunc) main_loop_running_cb, self,
      NULL);
  g_source_attach (source, self->priv->context);
  g_source_unref (source);

  self->priv->playbin = gst_element_factory_make ("playbin", "playbin");

  self->priv->bus = bus = gst_element_get_bus (self->priv->playbin);
  bus_source = gst_bus_create_watch (bus);
  g_source_set_callback (bus_source, (GSourceFunc) gst_bus_async_signal_func,
      NULL, NULL);
  g_source_attach (bus_source, self->priv->context);

  g_signal_connect (G_OBJECT (bus), "message::error", G_CALLBACK (error_cb),
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

  self->priv->target_state = GST_STATE_NULL;
  self->priv->current_state = GST_STATE_NULL;
  change_state (self, GST_PLAYER_STATE_STOPPED);
  self->priv->buffering = 100;
  self->priv->is_eos = FALSE;
  self->priv->is_live = FALSE;

  GST_TRACE_OBJECT (self, "Starting main loop");
  g_main_loop_run (self->priv->loop);
  GST_TRACE_OBJECT (self, "Stopped main loop");

  g_main_loop_unref (self->priv->loop);
  self->priv->loop = NULL;

  g_source_destroy (bus_source);
  g_source_unref (bus_source);
  gst_object_unref (bus);

  remove_tick_source (self);
  remove_ready_timeout_source (self);

  if (self->priv->seek_source)
    g_source_unref (self->priv->seek_source);
  self->priv->seek_source = NULL;

  g_main_context_pop_thread_default (self->priv->context);
  g_main_context_unref (self->priv->context);
  self->priv->context = NULL;

  self->priv->target_state = GST_STATE_NULL;
  self->priv->current_state = GST_STATE_NULL;
  if (self->priv->playbin) {
    gst_element_set_state (self->priv->playbin, GST_STATE_NULL);
    gst_object_unref (self->priv->playbin);
    self->priv->playbin = NULL;
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

  g_mutex_lock (&self->priv->lock);
  if (!self->priv->uri) {
    g_mutex_unlock (&self->priv->lock);
    return G_SOURCE_REMOVE;
  }
  g_mutex_unlock (&self->priv->lock);

  remove_ready_timeout_source (self);
  self->priv->target_state = GST_STATE_PLAYING;

  if (self->priv->current_state < GST_STATE_PAUSED)
    change_state (self, GST_PLAYER_STATE_BUFFERING);

  if (self->priv->current_state >= GST_STATE_PAUSED && !self->priv->is_eos) {
    state_ret = gst_element_set_state (self->priv->playbin, GST_STATE_PLAYING);
  } else {
    state_ret = gst_element_set_state (self->priv->playbin, GST_STATE_PAUSED);
  }

  if (state_ret == GST_STATE_CHANGE_NO_PREROLL) {
    self->priv->is_live = TRUE;
    GST_DEBUG_OBJECT (self, "Pipeline is live");
  }

  if (state_ret == GST_STATE_CHANGE_FAILURE) {
    emit_error (self, g_error_new (GST_PLAYER_ERROR, GST_PLAYER_ERROR_FAILED,
            "Failed to play"));
    return G_SOURCE_REMOVE;
  } else if (state_ret == GST_STATE_CHANGE_NO_PREROLL) {
    self->priv->is_live = TRUE;
    GST_DEBUG_OBJECT (self, "Pipeline is live");
  }

  if (self->priv->is_eos) {
    gboolean ret;

    GST_DEBUG_OBJECT (self, "Was EOS, seeking to beginning");
    self->priv->is_eos = FALSE;
    ret =
        gst_element_seek_simple (self->priv->playbin, GST_FORMAT_TIME,
        GST_SEEK_FLAG_FLUSH, 0);
    if (!ret) {
      GST_ERROR_OBJECT (self, "Seek to beginning failed");
      gst_element_set_state (self->priv->playbin, GST_STATE_READY);
      gst_player_play_internal (self);
    }
  }

  return G_SOURCE_REMOVE;
}

void
gst_player_play (GstPlayer * self)
{
  g_return_if_fail (GST_IS_PLAYER (self));

  g_main_context_invoke (self->priv->context, gst_player_play_internal, self);
}

static gboolean
gst_player_pause_internal (gpointer user_data)
{
  GstPlayer *self = GST_PLAYER (user_data);
  GstStateChangeReturn state_ret;

  GST_DEBUG_OBJECT (self, "Pause");

  g_mutex_lock (&self->priv->lock);
  if (!self->priv->uri) {
    g_mutex_unlock (&self->priv->lock);
    return G_SOURCE_REMOVE;
  }
  g_mutex_unlock (&self->priv->lock);

  tick_cb (self);
  remove_tick_source (self);
  remove_ready_timeout_source (self);

  self->priv->target_state = GST_STATE_PAUSED;

  if (self->priv->current_state < GST_STATE_PAUSED)
    change_state (self, GST_PLAYER_STATE_BUFFERING);

  state_ret = gst_element_set_state (self->priv->playbin, GST_STATE_PAUSED);
  if (state_ret == GST_STATE_CHANGE_FAILURE) {
    emit_error (self, g_error_new (GST_PLAYER_ERROR, GST_PLAYER_ERROR_FAILED,
            "Failed to pause"));
    return G_SOURCE_REMOVE;
  } else if (state_ret == GST_STATE_CHANGE_NO_PREROLL) {
    self->priv->is_live = TRUE;
    GST_DEBUG_OBJECT (self, "Pipeline is live");
  }

  if (self->priv->is_eos) {
    gboolean ret;

    GST_DEBUG_OBJECT (self, "Was EOS, seeking to beginning");
    self->priv->is_eos = FALSE;
    ret =
        gst_element_seek_simple (self->priv->playbin, GST_FORMAT_TIME,
        GST_SEEK_FLAG_FLUSH, 0);
    if (!ret) {
      GST_ERROR_OBJECT (self, "Seek to beginning failed");
      gst_element_set_state (self->priv->playbin, GST_STATE_READY);
      gst_player_pause_internal (self);
    }
  }

  return G_SOURCE_REMOVE;
}

void
gst_player_pause (GstPlayer * self)
{
  g_return_if_fail (GST_IS_PLAYER (self));

  g_main_context_invoke (self->priv->context, gst_player_pause_internal, self);
}

static gboolean
gst_player_stop_internal (gpointer user_data)
{
  GstPlayer *self = GST_PLAYER (user_data);

  GST_DEBUG_OBJECT (self, "Stop");

  tick_cb (self);
  remove_tick_source (self);

  add_ready_timeout_source (self);

  self->priv->target_state = GST_STATE_NULL;
  self->priv->current_state = GST_STATE_READY;
  self->priv->is_live = FALSE;
  self->priv->is_eos = FALSE;
  gst_bus_set_flushing (self->priv->bus, TRUE);
  gst_element_set_state (self->priv->playbin, GST_STATE_READY);
  gst_bus_set_flushing (self->priv->bus, FALSE);
  change_state (self, GST_PLAYER_STATE_STOPPED);
  self->priv->buffering = 100;

  g_mutex_lock (&self->priv->lock);
  self->priv->seek_pending = FALSE;
  if (self->priv->seek_source) {
    g_source_destroy (self->priv->seek_source);
    g_source_unref (self->priv->seek_source);
    self->priv->seek_source = NULL;
  }
  self->priv->seek_position = GST_CLOCK_TIME_NONE;
  self->priv->last_seek_time = GST_CLOCK_TIME_NONE;
  g_mutex_unlock (&self->priv->lock);

  return G_SOURCE_REMOVE;
}

void
gst_player_stop (GstPlayer * self)
{
  g_return_if_fail (GST_IS_PLAYER (self));

  g_main_context_invoke (self->priv->context, gst_player_stop_internal, self);
}

/* Must be called with lock from main context, releases lock! */
static void
gst_player_seek_internal_locked (GstPlayer * self)
{
  GstClockTime position,start_pos;
  gboolean ret;
  GstStateChangeReturn state_ret;

  if (self->priv->seek_source) {
    g_source_destroy (self->priv->seek_source);
    g_source_unref (self->priv->seek_source);
    self->priv->seek_source = NULL;
  }

  /* Only seek in PAUSED */
  if (self->priv->current_state < GST_STATE_PAUSED) {
    return;
  } else if (self->priv->current_state != GST_STATE_PAUSED) {
    g_mutex_unlock (&self->priv->lock);
    state_ret = gst_element_set_state (self->priv->playbin, GST_STATE_PAUSED);
    if (state_ret == GST_STATE_CHANGE_FAILURE) {
      emit_error (self, g_error_new (GST_PLAYER_ERROR, GST_PLAYER_ERROR_FAILED,
              "Failed to seek"));
      g_mutex_lock (&self->priv->lock);
      return;
    }
    g_mutex_lock (&self->priv->lock);
    return;
  }

  self->priv->last_seek_time = gst_util_get_timestamp ();
  position = self->priv->seek_position;
  self->priv->seek_position = GST_CLOCK_TIME_NONE;
  self->priv->seek_pending = TRUE;
  g_mutex_unlock (&self->priv->lock);

  GST_DEBUG_OBJECT (self, "Seek to %" GST_TIME_FORMAT,
      GST_TIME_ARGS (position));

  remove_tick_source (self);
  self->priv->is_eos = FALSE;
  
  start_pos = gst_player_get_position(self);

  ret =
      gst_element_seek (self->priv->playbin,
                          self->priv->rate,
                          GST_FORMAT_TIME,
                          GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_ACCURATE,
                          GST_SEEK_TYPE_SET,
                          start_pos,
                          GST_SEEK_TYPE_NONE,
                          position);

  if (!ret)
    emit_error (self, g_error_new (GST_PLAYER_ERROR, GST_PLAYER_ERROR_FAILED,
            "Failed to seek to %" GST_TIME_FORMAT, GST_TIME_ARGS (position)));

  g_mutex_lock (&self->priv->lock);
}

static gboolean
gst_player_seek_internal (gpointer user_data)
{
  GstPlayer *self = GST_PLAYER (user_data);

  g_mutex_lock (&self->priv->lock);
  gst_player_seek_internal_locked (self);
  g_mutex_unlock (&self->priv->lock);

  return G_SOURCE_REMOVE;
}

void
gst_player_seek (GstPlayer * self, GstClockTime position)
{
  g_return_if_fail (GST_IS_PLAYER (self));
  g_return_if_fail (GST_CLOCK_TIME_IS_VALID (position));

  g_mutex_lock (&self->priv->lock);
  self->priv->seek_position = position;

  /* If there is no seek being dispatch to the main context currently do that,
   * otherwise we just updated the seek position so that it will be taken by
   * the seek handler from the main context instead of the old one.
   */
  if (!self->priv->seek_source) {
    GstClockTime now = gst_util_get_timestamp ();

    /* If no seek is pending or it was started more than 250 mseconds ago seek
     * immediately, otherwise wait until the 250 mseconds have passed */
    if (!self->priv->seek_pending
        || (now - self->priv->last_seek_time > 250 * GST_MSECOND)) {
      self->priv->seek_source = g_idle_source_new ();
      g_source_set_callback (self->priv->seek_source,
          (GSourceFunc) gst_player_seek_internal, self, NULL);
      GST_TRACE_OBJECT (self, "Dispatching seek to position %" GST_TIME_FORMAT,
          GST_TIME_ARGS (position));
      g_source_attach (self->priv->seek_source, self->priv->context);
    } else {
      guint delay = 250000 - (now - self->priv->last_seek_time) / 1000;

      /* Note that last_seek_time must be set to something at this point and
       * it must be smaller than 250 mseconds */
      self->priv->seek_source = g_timeout_source_new (delay);
      g_source_set_callback (self->priv->seek_source,
          (GSourceFunc) gst_player_seek_internal, self, NULL);

      GST_TRACE_OBJECT (self,
          "Delaying seek to position %" GST_TIME_FORMAT " by %u us",
          GST_TIME_ARGS (position), delay);
      g_source_attach (self->priv->seek_source, self->priv->context);
    }
  }
  g_mutex_unlock (&self->priv->lock);
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
