/* GStreamer command line playback testing utility
 *
 * Copyright (C) 2013-2014 Tim-Philipp Müller <tim centricular net>
 * Copyright (C) 2013 Collabora Ltd.
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

#include <locale.h>

#include <gst/gst.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "gst-play-kb.h"
#include <gst/player/player.h>

#define VOLUME_STEPS 20

GST_DEBUG_CATEGORY (play_debug);
#define GST_CAT_DEFAULT play_debug

typedef struct
{
  gchar **uris;
  guint num_uris;
  gint cur_idx;

  GstPlayer *player;
  GstState desired_state;

  GstPlayerMediaInfo  *media_info;
  GMainLoop *loop;
} GstPlay;

static gboolean play_next (GstPlay * play);
static gboolean play_prev (GstPlay * play);
static void play_reset (GstPlay * play);
static void play_set_relative_volume (GstPlay * play, gdouble volume_step);

static void
end_of_stream_cb (GstPlayer * player, GstPlay * play)
{
  g_print ("\n");
  /* and switch to next item in list */
  if (!play_next (play)) {
    g_print ("Reached end of play list.\n");
    g_main_loop_quit (play->loop);
  }
}

static void
error_cb (GstPlayer * player, GError * err, GstPlay * play)
{
  g_printerr ("ERROR %s for %s\n", err->message, play->uris[play->cur_idx]);

  /* try next item in list then */
  if (!play_next (play)) {
    g_print ("Reached end of play list.\n");
    g_main_loop_quit (play->loop);
  }
}

static void
position_updated_cb (GstPlayer * player, GstClockTime pos, GstPlay * play)
{
  GstClockTime dur = -1;
  gchar status[64] = { 0, };

  g_object_get (play->player, "duration", &dur, NULL);

  memset (status, ' ', sizeof (status) - 1);

  if (pos >= 0 && dur > 0) {
    gchar dstr[32], pstr[32];

    /* FIXME: pretty print in nicer format */
    g_snprintf (pstr, 32, "%" GST_TIME_FORMAT, GST_TIME_ARGS (pos));
    pstr[9] = '\0';
    g_snprintf (dstr, 32, "%" GST_TIME_FORMAT, GST_TIME_ARGS (dur));
    dstr[9] = '\0';
    g_print ("%s / %s %s\r", pstr, dstr, status);
  }
}

static void
state_changed_cb (GstPlayer * player, GstPlayerState state, GstPlay * play)
{
  g_print ("State changed: %s\n", gst_player_state_get_name (state));
}

static void
buffering_cb (GstPlayer * player, gint percent, GstPlay * play)
{
  g_print ("Buffering: %d\n", percent);
}

static void
print_one_tag (const GstTagList *list, const gchar *tag, gpointer user_data)
{
  gint i, num;

  return;
  num = gst_tag_list_get_tag_size (list, tag);
  for (i = 0; i < num; ++i) {
    const GValue *val;

    val = gst_tag_list_get_value_index (list, tag, i);
    if (G_VALUE_HOLDS_STRING (val)) {
      g_print ("    %s : %s \n", tag, g_value_get_string (val));
    }
    else if (G_VALUE_HOLDS_UINT (val)) {
      g_print ("    %s : %u \n", tag, g_value_get_uint (val));
    }
    else if (G_VALUE_HOLDS_DOUBLE (val)) {
      g_print ("    %s : %g \n", tag, g_value_get_double (val));
    }
    else if (G_VALUE_HOLDS_BOOLEAN (val)) {
      g_print ("    %s : %s \n", tag,
                g_value_get_boolean (val) ? "true" : "false");
    }
    else if (GST_VALUE_HOLDS_DATE_TIME (val)) {
      GstDateTime *dt = g_value_get_boxed (val);
      gchar *dt_str = gst_date_time_to_iso8601_string (dt);

      g_print ("    %s : %s \n", tag, dt_str);
      g_free (dt_str);
    }
    else {
      g_print ("    %s : tag of type '%s' \n", tag, G_VALUE_TYPE_NAME (val));
    }
  }
}

