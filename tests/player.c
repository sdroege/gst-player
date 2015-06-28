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

/* TODO:
 * - start with pause, go to playing
 * - play, pause, play
 * - set uri in play/pause
 * - play/pause after eos
 * - seek in play/pause/stopped, after eos, back to 0, after duration
 * - http buffering
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <check.h>

#define fail_unless_equals_int(a, b)                                    \
G_STMT_START {                                                          \
  int first = a;                                                        \
  int second = b;                                                       \
  fail_unless(first == second,                                          \
    "'" #a "' (%d) is not equal to '" #b"' (%d)", first, second);       \
} G_STMT_END;

#define fail_unless_equals_uint64(a, b)                                 \
G_STMT_START {                                                          \
  guint64 first = a;                                                    \
  guint64 second = b;                                                   \
  fail_unless(first == second,                                          \
    "'" #a "' (%" G_GUINT64_FORMAT ") is not equal to '" #b"' (%"       \
    G_GUINT64_FORMAT ")", first, second);                               \
} G_STMT_END;

#define fail_unless_equals_string(a, b)                                 \
G_STMT_START {                                                          \
  const gchar *first = a;                                               \
  const gchar *second = b;                                              \
  fail_unless(strcmp(first, second) == 0,                               \
    "'" #a "' (%s) is not equal to '" #b "' (%s)", first, second);      \
} G_STMT_END;

#include <gst/player/gstplayer.h>

GST_DEBUG_CATEGORY_STATIC (test_debug);
#define GST_CAT_DEFAULT test_debug

START_TEST (test_create_and_free)
{
  GstPlayer *player;

  player = gst_player_new ();
  fail_unless (player != NULL);
  g_object_unref (player);
}

END_TEST;

START_TEST (test_set_and_get_uri)
{
  GstPlayer *player;
  gchar *uri;

  player = gst_player_new ();

  fail_unless (player != NULL);

  gst_player_set_uri (player, "file:///path/to/a/file");
  uri = gst_player_get_uri (player);

  fail_unless (g_strcmp0 (uri, "file:///path/to/a/file") == 0);

  g_free (uri);
  g_object_unref (player);
}

END_TEST;

typedef enum
{
  STATE_CHANGE_BUFFERING,
  STATE_CHANGE_DURATION_CHANGED,
  STATE_CHANGE_END_OF_STREAM,
  STATE_CHANGE_ERROR,
  STATE_CHANGE_WARNING,
  STATE_CHANGE_POSITION_UPDATED,
  STATE_CHANGE_STATE_CHANGED,
  STATE_CHANGE_VIDEO_DIMENSIONS_CHANGED,
  STATE_CHANGE_MEDIA_INFO_UPDATED,
} TestPlayerStateChange;

static const gchar *
test_player_state_change_get_name (TestPlayerStateChange change)
{
  switch (change) {
    case STATE_CHANGE_BUFFERING:
      return "buffering";
    case STATE_CHANGE_DURATION_CHANGED:
      return "duration-changed";
    case STATE_CHANGE_END_OF_STREAM:
      return "end-of-stream";
    case STATE_CHANGE_ERROR:
      return "error";
    case STATE_CHANGE_POSITION_UPDATED:
      return "position-updated";
    case STATE_CHANGE_STATE_CHANGED:
      return "state-changed";
    case STATE_CHANGE_VIDEO_DIMENSIONS_CHANGED:
      return "video-dimensions-changed";
    case STATE_CHANGE_MEDIA_INFO_UPDATED:
      return "media-info-updated";
    default:
      g_assert_not_reached ();
      break;
  }
}

typedef struct _TestPlayerState TestPlayerState;
struct _TestPlayerState
{
  GMainLoop *loop;

  gint buffering_percent;
  guint64 position, duration;
  gboolean end_of_stream, error, warning;
  GstPlayerState state;
  gint width, height;
  GstPlayerMediaInfo *media_info;

  void (*test_callback) (GstPlayer * player, TestPlayerStateChange change,
      TestPlayerState * old_state, TestPlayerState * new_state);
  gpointer test_data;
};

static void
test_player_state_change_debug (GstPlayer * player,
    TestPlayerStateChange change, TestPlayerState * old_state,
    TestPlayerState * new_state)
{
  GST_DEBUG_OBJECT (player, "Changed %s:\n"
      "\tbuffering %d%% -> %d%%\n"
      "\tposition %" GST_TIME_FORMAT " -> %" GST_TIME_FORMAT "\n"
      "\tduration %" GST_TIME_FORMAT " -> %" GST_TIME_FORMAT "\n"
      "\tend-of-stream %d -> %d\n"
      "\terror %d -> %d\n"
      "\tstate %s -> %s\n"
      "\twidth/height %d/%d -> %d/%d\n"
      "\tmedia_info %p -> %p",
      test_player_state_change_get_name (change),
      old_state->buffering_percent, new_state->buffering_percent,
      GST_TIME_ARGS (old_state->position), GST_TIME_ARGS (new_state->position),
      GST_TIME_ARGS (old_state->duration), GST_TIME_ARGS (new_state->duration),
      old_state->end_of_stream, new_state->end_of_stream,
      old_state->error, new_state->error,
      gst_player_state_get_name (old_state->state),
      gst_player_state_get_name (new_state->state), old_state->width,
      old_state->height, new_state->width, new_state->height,
      old_state->media_info, new_state->media_info);
}

static void
test_player_state_reset (TestPlayerState * state)
{
  state->buffering_percent = 100;
  state->position = state->duration = -1;
  state->end_of_stream = state->error = FALSE;
  state->state = GST_PLAYER_STATE_STOPPED;
  state->width = state->height = 0;
  state->media_info = NULL;
}

static void
buffering_cb (GstPlayer * player, gint percent, TestPlayerState * state)
{
  TestPlayerState old_state = *state;

  state->buffering_percent = percent;
  test_player_state_change_debug (player, STATE_CHANGE_BUFFERING, &old_state,
      state);
  state->test_callback (player, STATE_CHANGE_BUFFERING, &old_state, state);
}

static void
duration_changed_cb (GstPlayer * player, guint64 duration,
    TestPlayerState * state)
{
  TestPlayerState old_state = *state;

  state->duration = duration;
  test_player_state_change_debug (player, STATE_CHANGE_DURATION_CHANGED,
      &old_state, state);
  state->test_callback (player, STATE_CHANGE_DURATION_CHANGED, &old_state,
      state);
}

static void
end_of_stream_cb (GstPlayer * player, TestPlayerState * state)
{
  TestPlayerState old_state = *state;

  state->end_of_stream = TRUE;
  test_player_state_change_debug (player, STATE_CHANGE_END_OF_STREAM,
      &old_state, state);
  state->test_callback (player, STATE_CHANGE_END_OF_STREAM, &old_state, state);
}

static void
error_cb (GstPlayer * player, GError * error, TestPlayerState * state)
{
  TestPlayerState old_state = *state;

  state->error = TRUE;
  test_player_state_change_debug (player, STATE_CHANGE_ERROR, &old_state,
      state);
  state->test_callback (player, STATE_CHANGE_ERROR, &old_state, state);
}

static void
warning_cb (GstPlayer * player, GError * error, TestPlayerState * state)
{
  TestPlayerState old_state = *state;

  state->warning = TRUE;
  test_player_state_change_debug (player, STATE_CHANGE_WARNING, &old_state,
      state);
  state->test_callback (player, STATE_CHANGE_WARNING, &old_state, state);
}

static void
position_updated_cb (GstPlayer * player, guint64 position,
    TestPlayerState * state)
{
  TestPlayerState old_state = *state;

  state->position = position;
  test_player_state_change_debug (player, STATE_CHANGE_POSITION_UPDATED,
      &old_state, state);
  state->test_callback (player, STATE_CHANGE_POSITION_UPDATED, &old_state,
      state);
}

static void
media_info_updated_cb (GstPlayer * player, GstPlayerMediaInfo * media_info,
    TestPlayerState * state)
{
  TestPlayerState old_state = *state;

  state->media_info = media_info;

  test_player_state_change_debug (player, STATE_CHANGE_MEDIA_INFO_UPDATED,
      &old_state, state);
  state->test_callback (player, STATE_CHANGE_MEDIA_INFO_UPDATED, &old_state,
      state);
}

static void
state_changed_cb (GstPlayer * player, GstPlayerState player_state,
    TestPlayerState * state)
{
  TestPlayerState old_state = *state;

  state->state = player_state;

  if (player_state == GST_PLAYER_STATE_STOPPED)
    test_player_state_reset (state);

  test_player_state_change_debug (player, STATE_CHANGE_STATE_CHANGED,
      &old_state, state);
  state->test_callback (player, STATE_CHANGE_STATE_CHANGED, &old_state, state);
}

static void
video_dimensions_changed_cb (GstPlayer * player, gint width, gint height,
    TestPlayerState * state)
{
  TestPlayerState old_state = *state;

  state->width = width;
  state->height = height;
  test_player_state_change_debug (player, STATE_CHANGE_VIDEO_DIMENSIONS_CHANGED,
      &old_state, state);
  state->test_callback (player, STATE_CHANGE_VIDEO_DIMENSIONS_CHANGED,
      &old_state, state);
}

static GstPlayer *
test_player_new (TestPlayerState * state)
{
  GstPlayer *player;
  GstElement *playbin, *fakesink;

  player = gst_player_new ();
  fail_unless (player != NULL);

  test_player_state_reset (state);

  playbin = gst_player_get_pipeline (player);
  fakesink = gst_element_factory_make ("fakesink", "audio-sink");
  g_object_set (fakesink, "sync", TRUE, NULL);
  g_object_set (playbin, "audio-sink", fakesink, NULL);
  fakesink = gst_element_factory_make ("fakesink", "video-sink");
  g_object_set (fakesink, "sync", TRUE, NULL);
  g_object_set (playbin, "video-sink", fakesink, NULL);
  gst_object_unref (playbin);

  g_object_set (player, "dispatch-to-main-context", TRUE, NULL);
  g_signal_connect (player, "buffering", G_CALLBACK (buffering_cb), state);
  g_signal_connect (player, "duration-changed",
      G_CALLBACK (duration_changed_cb), state);
  g_signal_connect (player, "end-of-stream", G_CALLBACK (end_of_stream_cb),
      state);
  g_signal_connect (player, "error", G_CALLBACK (error_cb), state);
  g_signal_connect (player, "warning", G_CALLBACK (warning_cb), state);
  g_signal_connect (player, "position-updated",
      G_CALLBACK (position_updated_cb), state);
  g_signal_connect (player, "state-changed", G_CALLBACK (state_changed_cb),
      state);
  g_signal_connect (player, "media-info-updated",
      G_CALLBACK (media_info_updated_cb), state);
  g_signal_connect (player, "video-dimensions-changed",
      G_CALLBACK (video_dimensions_changed_cb), state);

  return player;
}

static void
test_play_audio_video_eos_cb (GstPlayer * player, TestPlayerStateChange change,
    TestPlayerState * old_state, TestPlayerState * new_state)
{
  gint step = GPOINTER_TO_INT (new_state->test_data);
  gboolean video;

  video = ! !(step & 0x10);
  step = (step & (~0x10));

  switch (step) {
    case 0:
      fail_unless_equals_int (change, STATE_CHANGE_STATE_CHANGED);
      fail_unless_equals_int (old_state->state, GST_PLAYER_STATE_STOPPED);
      fail_unless_equals_int (new_state->state, GST_PLAYER_STATE_BUFFERING);
      new_state->test_data =
          GINT_TO_POINTER ((video ? 0x10 : 0x00) | (step + 1));
      break;
    case 1:
      fail_unless_equals_int (change, STATE_CHANGE_MEDIA_INFO_UPDATED);
      new_state->test_data =
          GINT_TO_POINTER ((video ? 0x10 : 0x00) | (step + 1));
      break;
    case 2:
      fail_unless_equals_int (change, STATE_CHANGE_VIDEO_DIMENSIONS_CHANGED);
      if (video) {
        fail_unless_equals_int (new_state->width, 320);
        fail_unless_equals_int (new_state->height, 240);
      } else {
        fail_unless_equals_int (new_state->width, 0);
        fail_unless_equals_int (new_state->height, 0);
      }
      new_state->test_data =
          GINT_TO_POINTER ((video ? 0x10 : 0x00) | (step + 1));
      break;
    case 3:
      fail_unless_equals_int (change, STATE_CHANGE_DURATION_CHANGED);
      fail_unless_equals_uint64 (new_state->duration,
          G_GUINT64_CONSTANT (464399092));
      new_state->test_data =
          GINT_TO_POINTER ((video ? 0x10 : 0x00) | (step + 1));
      break;
    case 4:
      fail_unless_equals_int (change, STATE_CHANGE_POSITION_UPDATED);
      fail_unless_equals_uint64 (new_state->position, G_GUINT64_CONSTANT (0));
      new_state->test_data =
          GINT_TO_POINTER ((video ? 0x10 : 0x00) | (step + 1));
      break;
    case 5:
      fail_unless_equals_int (change, STATE_CHANGE_STATE_CHANGED);
      fail_unless_equals_int (old_state->state, GST_PLAYER_STATE_BUFFERING);
      fail_unless_equals_int (new_state->state, GST_PLAYER_STATE_PLAYING);
      new_state->test_data =
          GINT_TO_POINTER ((video ? 0x10 : 0x00) | (step + 1));
      break;
    case 6:
      if (change == STATE_CHANGE_POSITION_UPDATED) {
        fail_unless (old_state->position <= new_state->position);
      } else {
        fail_unless_equals_uint64 (old_state->position, old_state->duration);
        fail_unless_equals_int (change, STATE_CHANGE_END_OF_STREAM);
        new_state->test_data =
            GINT_TO_POINTER ((video ? 0x10 : 0x00) | (step + 1));
      }
      break;
    case 7:
      fail_unless_equals_int (change, STATE_CHANGE_STATE_CHANGED);
      fail_unless_equals_int (old_state->state, GST_PLAYER_STATE_PLAYING);
      fail_unless_equals_int (new_state->state, GST_PLAYER_STATE_STOPPED);
      new_state->test_data =
          GINT_TO_POINTER ((video ? 0x10 : 0x00) | (step + 1));
      g_main_loop_quit (new_state->loop);
      break;
    default:
      fail ();
      break;
  }
}

START_TEST (test_play_audio_eos)
{
  GstPlayer *player;
  TestPlayerState state;
  gchar *uri;

  memset (&state, 0, sizeof (state));
  state.loop = g_main_loop_new (NULL, FALSE);
  state.test_callback = test_play_audio_video_eos_cb;
  state.test_data = GINT_TO_POINTER (0);

  player = test_player_new (&state);

  fail_unless (player != NULL);

  uri = gst_filename_to_uri (TEST_PATH "/audio-short.ogg", NULL);
  fail_unless (uri != NULL);
  gst_player_set_uri (player, uri);
  g_free (uri);

  gst_player_play (player);
  g_main_loop_run (state.loop);

  fail_unless_equals_int (GPOINTER_TO_INT (state.test_data), 8);

  g_object_unref (player);
  g_main_loop_unref (state.loop);
}

END_TEST;

static void
test_audio_info (GstPlayerMediaInfo * media_info)
{
  gint i = 0;
  GList *list;

  for (list = gst_player_get_audio_streams (media_info);
      list != NULL; list = list->next) {
    GstPlayerStreamInfo *stream = (GstPlayerStreamInfo *) list->data;
    GstPlayerAudioInfo *audio_info = (GstPlayerAudioInfo *) stream;

    fail_unless (gst_player_stream_info_get_tags (stream) != NULL);
    fail_unless (gst_player_stream_info_get_caps (stream) != NULL);
    fail_unless_equals_string (gst_player_stream_info_get_stream_type (stream),
        "audio");

    if (i == 0) {
      fail_unless_equals_string (gst_player_stream_info_get_codec (stream),
          "MPEG-1 Layer 3 (MP3)");
      fail_unless_equals_int (gst_player_audio_info_get_sample_rate
          (audio_info), 48000);
      fail_unless_equals_int (gst_player_audio_info_get_channels (audio_info),
          2);
      fail_unless_equals_int (gst_player_audio_info_get_max_bitrate
          (audio_info), 192000);
      fail_unless (gst_player_audio_info_get_language (audio_info) != NULL);
    } else {
      fail_unless_equals_string (gst_player_stream_info_get_codec (stream),
          "MPEG-4 AAC");
      fail_unless_equals_int (gst_player_audio_info_get_sample_rate
          (audio_info), 48000);
      fail_unless_equals_int (gst_player_audio_info_get_channels (audio_info),
          6);
      fail_unless (gst_player_audio_info_get_language (audio_info) != NULL);
    }

    i++;
  }
}

static void
test_video_info (GstPlayerMediaInfo * media_info)
{
  GList *list;

  for (list = gst_player_get_video_streams (media_info);
      list != NULL; list = list->next) {
    gint fps_d, fps_n;
    guint par_d, par_n;
    GstPlayerStreamInfo *stream = (GstPlayerStreamInfo *) list->data;
    GstPlayerVideoInfo *video_info = (GstPlayerVideoInfo *) stream;

    fail_unless (gst_player_stream_info_get_tags (stream) != NULL);
    fail_unless (gst_player_stream_info_get_caps (stream) != NULL);
    fail_unless_equals_int (gst_player_stream_info_get_index (stream), 0);
    fail_unless (strstr (gst_player_stream_info_get_codec (stream),
            "H.264") != NULL);
    fail_unless_equals_int (gst_player_video_info_get_width (video_info), 320);
    fail_unless_equals_int (gst_player_video_info_get_height (video_info), 240);
    gst_player_video_info_get_framerate (video_info, &fps_n, &fps_d);
    fail_unless_equals_int (fps_n, 24);
    fail_unless_equals_int (fps_d, 1);
    gst_player_video_info_get_pixel_aspect_ratio (video_info, &par_n, &par_d);
    fail_unless_equals_int (par_n, 20);
    fail_unless_equals_int (par_d, 33);
  }
}

static void
test_subtitle_info (GstPlayerMediaInfo * media_info)
{
  GList *list;

  for (list = gst_player_get_subtitle_streams (media_info);
      list != NULL; list = list->next) {
    GstPlayerStreamInfo *stream = (GstPlayerStreamInfo *) list->data;
    GstPlayerSubtitleInfo *sub = (GstPlayerSubtitleInfo *) stream;

    fail_unless_equals_string (gst_player_stream_info_get_stream_type (stream),
        "subtitle");
    fail_unless (gst_player_stream_info_get_tags (stream) != NULL);
    fail_unless (gst_player_stream_info_get_caps (stream) != NULL);
    fail_unless_equals_string (gst_player_stream_info_get_codec (stream),
        "Timed Text");
    fail_unless (gst_player_subtitle_info_get_language (sub) != NULL);
  }
}

static void
test_media_info_object (GstPlayer * player, GstPlayerMediaInfo * media_info)
{
  GList *list;

  /* gloabl tag */
  fail_unless (gst_player_media_info_is_seekable (media_info) == TRUE);
  fail_unless (gst_player_media_info_get_tags (media_info) != NULL);
  fail_unless_equals_string (gst_player_media_info_get_title (media_info),
      "Sintel");
  fail_unless_equals_string (gst_player_media_info_get_container_format
      (media_info), "Matroska");
  fail_unless (gst_player_media_info_get_image_sample (media_info) == NULL);
  fail_unless (strstr (gst_player_media_info_get_uri (media_info),
          "sintel.mkv") != NULL);

  /* number of streams */
  list = gst_player_media_info_get_stream_list (media_info);
  fail_unless (list != NULL);
  fail_unless_equals_int (g_list_length (list), 10);

  list = gst_player_get_video_streams (media_info);
  fail_unless (list != NULL);
  fail_unless_equals_int (g_list_length (list), 1);

  list = gst_player_get_audio_streams (media_info);
  fail_unless (list != NULL);
  fail_unless_equals_int (g_list_length (list), 2);

  list = gst_player_get_subtitle_streams (media_info);
  fail_unless (list != NULL);
  fail_unless_equals_int (g_list_length (list), 7);

  /* test subtitle */
  test_subtitle_info (media_info);

  /* test audio */
  test_audio_info (media_info);

  /* test video */
  test_video_info (media_info);
}

