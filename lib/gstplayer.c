/* GStreamer
 *
 * Copyright (C) 2014 Sebastian Dröge <sebastian@centricular.com>
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

/* TODO:
 * - Media info, tags
 * - Audio track selection
 * - Subtitle track selection, external subs, disable
 * - Visualization
 * - Playback rate
 *
 */

#include "gstplayer.h"

#include <gst/video/video.h>

enum
{
  PROP_0,
  PROP_DISPATCH_TO_MAIN_CONTEXT,
  PROP_URI,
  PROP_PLAYING,
  PROP_POSITION,
  PROP_DURATION,
  PROP_VOLUME,
  PROP_MUTE,
  PROP_WINDOW_HANDLE,
  PROP_LAST
};

enum
{
  SIGNAL_POSITION_UPDATED,
  SIGNAL_DURATION_CHANGED,
  SIGNAL_END_OF_STREAM,
  SIGNAL_SEEK_FINISHED,
  SIGNAL_ERROR,
  SIGNAL_VIDEO_DIMENSIONS_CHANGED,
  SIGNAL_LAST
};

struct _GstPlayer
{
  GObject parent;

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
  GstState target_state;
  gboolean is_live;
  gboolean seek_pending;
};

struct _GstPlayerClass
{
  GObjectClass parent_class;
};

#define parent_class gst_player_parent_class
G_DEFINE_TYPE (GstPlayer, gst_player, G_TYPE_OBJECT);

static guint signals[SIGNAL_LAST] = { 0, };
static GParamSpec *param_specs[PROP_LAST] = { NULL, };

static void gst_player_finalize (GObject * object);
static void gst_player_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gst_player_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec);

static gpointer gst_player_main (gpointer data);

static void
gst_player_init (GstPlayer * self)
{
  g_mutex_init (&self->lock);
  g_cond_init (&self->cond);

  g_mutex_lock (&self->lock);
  self->thread = g_thread_new ("GstPlayer", gst_player_main, self);
  while (!self->loop || !g_main_loop_is_running (self->loop))
    g_cond_wait (&self->cond, &self->lock);
  g_mutex_unlock (&self->lock);
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
      FALSE,
      G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_URI] = g_param_spec_string ("uri", "URI", "Current URI",
      NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_PLAYING] =
      g_param_spec_boolean ("playing", "Playing", "Currently playing",
      FALSE, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

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

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);

  signals[SIGNAL_POSITION_UPDATED] =
      g_signal_new ("position-updated", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 1, GST_TYPE_CLOCK_TIME);

  signals[SIGNAL_DURATION_CHANGED] =
      g_signal_new ("duration-changed", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 1, GST_TYPE_CLOCK_TIME);

  signals[SIGNAL_END_OF_STREAM] =
      g_signal_new ("end-of-stream", G_TYPE_FROM_CLASS (klass),
      G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS, 0, NULL,
      NULL, NULL, G_TYPE_NONE, 0, G_TYPE_INVALID);

  signals[SIGNAL_SEEK_FINISHED] =
      g_signal_new ("seek-finished", G_TYPE_FROM_CLASS (klass),
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

  g_main_loop_quit (self->loop);
  g_thread_join (self->thread);

  g_free (self->uri);
  if (self->application_context)
    g_main_context_unref (self->application_context);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

typedef struct
{
  GstPlayer *player;
  gchar *uri;
} SetUriData;

static gboolean
gst_player_set_uri (gpointer user_data)
{
  SetUriData *data = user_data;
  GstPlayer *self = data->player;
  gchar *uri = data->uri;

  g_free (self->uri);
  self->uri = uri;

  gst_element_set_state (self->playbin, GST_STATE_READY);
  g_object_set (self->playbin, "uri", uri, NULL);

  g_slice_free (SetUriData, data);

  return FALSE;
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
      SetUriData *data = g_slice_new (SetUriData);
      data->player = self;
      data->uri = g_value_dup_string (value);
      g_main_context_invoke (self->context, gst_player_set_uri, data);
      break;
    }
    case PROP_VOLUME:
      g_object_set_property (G_OBJECT (self->playbin), "volume", value);
      break;
    case PROP_MUTE:
      g_object_set_property (G_OBJECT (self->playbin), "mute", value);
      break;
    case PROP_WINDOW_HANDLE:
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
      g_value_set_string (value, self->uri);
      break;
    case PROP_PLAYING:
      g_value_set_boolean (value, self->target_state == GST_STATE_PLAYING);
      break;
    case PROP_POSITION:{
      gint64 position;

      gst_element_query_position (self->playbin, GST_FORMAT_TIME, &position);
      g_value_set_uint64 (value, position);
      break;
    }
    case PROP_DURATION:{
      gint64 duration;

      gst_element_query_duration (self->playbin, GST_FORMAT_TIME, &duration);
      g_value_set_uint64 (value, duration);
      break;
    }
    case PROP_VOLUME:
      g_object_get_property (G_OBJECT (self->playbin), "volume", value);
      break;
    case PROP_MUTE:
      g_object_get_property (G_OBJECT (self->playbin), "mute", value);
      break;
    case PROP_WINDOW_HANDLE:
      g_value_set_pointer (value, (gpointer) self->window_handle);
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

  g_mutex_lock (&self->lock);
  g_cond_signal (&self->cond);
  g_mutex_unlock (&self->lock);

  return FALSE;
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

  return FALSE;
}