static void
print_video_info (GstPlayerVideoInfo *info)
{
  gint fps_n, fps_d;
  guint par_n, par_d;

  if (info == NULL)
    return;

  g_print ("  width : %d\n", gst_player_video_info_get_width(info));
  g_print ("  height : %d\n", gst_player_video_info_get_height (info));
  g_print ("  max_bitrate : %d\n", gst_player_video_info_get_max_bitrate (info));
  g_print ("  bitrate : %d\n", gst_player_video_info_get_bitrate (info));
  gst_player_video_info_get_framerate (info, &fps_n, &fps_d);
  g_print ("  frameate : %.2f\n", (gdouble) fps_n/fps_d);
  gst_player_video_info_get_pixel_aspect_ratio (info, &par_n, &par_d);
  g_print ("  pixel-aspect-ratio  %u:%u\n", par_n, par_d);
}

static void
print_audio_info (GstPlayerAudioInfo *info)
{
  if (info == NULL)
    return;

  g_print ("  sample rate : %d\n", gst_player_audio_info_get_sample_rate (info));
  g_print ("  channels : %d\n", gst_player_audio_info_get_channels (info));
  g_print ("  max_bitrate : %d\n", gst_player_audio_info_get_max_bitrate (info));
  g_print ("  bitrate : %d\n", gst_player_audio_info_get_bitrate (info));
  g_print ("  language : %s\n", gst_player_audio_info_get_language (info));
}

static void
print_subtitle_info (GstPlayerSubtitleInfo *info)
{
  if (info == NULL)
    return;

  g_print ("  language : %s\n", gst_player_subtitle_get_language (info));
}

static void
print_all_stream_info (GstPlay *play)
{
  guint count = 0;
  GList *l, *list;
  GstPlayerMediaInfo  *info = play->media_info;

  if (!play->media_info)
    return;

  list = gst_player_media_info_get_stream_list (info);
  g_print ("URI : %s\n", gst_player_media_info_get_uri (info));
  g_print ("Duration: %" GST_TIME_FORMAT "\n",
      GST_TIME_ARGS(gst_player_media_info_get_duration (info)));
  for (l = list; l != NULL; l = l->next) {
    GstTagList  *tags = NULL;
    GstPlayerStreamInfo *stream = (GstPlayerStreamInfo*) l->data;

    g_print (" Stream # %u \n", count++);
    g_print ("  type : %s_%u\n",
              gst_player_stream_info_get_stream_type_nick (stream),
              gst_player_stream_info_get_stream_index (stream));
    tags = gst_player_stream_info_get_stream_tags (stream);
    g_print ("  taglist : \n");
    if (tags) {
      gst_tag_list_foreach (tags, print_one_tag, NULL);
    }

    if (GST_IS_PLAYER_VIDEO_INFO (stream))
      print_video_info ((GstPlayerVideoInfo*)stream);
    if (GST_IS_PLAYER_AUDIO_INFO (stream))
      print_audio_info ((GstPlayerAudioInfo*)stream);
    if (GST_IS_PLAYER_SUBTITLE_INFO (stream))
      print_subtitle_info ((GstPlayerSubtitleInfo*)stream);
  }
}

static void
print_all_video_stream (GstPlay *play)
{
  GList *list = NULL, *l;

  if (!play->media_info)
    return;

  list = gst_player_get_video_streams (play->media_info);
  if (!list)
    return;

  g_print ("All video streams\n");
  for (l = list; l != NULL; l = l->next) {
    GstPlayerVideoInfo  *info = (GstPlayerVideoInfo*) l->data;
    GstPlayerStreamInfo *sinfo = (GstPlayerStreamInfo*) info;
    g_print (" %s_%d #\n", gst_player_stream_info_get_stream_type_nick (sinfo),
            gst_player_stream_info_get_stream_index (sinfo));
    print_video_info (info);
  }
  gst_player_stream_info_list_free (list);
}

static void
print_all_subtitle_stream (GstPlay *play)
{
  GList *list = NULL, *l;

  if (!play->media_info)
    return;

  list = gst_player_get_subtitle_streams (play->media_info);

  if (!list)
  return;

  g_print ("All subtitle streams:\n");
  for (l = list; l != NULL; l = l->next) {
    GstPlayerSubtitleInfo  *info = (GstPlayerSubtitleInfo*) l->data;
    GstPlayerStreamInfo *sinfo = (GstPlayerStreamInfo*) info;
    g_print (" %s_%d #\n", gst_player_stream_info_get_stream_type_nick (sinfo),
    gst_player_stream_info_get_stream_index (sinfo));
    print_subtitle_info (info);
  }

  gst_player_stream_info_list_free (list);
}