static void
test_play_media_info_cb (GstPlayer * player, TestPlayerStateChange change,
    TestPlayerState * old_state, TestPlayerState * new_state)
{
  gint completed = GPOINTER_TO_INT (new_state->test_data);

  if (change == STATE_CHANGE_MEDIA_INFO_UPDATED) {
    test_media_info_object (player, new_state->media_info);
    new_state->test_data = GINT_TO_POINTER (completed + 1);
    g_main_loop_quit (new_state->loop);
  } else if (change == STATE_CHANGE_END_OF_STREAM ||
      change == STATE_CHANGE_ERROR) {
    g_main_loop_quit (new_state->loop);
  }
}

START_TEST (test_play_media_info)
{
  GstPlayer *player;
  TestPlayerState state;
  gchar *uri;

  memset (&state, 0, sizeof (state));
  state.loop = g_main_loop_new (NULL, FALSE);
  state.test_callback = test_play_media_info_cb;
  state.test_data = GINT_TO_POINTER (0);

  player = test_player_new (&state);

  fail_unless (player != NULL);

  uri = gst_filename_to_uri (TEST_PATH "/sintel.mkv", NULL);
  fail_unless (uri != NULL);
  gst_player_set_uri (player, uri);
  g_free (uri);

  gst_player_play (player);
  g_main_loop_run (state.loop);

  fail_unless_equals_int (GPOINTER_TO_INT (state.test_data), 1);
  g_object_unref (player);
  g_main_loop_unref (state.loop);
}