static gboolean
tick_cb (gpointer user_data)
{
  GstPlayer *self = GST_PLAYER (user_data);
  gint64 position;

  if (gst_element_query_position (self->playbin, GST_FORMAT_TIME, &position)) {
    if (self->dispatch_to_main_context) {
      PositionUpdatedSignalData *data = g_slice_new (PositionUpdatedSignalData);

      data->player = self;
      data->position = position;
      g_main_context_invoke (self->application_context,
          position_updated_dispatch, data);
    } else {
      g_signal_emit (self, signals[SIGNAL_POSITION_UPDATED], 0, position);
      g_object_notify_by_pspec (G_OBJECT (self), param_specs[PROP_POSITION]);
    }
  }

  return TRUE;
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
  g_clear_error (&data->err);
  g_slice_free (ErrorSignalData, data);

  return FALSE;
}

static void
emit_error (GstPlayer * self, GError * err)
{
  if (self->dispatch_to_main_context) {
    ErrorSignalData *data = g_slice_new (ErrorSignalData);

    data->player = self;
    // FIXME
    data->err = err ? g_error_copy (err) : NULL;
    g_main_context_invoke (self->application_context, error_dispatch, data);
  } else {
    g_signal_emit (self, signals[SIGNAL_ERROR], 0, err);
  }
}

static void
error_cb (GstBus * bus, GstMessage * msg, gpointer user_data)
{
  GstPlayer *self = GST_PLAYER (user_data);
  GError *err;

  gst_message_parse_error (msg, &err, NULL);
  emit_error (self, err);
  g_clear_error (&err);

  self->target_state = GST_STATE_NULL;
  gst_element_set_state (self->playbin, GST_STATE_NULL);
  self->seek_pending = FALSE;
}

static gboolean
eos_dispatch (gpointer user_data)
{
  g_signal_emit (user_data, signals[SIGNAL_END_OF_STREAM], 0);

  return FALSE;
}

static void
eos_cb (GstBus * bus, GstMessage * msg, gpointer user_data)
{
  GstPlayer *self = GST_PLAYER (user_data);

  tick_cb (self);

  if (self->dispatch_to_main_context) {
    g_main_context_invoke (self->application_context, eos_dispatch, self);
  } else {
    g_signal_emit (self, signals[SIGNAL_END_OF_STREAM], 0);
  }
}

static void
buffering_cb (GstBus * bus, GstMessage * msg, gpointer user_data)
{
  GstPlayer *self = GST_PLAYER (user_data);
  gint percent;

  if (self->is_live)
    return;

  gst_message_parse_buffering (msg, &percent);
  if (percent < 100 && self->target_state >= GST_STATE_PAUSED) {
    gst_element_set_state (self->playbin, GST_STATE_PAUSED);
  } else if (self->target_state >= GST_STATE_PLAYING) {
    gst_element_set_state (self->playbin, GST_STATE_PLAYING);
  } else if (self->target_state >= GST_STATE_PAUSED) {
  }
}