static void
print_all_audio_stream (GstPlay *play)
{
  GList *list = NULL, *l;

  if (!play->media_info)
    return;

  list = gst_player_get_audio_streams (play->media_info);
  if (!list)
    return;

  g_print ("All audio streams: \n");
  for (l = list; l != NULL; l = l->next) {
    GstPlayerAudioInfo  *info = (GstPlayerAudioInfo*) l->data;
    GstPlayerStreamInfo *sinfo = (GstPlayerStreamInfo*) info;
    g_print (" %s_%d #\n", gst_player_stream_info_get_stream_type_nick (sinfo),
              gst_player_stream_info_get_stream_index (sinfo));
    print_audio_info (info);
  }

  gst_player_stream_info_list_free (list);
}

static void
print_current_tracks (GstPlay *play)
{
  g_print ("Current video track: \n");
  print_video_info (gst_player_get_video_track(play->player));
  g_print ("Current audio track: \n");
  print_audio_info (gst_player_get_audio_track(play->player));
  g_print ("Current subtitle track: \n");
  print_subtitle_info (gst_player_get_subtitle_track(play->player));
}

static void
media_info_cb (GstPlayer *player, GstPlayerMediaInfo *info, GstPlay *play)
{
  play->media_info = info;
}

static GstPlay *
play_new (gchar ** uris, gdouble initial_volume)
{
  GstPlay *play;

  play = g_new0 (GstPlay, 1);

  play->uris = uris;
  play->num_uris = g_strv_length (uris);
  play->cur_idx = -1;

  play->player = gst_player_new ();

  g_object_set (play->player, "dispatch-to-main-context", TRUE, NULL);
  g_signal_connect (play->player, "position-updated",
      G_CALLBACK (position_updated_cb), play);
  g_signal_connect (play->player, "state-changed",
      G_CALLBACK (state_changed_cb), play);
  g_signal_connect (play->player, "buffering",
      G_CALLBACK (buffering_cb), play);
  g_signal_connect (play->player, "end-of-stream",
      G_CALLBACK (end_of_stream_cb), play);
  g_signal_connect (play->player, "error", G_CALLBACK (error_cb), play);

  g_signal_connect (play->player, "media-info-updated",
      G_CALLBACK (media_info_cb), play);

  play->loop = g_main_loop_new (NULL, FALSE);
  play->desired_state = GST_STATE_PLAYING;

  play_set_relative_volume (play, initial_volume - 1.0);

  return play;
}

static void
play_free (GstPlay * play)
{
  play_reset (play);

  gst_object_unref (play->player);

  g_main_loop_unref (play->loop);

  g_strfreev (play->uris);
  g_free (play);
}

/* reset for new file/stream */
static void
play_reset (GstPlay * play)
{
}

static void
play_set_relative_volume (GstPlay * play, gdouble volume_step)
{
  gdouble volume;

  g_object_get (play->player, "volume", &volume, NULL);
  volume = round ((volume + volume_step) * VOLUME_STEPS) / VOLUME_STEPS;
  volume = CLAMP (volume, 0.0, 10.0);

  g_object_set (play->player, "volume", volume, NULL);

  g_print ("Volume: %.0f%%                  \n", volume * 100);
}

static gchar *
play_uri_get_display_name (GstPlay * play, const gchar * uri)
{
  gchar *loc;

  if (gst_uri_has_protocol (uri, "file")) {
    loc = g_filename_from_uri (uri, NULL, NULL);
  } else if (gst_uri_has_protocol (uri, "pushfile")) {
    loc = g_filename_from_uri (uri + 4, NULL, NULL);
  } else {
    loc = g_strdup (uri);
  }

  /* Maybe additionally use glib's filename to display name function */
  return loc;
}

static void
play_uri (GstPlay * play, const gchar * next_uri)
{
  gchar *loc;

  play_reset (play);

  loc = play_uri_get_display_name (play, next_uri);
  g_print ("Now playing %s\n", loc);
  g_free (loc);

  g_object_set (play->player, "uri", next_uri, NULL);
  gst_player_play (play->player);
}

/* returns FALSE if we have reached the end of the playlist */
static gboolean
play_next (GstPlay * play)
{
  if ((play->cur_idx + 1) >= play->num_uris)
    return FALSE;

  play_uri (play, play->uris[++play->cur_idx]);
  return TRUE;
}

/* returns FALSE if we have reached the beginning of the playlist */
static gboolean
play_prev (GstPlay * play)
{
  if (play->cur_idx == 0 || play->num_uris <= 1)
    return FALSE;

  play_uri (play, play->uris[--play->cur_idx]);
  return TRUE;
}