END_TEST;

typedef struct SwitchStreamArgs
{
  gint index;
  GstPlayer *player;
  GMainLoop *loop;
  gpointer *test_data;
} SwitchStreamArgs;

static gboolean
switch_audio_stream (gpointer user_data)
{
  GstPlayerStreamInfo *new_stream;
  SwitchStreamArgs *args = (SwitchStreamArgs *) user_data;
  gint step = GPOINTER_TO_INT (*args->test_data);

  fail_unless (gst_player_set_audio_track (args->player, args->index) == TRUE);
  new_stream = (GstPlayerStreamInfo *)
      gst_player_get_current_audio_track (args->player);

  fail_unless_equals_int (gst_player_stream_info_get_index (new_stream),
      args->index);

  *(args->test_data) = GINT_TO_POINTER (step + 1);
  return G_SOURCE_REMOVE;
}

static gboolean
switch_subtitle_stream (gpointer user_data)
{
  GstPlayerStreamInfo *new_stream;
  SwitchStreamArgs *args = (SwitchStreamArgs *) user_data;
  gint step = GPOINTER_TO_INT (*(args->test_data));

  fail_unless (gst_player_set_subtitle_track
      (args->player, args->index) == TRUE);
  new_stream = (GstPlayerStreamInfo *)
      gst_player_get_current_subtitle_track (args->player);
  fail_unless_equals_int (gst_player_stream_info_get_index (new_stream),
      args->index);

  *(args->test_data) = GINT_TO_POINTER (step + 1);
  return G_SOURCE_REMOVE;
}