static void
clock_lost_cb (GstBus * bus, GstMessage * msg, gpointer user_data)
{
  GstPlayer *self = GST_PLAYER (user_data);

  if (self->target_state >= GST_STATE_PLAYING) {
    gst_element_set_state (self->playbin, GST_STATE_PAUSED);
    gst_element_set_state (self->playbin, GST_STATE_PLAYING);
  }
}

static gboolean
seek_finished_dispatch (gpointer user_data)
{
  g_signal_emit (user_data, signals[SIGNAL_SEEK_FINISHED], 0);

  return FALSE;
}

static void
async_done_cb (GstBus * bus, GstMessage * msg, gpointer user_data)
{
  GstPlayer *self = GST_PLAYER (user_data);

  if (GST_MESSAGE_SRC (msg) == GST_OBJECT_CAST (self->playbin)
      && self->seek_pending) {
    if (self->dispatch_to_main_context) {
      g_main_context_invoke (self->application_context, seek_finished_dispatch,
          self);
    } else {
      g_signal_emit (self, signals[SIGNAL_SEEK_FINISHED], 0);
    }
    self->seek_pending = FALSE;
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
  g_slice_free (VideoDimensionsChangedSignalData, data);

  return FALSE;
}

static void
check_video_dimensions_changed (GstPlayer * self)
{
  GstElement *video_sink;
  GstPad *video_sink_pad;
  GstCaps *caps;
  GstVideoInfo info;

  g_object_get (self->playbin, "video-sink", &video_sink, NULL);
  if (!video_sink)
    return;

  video_sink_pad = gst_element_get_static_pad (video_sink, "sink");
  if (!video_sink_pad) {
    gst_object_unref (video_sink);
    return;
  }

  caps = gst_pad_get_current_caps (video_sink_pad);

  if (caps) {
    if (gst_video_info_from_caps (&info, caps)) {
      info.width = info.width * info.par_n / info.par_d;

      if (self->dispatch_to_main_context) {
        VideoDimensionsChangedSignalData *data =
            g_slice_new (VideoDimensionsChangedSignalData);

        data->player = self;
        data->width = info.width;
        data->height = info.height;
        g_main_context_invoke (self->application_context,
            video_dimensions_changed_dispatch, data);
      } else {
        g_signal_emit (self, signals[SIGNAL_VIDEO_DIMENSIONS_CHANGED], 0,
            info.width, info.height);
      }
    }

    gst_caps_unref (caps);
  }
  gst_object_unref (video_sink_pad);
  gst_object_unref (video_sink);
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
  g_slice_free (DurationChangedSignalData, data);

  return FALSE;
}

static void
emit_duration_changed (GstPlayer * self, GstClockTime duration)
{
  if (self->dispatch_to_main_context) {
    DurationChangedSignalData *data = g_slice_new (DurationChangedSignalData);

    data->player = self;
    data->duration = duration;
    g_main_context_invoke (self->application_context, duration_changed_dispatch,
        data);
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
    if (old_state == GST_STATE_READY && new_state == GST_STATE_PAUSED) {
      GstElement *video_sink;
      GstPad *video_sink_pad;
      gint64 duration = -1;

      g_object_get (self->playbin, "video-sink", &video_sink, NULL);

      if (video_sink) {
        video_sink_pad = gst_element_get_static_pad (video_sink, "sink");

        if (video_sink_pad) {
          check_video_dimensions_changed (self);
          g_signal_connect (video_sink_pad, "notify::caps",
              (GCallback) notify_caps_cb, self);
          gst_object_unref (video_sink_pad);
        }
        gst_object_unref (video_sink);
      }

      gst_element_query_duration (self->playbin, GST_FORMAT_TIME, &duration);
      emit_duration_changed (self, duration);
      tick_cb (self);
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

  gst_bin_recalculate_latency (GST_BIN (self->playbin));
}

static void
request_state_cb (GstBus * bus, GstMessage * msg, gpointer user_data)
{
  GstPlayer *self = GST_PLAYER (user_data);
  GstState state;

  gst_message_parse_request_state (msg, &state);

  gst_element_set_state (self->playbin, state);
}

static gpointer
gst_player_main (gpointer data)
{
  GstPlayer *self = GST_PLAYER (data);
  GstBus *bus;
  GSource *source;
  GSource *tick_source, *bus_source;

  self->context = g_main_context_new ();
  g_main_context_push_thread_default (self->context);

  tick_source = g_timeout_source_new (100);
  g_source_set_callback (tick_source, (GSourceFunc) tick_cb, self, NULL);
  g_source_attach (tick_source, self->context);

  self->loop = g_main_loop_new (self->context, FALSE);

  source = g_idle_source_new ();
  g_source_set_callback (source, (GSourceFunc) main_loop_running_cb, self,
      NULL);
  g_source_attach (source, self->context);
  g_source_unref (source);

  self->playbin = gst_element_factory_make ("playbin", "playbin");

  bus = gst_element_get_bus (self->playbin);
  bus_source = gst_bus_create_watch (bus);
  g_source_set_callback (bus_source, (GSourceFunc) gst_bus_async_signal_func,
      NULL, NULL);
  g_source_attach (bus_source, self->context);

  g_signal_connect (G_OBJECT (bus), "message::error", G_CALLBACK (error_cb),
      self);
  g_signal_connect (G_OBJECT (bus), "message::eos", G_CALLBACK (eos_cb), self);
  g_signal_connect (G_OBJECT (bus), "message::state-changed",
      G_CALLBACK (state_changed_cb), self);
  g_signal_connect (G_OBJECT (bus), "message::async-done",
      G_CALLBACK (async_done_cb), self);
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

  self->target_state = GST_STATE_NULL;

  g_main_loop_run (self->loop);

  g_main_loop_unref (self->loop);
  self->loop = NULL;

  g_source_destroy (bus_source);
  g_source_unref (bus_source);

  g_source_destroy (tick_source);
  g_source_unref (tick_source);

  g_main_context_pop_thread_default (self->context);
  g_main_context_unref (self->context);
  self->context = NULL;

  self->target_state = GST_STATE_NULL;
  if (self->playbin) {
    gst_element_set_state (self->playbin, GST_STATE_NULL);
    gst_object_unref (self->playbin);
    self->playbin = NULL;
  }

  return NULL;
}

GstPlayer *
gst_player_new (gboolean dispatch_to_main_context)
{
  return g_object_new (GST_TYPE_PLAYER, "dispatch-to-main-context",
      dispatch_to_main_context, NULL);
}

static gboolean
gst_player_play_internal (gpointer user_data)
{
  GstPlayer *self = GST_PLAYER (user_data);
  GstStateChangeReturn state_ret;

  state_ret = gst_element_set_state (self->playbin, GST_STATE_PLAYING);
  if (state_ret == GST_STATE_CHANGE_FAILURE) {
    // FIXME
    emit_error (self, NULL);
  } else if (state_ret == GST_STATE_CHANGE_NO_PREROLL) {
    self->is_live = TRUE;
  }

  return FALSE;
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

  state_ret = gst_element_set_state (self->playbin, GST_STATE_PAUSED);
  if (state_ret == GST_STATE_CHANGE_FAILURE) {
    // FIXME
    emit_error (self, NULL);
  } else if (state_ret == GST_STATE_CHANGE_NO_PREROLL) {
    self->is_live = TRUE;
  }

  return FALSE;
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

  gst_element_set_state (self->playbin, GST_STATE_READY);
  self->seek_pending = FALSE;

  return FALSE;
}

void
gst_player_stop (GstPlayer * self)
{
  g_return_if_fail (GST_IS_PLAYER (self));

  g_main_context_invoke (self->context, gst_player_stop_internal, self);
}

typedef struct
{
  GstPlayer *player;
  GstClockTime position;
} SeekData;

static gboolean
gst_player_seek_internal (gpointer user_data)
{
  SeekData *data = user_data;
  GstPlayer *self = GST_PLAYER (data->player);
  GstClockTime position = data->position;
  gboolean ret;

  self->seek_pending = TRUE;
  ret =
      gst_element_seek_simple (self->playbin, GST_FORMAT_TIME,
      GST_SEEK_FLAG_FLUSH, position);

  if (!ret) {
    // FIXME
    emit_error (self, NULL);
  }

  g_slice_free (SeekData, data);

  return FALSE;
}

void
gst_player_seek (GstPlayer * self, GstClockTime position)
{
  SeekData *data;

  g_return_if_fail (GST_IS_PLAYER (self));

  data = g_slice_new (SeekData);

  data->player = self;
  data->position = position;
  g_main_context_invoke (self->context, gst_player_seek_internal, data);
}