static void
do_play (GstPlay * play)
{
  gint i;

  /* dump playlist */
  for (i = 0; i < play->num_uris; ++i)
    GST_INFO ("%4u : %s", i, play->uris[i]);

  if (!play_next (play))
    return;

  g_main_loop_run (play->loop);
}

static void
add_to_playlist (GPtrArray * playlist, const gchar * filename)
{
  GDir *dir;
  gchar *uri;

  if (gst_uri_is_valid (filename)) {
    g_ptr_array_add (playlist, g_strdup (filename));
    return;
  }

  if ((dir = g_dir_open (filename, 0, NULL))) {
    const gchar *entry;

    /* FIXME: sort entries for each directory? */
    while ((entry = g_dir_read_name (dir))) {
      gchar *path;

      path = g_strconcat (filename, G_DIR_SEPARATOR_S, entry, NULL);
      add_to_playlist (playlist, path);
      g_free (path);
    }

    g_dir_close (dir);
    return;
  }

  uri = gst_filename_to_uri (filename, NULL);
  if (uri != NULL)
    g_ptr_array_add (playlist, uri);
  else
    g_warning ("Could not make URI out of filename '%s'", filename);
}

static void
shuffle_uris (gchar ** uris, guint num)
{
  gchar *tmp;
  guint i, j;

  if (num < 2)
    return;

  for (i = 0; i < num; i++) {
    /* gets equally distributed random number in 0..num-1 [0;num[ */
    j = g_random_int_range (0, num);
    tmp = uris[j];
    uris[j] = uris[i];
    uris[i] = tmp;
  }
}

static void
restore_terminal (void)
{
  gst_play_kb_set_key_handler (NULL, NULL);
}

static void
toggle_paused (GstPlay * play)
{
  if (play->desired_state == GST_STATE_PLAYING)
    play->desired_state = GST_STATE_PAUSED;
  else
    play->desired_state = GST_STATE_PLAYING;

  if (play->desired_state == GST_STATE_PLAYING)
    gst_player_play (play->player);
  else
    gst_player_pause (play->player);
}

static void
relative_seek (GstPlay * play, gdouble percent)
{
  gint64 dur = -1, pos = -1;

  g_return_if_fail (percent >= -1.0 && percent <= 1.0);

  g_object_get (play->player, "position", &pos, "duration", &dur, NULL);

  if (dur <= 0)
    goto seek_failed;

  pos = pos + dur * percent;
  if (pos < 0)
    pos = 0;
  gst_player_seek (play->player, pos);

  return;

seek_failed:
  {
    g_print ("\nCould not seek.\n");
  }
}

static void
keyboard_cb (const gchar * key_input, gpointer user_data)
{
  GstPlay *play = (GstPlay *) user_data;

  switch (g_ascii_tolower (key_input[0])) {
    case 'i':
      print_all_stream_info (play);
      g_print ("\n");
      print_all_video_stream (play);
      g_print ("\n");
      print_all_audio_stream (play);
      g_print ("\n");
      print_all_subtitle_stream (play);
      g_print ("\n");
      print_current_tracks (play);
      g_print ("\n");
      break;
    case ' ':
      toggle_paused (play);
      break;
    case 'q':
    case 'Q':
      g_main_loop_quit (play->loop);
      break;
    case '>':
      if (!play_next (play)) {
        g_print ("\nReached end of play list.\n");
        g_main_loop_quit (play->loop);
      }
      break;
    case '<':
      play_prev (play);
      break;
    case 27:                   /* ESC */
      if (key_input[1] == '\0') {
        g_main_loop_quit (play->loop);
        break;
      }
      /* fall through */
    default:
      if (strcmp (key_input, GST_PLAY_KB_ARROW_RIGHT) == 0) {
        relative_seek (play, +0.08);
      } else if (strcmp (key_input, GST_PLAY_KB_ARROW_LEFT) == 0) {
        relative_seek (play, -0.01);
      } else if (strcmp (key_input, GST_PLAY_KB_ARROW_UP) == 0) {
        play_set_relative_volume (play, +1.0 / VOLUME_STEPS);
      } else if (strcmp (key_input, GST_PLAY_KB_ARROW_DOWN) == 0) {
        play_set_relative_volume (play, -1.0 / VOLUME_STEPS);
      } else {
        GST_INFO ("keyboard input:");
        for (; *key_input != '\0'; ++key_input)
          GST_INFO ("  code %3d", *key_input);
      }
      break;
  }
}