static void
switch_streams (GList * list, SwitchStreamArgs * args)
{
  gint i = 1;

  /* go through the list of available stream index and switch
   * to non active stream index.
   */
  for (; list != NULL; list = list->next) {
    GstPlayerStreamInfo *stream = (GstPlayerStreamInfo *) list->data;

    args->index = gst_player_stream_info_get_index (stream);

    if (!strcmp (gst_player_stream_info_get_stream_type (stream), "audio"))
      g_timeout_add (3000 * i, switch_audio_stream, args);
    else if (!strcmp (gst_player_stream_info_get_stream_type
            (stream), "subtitle"))
      g_timeout_add (3000 * i, switch_subtitle_stream, args);
    i++;
  }
}

static gboolean
switch_streams_completed_loop (gpointer user_data)
{
  SwitchStreamArgs *args = (SwitchStreamArgs *) user_data;
  gint step = GPOINTER_TO_INT (*(args->test_data));

  if (step != 10)
    return G_SOURCE_CONTINUE;

  /* if all the stream switching is completed then quit the loop */
  g_main_loop_quit (args->loop);

  return G_SOURCE_REMOVE;
}

static void
test_play_stream_selection_cb (GstPlayer * player, TestPlayerStateChange change,
    TestPlayerState * old_state, TestPlayerState * new_state)
{
  gint running = GPOINTER_TO_INT (new_state->test_data);

  /* If pipeline is playing and stream switching thread is not started
   * then switch to next available streams */
  if (new_state->state == GST_PLAYER_STATE_PLAYING && !running) {
    GstPlayerMediaInfo *media_info;
    SwitchStreamArgs *args = g_new0 (SwitchStreamArgs, 1);

    args->player = player;
    args->test_data = &new_state->test_data;
    args->loop = new_state->loop;
    new_state->test_data = GINT_TO_POINTER (running + 1);

    media_info = gst_player_get_media_info (player);
    switch_streams (gst_player_get_subtitle_streams (media_info), args);
    switch_streams (gst_player_get_audio_streams (media_info), args);
    g_idle_add_full (G_PRIORITY_DEFAULT_IDLE, switch_streams_completed_loop,
        args, (GDestroyNotify) g_free);

    g_object_unref (media_info);
  } else if (change == STATE_CHANGE_END_OF_STREAM ||
      change == STATE_CHANGE_ERROR) {
    g_main_loop_quit (new_state->loop);
  }
}

START_TEST (test_play_stream_selection)
{
  GstPlayer *player;
  TestPlayerState state;
  gchar *uri;

  memset (&state, 0, sizeof (state));
  state.loop = g_main_loop_new (NULL, FALSE);
  state.test_callback = test_play_stream_selection_cb;
  state.test_data = GINT_TO_POINTER (0);

  player = test_player_new (&state);

  fail_unless (player != NULL);

  uri = gst_filename_to_uri (TEST_PATH "/sintel.mkv", NULL);
  fail_unless (uri != NULL);
  gst_player_set_uri (player, uri);
  g_free (uri);

  gst_player_play (player);
  g_main_loop_run (state.loop);

  fail_unless_equals_int (GPOINTER_TO_INT (state.test_data), 10);

  g_object_unref (player);
  g_main_loop_unref (state.loop);
}

END_TEST;

typedef struct DisableStreamArgs
{
  GstPlayer *player;
  GMainLoop *loop;
  gpointer *test_data;
} DisableStreamArgs;

static gboolean
enable_stream_test (gpointer user_data)
{
  GstPlayerAudioInfo *audio;
  GstPlayerSubtitleInfo *sub;
  GstPlayerVideoInfo *video;
  DisableStreamArgs *args = (DisableStreamArgs *) user_data;

  /* audio should be enabled */
  audio = gst_player_get_current_audio_track (args->player);
  fail_unless (audio != NULL);
  g_object_unref (audio);

  /* subtitle should be enabled */
  sub = gst_player_get_current_subtitle_track (args->player);
  fail_unless (sub != NULL);
  g_object_unref (sub);

  /* video should be enabled */
  video = gst_player_get_current_video_track (args->player);
  fail_unless (video != NULL);
  g_object_unref (video);

  /* we are done with tests */
  g_main_loop_quit (args->loop);

  return G_SOURCE_REMOVE;
}