int
main (int argc, char **argv)
{
  GstPlay *play;
  GPtrArray *playlist;
  gboolean print_version = FALSE;
  gboolean interactive = FALSE; /* FIXME: maybe enable by default? */
  gboolean shuffle = FALSE;
  gdouble volume = 1.0;
  gchar **filenames = NULL;
  gchar **uris;
  guint num, i;
  GError *err = NULL;
  GOptionContext *ctx;
  gchar *playlist_file = NULL;
  GOptionEntry options[] = {
    {"version", 0, 0, G_OPTION_ARG_NONE, &print_version,
        "Print version information and exit", NULL},
    {"shuffle", 0, 0, G_OPTION_ARG_NONE, &shuffle,
        "Shuffle playlist", NULL},
    {"interactive", 0, 0, G_OPTION_ARG_NONE, &interactive,
        "Interactive control via keyboard", NULL},
    {"volume", 0, 0, G_OPTION_ARG_DOUBLE, &volume,
        "Volume", NULL},
    {"playlist", 0, 0, G_OPTION_ARG_FILENAME, &playlist_file,
        "Playlist file containing input media files", NULL},
    {G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &filenames, NULL},
    {NULL}
  };

  g_set_prgname ("gst-play");

  ctx = g_option_context_new ("FILE1|URI1 [FILE2|URI2] [FILE3|URI3] ...");
  g_option_context_add_main_entries (ctx, options, NULL);
  g_option_context_add_group (ctx, gst_init_get_option_group ());
  if (!g_option_context_parse (ctx, &argc, &argv, &err)) {
    g_print ("Error initializing: %s\n", GST_STR_NULL (err->message));
    return 1;
  }
  g_option_context_free (ctx);

  GST_DEBUG_CATEGORY_INIT (play_debug, "play", 0, "gst-play");

  if (print_version) {
    gchar *version_str;

    version_str = gst_version_string ();
    g_print ("%s version %s\n", g_get_prgname (), "1.0");
    g_print ("%s\n", version_str);
    g_free (version_str);

    g_free (playlist_file);

    return 0;
  }

  playlist = g_ptr_array_new ();

  if (playlist_file != NULL) {
    gchar *playlist_contents = NULL;
    gchar **lines = NULL;

    if (g_file_get_contents (playlist_file, &playlist_contents, NULL, &err)) {
      lines = g_strsplit (playlist_contents, "\n", 0);
      num = g_strv_length (lines);

      for (i = 0; i < num; i++) {
        if (lines[i][0] != '\0') {
          GST_LOG ("Playlist[%d]: %s", i + 1, lines[i]);
          add_to_playlist (playlist, lines[i]);
        }
      }
      g_strfreev (lines);
      g_free (playlist_contents);
    } else {
      g_printerr ("Could not read playlist: %s\n", err->message);
      g_clear_error (&err);
    }
    g_free (playlist_file);
    playlist_file = NULL;
  }

  if (playlist->len == 0 && (filenames == NULL || *filenames == NULL)) {
    g_printerr ("Usage: %s FILE1|URI1 [FILE2|URI2] [FILE3|URI3] ...",
        "gst-play");
    g_printerr ("\n\n"),
        g_printerr ("%s\n\n",
        "You must provide at least one filename or URI to play.");
    /* No input provided. Free array */
    g_ptr_array_free (playlist, TRUE);

    return 1;
  }

  /* fill playlist */
  if (filenames != NULL && *filenames != NULL) {
    num = g_strv_length (filenames);
    for (i = 0; i < num; ++i) {
      GST_LOG ("command line argument: %s", filenames[i]);
      add_to_playlist (playlist, filenames[i]);
    }
    g_strfreev (filenames);
  }

  num = playlist->len;
  g_ptr_array_add (playlist, NULL);

  uris = (gchar **) g_ptr_array_free (playlist, FALSE);

  if (shuffle)
    shuffle_uris (uris, num);

  /* prepare */
  play = play_new (uris, volume);

  if (interactive) {
    if (gst_play_kb_set_key_handler (keyboard_cb, play)) {
      atexit (restore_terminal);
    } else {
      g_print ("Interactive keyboard handling in terminal not available.\n");
    }
  }

  /* play */
  do_play (play);

  /* clean up */
  play_free (play);

  g_print ("\n");
  return 0;
}