static gboolean
disable_stream_test (gpointer user_data)
{
  GstPlayerAudioInfo *audio;
  GstPlayerSubtitleInfo *sub;
  GstPlayerVideoInfo *video;
  DisableStreamArgs *args = (DisableStreamArgs *) user_data;

  /* audio should be disabled */
  audio = gst_player_get_current_audio_track (args->player);
  fail_unless (audio == NULL);

  /* subtitle should be disabled */
  sub = gst_player_get_current_subtitle_track (args->player);
  fail_unless (sub == NULL);

  /* video should be disabled */
  video = gst_player_get_current_video_track (args->player);
  fail_unless (video == NULL);

  /* lets enable the streams and verify again */
  gst_player_set_audio_track_enabled (args->player, TRUE);
  gst_player_set_subtitle_track_enabled (args->player, TRUE);
  gst_player_set_video_track_enabled (args->player, TRUE);

  /* wait for 4 second and verify that streams are enabled */
  g_timeout_add_full (G_PRIORITY_DEFAULT, 4000, enable_stream_test,
      args, (GDestroyNotify) g_free);

  return G_SOURCE_REMOVE;
}

static void
test_play_stream_disable_enable_cb (GstPlayer * player,
    TestPlayerStateChange change, TestPlayerState * old_state,
    TestPlayerState * new_state)
{
  gint running = GPOINTER_TO_INT (new_state->test_data);

  if (new_state->state == GST_PLAYER_STATE_PLAYING && !running) {
    DisableStreamArgs *args = g_new0 (DisableStreamArgs, 1);

    /* Disable all the streams */
    gst_player_set_video_track_enabled (player, FALSE);
    gst_player_set_audio_track_enabled (player, FALSE);
    gst_player_set_subtitle_track_enabled (player, FALSE);

    args->player = player;
    args->loop = new_state->loop;
    new_state->test_data = GINT_TO_POINTER (running + 1);

    /* wait for 5sec and verify that streams are disabled */
    g_timeout_add (5000, disable_stream_test, args);
  } else if (change == STATE_CHANGE_END_OF_STREAM ||
      change == STATE_CHANGE_ERROR)
    g_main_loop_quit (new_state->loop);
}

START_TEST (test_play_stream_disable_enable)
{
  GstPlayer *player;
  TestPlayerState state;
  gchar *uri;

  memset (&state, 0, sizeof (state));
  state.loop = g_main_loop_new (NULL, FALSE);
  state.test_callback = test_play_stream_disable_enable_cb;
  state.test_data = GINT_TO_POINTER (0);

  player = test_player_new (&state);

  fail_unless (player != NULL);

  uri = gst_filename_to_uri (TEST_PATH "/sintel.mkv", NULL);
  fail_unless (uri != NULL);
  gst_player_set_uri (player, uri);
  g_free (uri);

  gst_player_play (player);
  g_main_loop_run (state.loop);

  fail_unless_equals_int (GPOINTER_TO_INT (state.test_data), 1);

  g_object_unref (player);
  g_main_loop_unref (state.loop);
}

END_TEST;

static void
test_play_error_invalid_external_suburi_cb (GstPlayer * player,
    TestPlayerStateChange change, TestPlayerState * old_state,
    TestPlayerState * new_state)
{
  gint steps = GPOINTER_TO_INT (new_state->test_data);

  if (new_state->state == GST_PLAYER_STATE_PLAYING && !steps) {
    gchar *suburi;

    suburi = gst_filename_to_uri (TEST_PATH "/foo.srt", NULL);
    fail_unless (suburi != NULL);

    new_state->test_data = GINT_TO_POINTER (steps + 1);
    /* load invalid suburi */
    fail_unless (gst_player_set_subtitle_uri (player, suburi) != FALSE);
    g_free (suburi);

  } else if (steps && change == STATE_CHANGE_WARNING) {
    new_state->test_data = GINT_TO_POINTER (steps + 1);
    g_main_loop_quit (new_state->loop);

  } else if (change == STATE_CHANGE_END_OF_STREAM ||
      change == STATE_CHANGE_ERROR)
    g_main_loop_quit (new_state->loop);
}

START_TEST (test_play_error_invalid_external_suburi)
{
  GstPlayer *player;
  TestPlayerState state;
  gchar *uri;

  memset (&state, 0, sizeof (state));
  state.loop = g_main_loop_new (NULL, FALSE);
  state.test_callback = test_play_error_invalid_external_suburi_cb;
  state.test_data = GINT_TO_POINTER (0);

  player = test_player_new (&state);

  fail_unless (player != NULL);

  uri = gst_filename_to_uri (TEST_PATH "/audio-video.ogg", NULL);
  fail_unless (uri != NULL);
  gst_player_set_uri (player, uri);
  g_free (uri);

  gst_player_play (player);
  g_main_loop_run (state.loop);

  fail_unless_equals_int (GPOINTER_TO_INT (state.test_data), 2);

  g_object_unref (player);
  g_main_loop_unref (state.loop);
}

END_TEST;

static gboolean
has_subtitle_stream (TestPlayerState * new_state)
{
  if (gst_player_get_subtitle_streams (new_state->media_info))
    return TRUE;

  return FALSE;
}

static void
test_play_external_suburi_cb (GstPlayer * player,
    TestPlayerStateChange change, TestPlayerState * old_state,
    TestPlayerState * new_state)
{
  gint steps = GPOINTER_TO_INT (new_state->test_data);

  if (new_state->state == GST_PLAYER_STATE_PLAYING && !steps) {
    gchar *suburi;

    suburi = gst_filename_to_uri (TEST_PATH "/test_sub.srt", NULL);
    fail_unless (suburi != NULL);

    fail_unless (gst_player_set_subtitle_uri (player, suburi) != FALSE);
    g_free (suburi);
    new_state->test_data = GINT_TO_POINTER (steps + 1);

  } else if (change == STATE_CHANGE_MEDIA_INFO_UPDATED &&
      has_subtitle_stream (new_state)) {
    gchar *current_suburi, *suburi;

    current_suburi = gst_player_get_subtitle_uri (player);
    fail_unless (current_suburi != NULL);
    suburi = gst_filename_to_uri (TEST_PATH "/test_sub.srt", NULL);
    fail_unless (suburi != NULL);

    fail_unless_equals_int (g_strcmp0 (current_suburi, suburi), 0);

    g_free (current_suburi);
    g_free (suburi);
    new_state->test_data = GINT_TO_POINTER (steps + 1);
    g_main_loop_quit (new_state->loop);

  } else if (change == STATE_CHANGE_END_OF_STREAM ||
      change == STATE_CHANGE_ERROR)
    g_main_loop_quit (new_state->loop);
}

START_TEST (test_play_external_suburi)
{
  GstPlayer *player;
  TestPlayerState state;
  gchar *uri;

  memset (&state, 0, sizeof (state));
  state.loop = g_main_loop_new (NULL, FALSE);
  state.test_callback = test_play_external_suburi_cb;
  state.test_data = GINT_TO_POINTER (0);

  player = test_player_new (&state);

  fail_unless (player != NULL);

  uri = gst_filename_to_uri (TEST_PATH "/audio-video.ogg", NULL);
  fail_unless (uri != NULL);
  gst_player_set_uri (player, uri);
  g_free (uri);

  gst_player_play (player);
  g_main_loop_run (state.loop);

  fail_unless_equals_int (GPOINTER_TO_INT (state.test_data), 2);

  g_object_unref (player);
  g_main_loop_unref (state.loop);
}

END_TEST;

START_TEST (test_play_audio_video_eos)
{
  GstPlayer *player;
  TestPlayerState state;
  gchar *uri;

  memset (&state, 0, sizeof (state));
  state.loop = g_main_loop_new (NULL, FALSE);
  state.test_callback = test_play_audio_video_eos_cb;
  state.test_data = GINT_TO_POINTER (0x10);

  player = test_player_new (&state);

  fail_unless (player != NULL);

  uri = gst_filename_to_uri (TEST_PATH "/audio-video-short.ogg", NULL);
  fail_unless (uri != NULL);
  gst_player_set_uri (player, uri);
  g_free (uri);

  gst_player_play (player);
  g_main_loop_run (state.loop);

  fail_unless_equals_int (GPOINTER_TO_INT (state.test_data) & (~0x10), 8);

  g_object_unref (player);
  g_main_loop_unref (state.loop);
}

END_TEST;

static void
test_play_error_invalid_uri_cb (GstPlayer * player,
    TestPlayerStateChange change, TestPlayerState * old_state,
    TestPlayerState * new_state)
{
  gint step = GPOINTER_TO_INT (new_state->test_data);

  switch (step) {
    case 0:
      fail_unless_equals_int (change, STATE_CHANGE_STATE_CHANGED);
      fail_unless_equals_int (old_state->state, GST_PLAYER_STATE_STOPPED);
      fail_unless_equals_int (new_state->state, GST_PLAYER_STATE_BUFFERING);
      new_state->test_data = GINT_TO_POINTER (step + 1);
      break;
    case 1:
      fail_unless_equals_int (change, STATE_CHANGE_ERROR);
      new_state->test_data = GINT_TO_POINTER (step + 1);
      break;
    case 2:
      fail_unless_equals_int (change, STATE_CHANGE_STATE_CHANGED);
      fail_unless_equals_int (old_state->state, GST_PLAYER_STATE_BUFFERING);
      fail_unless_equals_int (new_state->state, GST_PLAYER_STATE_STOPPED);
      new_state->test_data = GINT_TO_POINTER (step + 1);
      g_main_loop_quit (new_state->loop);
      break;
    default:
      fail ();
      break;
  }
}

START_TEST (test_play_error_invalid_uri)
{
  GstPlayer *player;
  TestPlayerState state;

  memset (&state, 0, sizeof (state));
  state.loop = g_main_loop_new (NULL, FALSE);
  state.test_callback = test_play_error_invalid_uri_cb;
  state.test_data = GINT_TO_POINTER (0);

  player = test_player_new (&state);

  fail_unless (player != NULL);

  gst_player_set_uri (player, "foo://bar");

  gst_player_play (player);
  g_main_loop_run (state.loop);

  fail_unless_equals_int (GPOINTER_TO_INT (state.test_data), 3);

  g_object_unref (player);
  g_main_loop_unref (state.loop);
}

END_TEST;

static void
test_play_error_invalid_uri_and_play_cb (GstPlayer * player,
    TestPlayerStateChange change, TestPlayerState * old_state,
    TestPlayerState * new_state)
{
  gint step = GPOINTER_TO_INT (new_state->test_data);
  gchar *uri;

  switch (step) {
    case 0:
      fail_unless_equals_int (change, STATE_CHANGE_STATE_CHANGED);
      fail_unless_equals_int (old_state->state, GST_PLAYER_STATE_STOPPED);
      fail_unless_equals_int (new_state->state, GST_PLAYER_STATE_BUFFERING);
      new_state->test_data = GINT_TO_POINTER (step + 1);
      break;
    case 1:
      fail_unless_equals_int (change, STATE_CHANGE_ERROR);
      new_state->test_data = GINT_TO_POINTER (step + 1);
      break;
    case 2:
      fail_unless_equals_int (change, STATE_CHANGE_STATE_CHANGED);
      fail_unless_equals_int (old_state->state, GST_PLAYER_STATE_BUFFERING);
      fail_unless_equals_int (new_state->state, GST_PLAYER_STATE_STOPPED);
      new_state->test_data = GINT_TO_POINTER (step + 1);

      uri = gst_filename_to_uri (TEST_PATH "/audio-short.ogg", NULL);
      fail_unless (uri != NULL);
      gst_player_set_uri (player, uri);
      g_free (uri);

      gst_player_play (player);
      break;
    case 3:
      fail_unless_equals_int (change, STATE_CHANGE_STATE_CHANGED);
      fail_unless_equals_int (old_state->state, GST_PLAYER_STATE_STOPPED);
      fail_unless_equals_int (new_state->state, GST_PLAYER_STATE_BUFFERING);
      new_state->test_data = GINT_TO_POINTER (step + 1);
      break;
    case 4:
      fail_unless_equals_int (change, STATE_CHANGE_MEDIA_INFO_UPDATED);
      new_state->test_data = GINT_TO_POINTER (step + 1);
      break;
    case 5:
      fail_unless_equals_int (change, STATE_CHANGE_VIDEO_DIMENSIONS_CHANGED);
      fail_unless_equals_int (new_state->width, 0);
      fail_unless_equals_int (new_state->height, 0);
      new_state->test_data = GINT_TO_POINTER (step + 1);
      break;
    case 6:
      fail_unless_equals_int (change, STATE_CHANGE_DURATION_CHANGED);
      fail_unless_equals_uint64 (new_state->duration,
          G_GUINT64_CONSTANT (464399092));
      new_state->test_data = GINT_TO_POINTER (step + 1);
      break;
    case 7:
      fail_unless_equals_int (change, STATE_CHANGE_POSITION_UPDATED);
      fail_unless_equals_uint64 (new_state->position, G_GUINT64_CONSTANT (0));
      new_state->test_data = GINT_TO_POINTER (step + 1);
      break;
    case 8:
      fail_unless_equals_int (change, STATE_CHANGE_STATE_CHANGED);
      fail_unless_equals_int (old_state->state, GST_PLAYER_STATE_BUFFERING);
      fail_unless_equals_int (new_state->state, GST_PLAYER_STATE_PLAYING);
      new_state->test_data = GINT_TO_POINTER (step + 1);
      g_main_loop_quit (new_state->loop);
      break;
    default:
      fail ();
      break;
  }
}

START_TEST (test_play_error_invalid_uri_and_play)
{
  GstPlayer *player;
  TestPlayerState state;

  memset (&state, 0, sizeof (state));
  state.loop = g_main_loop_new (NULL, FALSE);
  state.test_callback = test_play_error_invalid_uri_and_play_cb;
  state.test_data = GINT_TO_POINTER (0);

  player = test_player_new (&state);

  fail_unless (player != NULL);

  gst_player_set_uri (player, "foo://bar");

  gst_player_play (player);
  g_main_loop_run (state.loop);

  fail_unless_equals_int (GPOINTER_TO_INT (state.test_data), 9);

  g_object_unref (player);
  g_main_loop_unref (state.loop);
}

END_TEST;

static Suite *
player_suite (void)
{
  Suite *s = suite_create ("GstPlayer");

  TCase *tc_general = tcase_create ("general");

  tcase_set_timeout (tc_general, 120);
  tcase_add_test (tc_general, test_create_and_free);
  tcase_add_test (tc_general, test_set_and_get_uri);
  tcase_add_test (tc_general, test_play_audio_eos);
  tcase_add_test (tc_general, test_play_audio_video_eos);
  tcase_add_test (tc_general, test_play_error_invalid_uri);
  tcase_add_test (tc_general, test_play_error_invalid_uri_and_play);
  tcase_add_test (tc_general, test_play_media_info);
  tcase_add_test (tc_general, test_play_stream_selection);
  tcase_add_test (tc_general, test_play_stream_disable_enable);
  tcase_add_test (tc_general, test_play_error_invalid_external_suburi);
  tcase_add_test (tc_general, test_play_external_suburi);

  suite_add_tcase (s, tc_general);

  return s;
}

int
main (int argc, char **argv)
{
  int number_failed;
  Suite *s;
  SRunner *sr;

  gst_init (NULL, NULL);

  GST_DEBUG_CATEGORY_INIT (test_debug, "test", 0, "GstPlayer test");

  s = player_suite ();
  sr = srunner_create (s);

  srunner_run_all (sr, CK_NORMAL);

  number_failed = srunner_ntests_failed (sr);

  srunner_free (sr);
  return (number_failed == 0) ? 0 : -1;
}
