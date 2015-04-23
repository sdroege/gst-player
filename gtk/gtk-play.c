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

#include <string.h>

#include <gst/gst.h>
#include <gst/video/videooverlay.h>

#include <gdk/gdk.h>
#if defined (GDK_WINDOWING_X11)
#include <gdk/gdkx.h>
#elif defined (GDK_WINDOWING_WIN32)
#include <gdk/gdkwin32.h>
#elif defined (GDK_WINDOWING_QUARTZ)
#include <gdk/gdkquartz.h>
#endif

#include <gtk/gtk.h>

#include <gst/player/player.h>

#define APP_NAME "gtk-play"

typedef struct
{
  GstPlayer *player;
  gchar *uri;

  GList *uris;

  GtkWidget *window;
  GtkWidget *play_pause_button;
  GtkWidget *prev_button, *next_button;
  GtkWidget *seekbar;
  GtkWidget *video_area;
  GtkWidget *info_label;
  GtkWidget *info_bar;
  GtkWidget *volume_button;
  gulong seekbar_value_changed_signal_id;
  gboolean playing;
} GtkPlay;

static void
set_title (GtkPlay * play, const gchar * title)
{
  if (title == NULL) {
    gtk_window_set_title (GTK_WINDOW (play->window), APP_NAME);
  } else {
    gtk_window_set_title (GTK_WINDOW (play->window), title);
  }
}

static void
delete_event_cb (GtkWidget * widget, GdkEvent * event, GtkPlay * play)
{
  gst_player_stop (play->player);
  gtk_main_quit ();
}

static void
video_area_realize_cb (GtkWidget * widget, GtkPlay * play)
{
  GdkWindow *window = gtk_widget_get_window (widget);
  guintptr window_handle;

  if (!gdk_window_ensure_native (window))
    g_error ("Couldn't create native window needed for GstXOverlay!");

#if defined (GDK_WINDOWING_WIN32)
  window_handle = (guintptr) GDK_WINDOW_HWND (window);
#elif defined (GDK_WINDOWING_QUARTZ)
  window_handle = gdk_quartz_window_get_nsview (window);
#elif defined (GDK_WINDOWING_X11)
  window_handle = GDK_WINDOW_XID (window);
#endif
  g_object_set (play->player, "window-handle", (gpointer) window_handle, NULL);
}

static void
play_pause_clicked_cb (GtkButton * button, GtkPlay * play)
{
  GtkWidget *image;

  if (play->playing) {
    gst_player_pause (play->player);
    image =
        gtk_image_new_from_icon_name ("media-playback-start",
        GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image (GTK_BUTTON (play->play_pause_button), image);
    play->playing = FALSE;
  } else {
    gchar *title;

    gst_player_play (play->player);
    image =
        gtk_image_new_from_icon_name ("media-playback-pause",
        GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image (GTK_BUTTON (play->play_pause_button), image);

    title = gst_player_get_uri (play->player);
    set_title (play, title);
    g_free (title);
    play->playing = TRUE;
  }
}

static void
clear_missing_plugins (GtkPlay * play)
{
  gtk_label_set_text (GTK_LABEL (play->info_label), "");
  gtk_widget_hide (play->info_bar);
}

static void
skip_prev_clicked_cb (GtkButton * button, GtkPlay * play)
{
  GList *prev;
  gchar *cur_uri;

  prev = g_list_find_custom (play->uris,
      gst_player_get_uri (play->player), (GCompareFunc) strcmp);

  g_return_if_fail (prev != NULL);
  prev = g_list_previous (prev);
  g_return_if_fail (prev != NULL);

  gtk_widget_set_sensitive (play->next_button, TRUE);
  gst_player_set_uri (play->player, prev->data);
  clear_missing_plugins (play);
  gst_player_play (play->player);
  set_title (play, prev->data);
  gtk_widget_set_sensitive (play->prev_button, g_list_previous (prev) != NULL);
}

static void
skip_next_clicked_cb (GtkButton * button, GtkPlay * play)
{
  GList *next, *l;
  gchar *cur_uri;

  next = g_list_find_custom (play->uris,
      gst_player_get_uri (play->player), (GCompareFunc) strcmp);

  g_return_if_fail (next != NULL);
  next = g_list_next (next);
  g_return_if_fail (next != NULL);

  gtk_widget_set_sensitive (play->prev_button, TRUE);
  gst_player_set_uri (play->player, next->data);
  clear_missing_plugins (play);
  gst_player_play (play->player);
  set_title (play, next->data);
  gtk_widget_set_sensitive (play->next_button, g_list_next (next) != NULL);
}

static void
seekbar_value_changed_cb (GtkRange * range, GtkPlay * play)
{
  gdouble value = gtk_range_get_value (GTK_RANGE (play->seekbar));
  gst_player_seek (play->player, gst_util_uint64_scale (value, GST_SECOND, 1));
}

void
volume_changed_cb (GtkScaleButton * button, gdouble value, GtkPlay * play)
{
  gst_player_set_volume (play->player, value);
}

void
info_bar_response_cb (GtkInfoBar * bar, gint response, GtkPlay * play)
{
  gtk_widget_hide (GTK_WIDGET (bar));
}

static void
create_ui (GtkPlay * play)
{
  GtkWidget *controls, *main_hbox, *main_vbox, *info_bar, *content_area;

  play->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (G_OBJECT (play->window), "delete-event",
      G_CALLBACK (delete_event_cb), play);
  set_title (play, APP_NAME);

  play->video_area = gtk_drawing_area_new ();
  gtk_widget_set_double_buffered (play->video_area, FALSE);
  g_signal_connect (play->video_area, "realize",
      G_CALLBACK (video_area_realize_cb), play);

  play->info_bar = gtk_info_bar_new ();
  gtk_info_bar_set_message_type (GTK_INFO_BAR (play->info_bar),
      GTK_MESSAGE_WARNING);
  gtk_info_bar_set_show_close_button (GTK_INFO_BAR (play->info_bar),
      TRUE);
  gtk_widget_set_no_show_all (play->info_bar, TRUE);
  g_signal_connect (play->info_bar, "response",
      G_CALLBACK (info_bar_response_cb), play);

  content_area = gtk_info_bar_get_content_area (GTK_INFO_BAR (play->info_bar));
  play->info_label = gtk_label_new ("");
  gtk_container_add (GTK_CONTAINER (content_area), play->info_label);
  gtk_widget_show (play->info_label);

  /* Unified play/pause button */
  play->play_pause_button =
      gtk_button_new_from_icon_name ("media-playback-pause",
      GTK_ICON_SIZE_BUTTON);
  g_signal_connect (G_OBJECT (play->play_pause_button), "clicked",
      G_CALLBACK (play_pause_clicked_cb), play);

  play->seekbar =
      gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL, 0, 100, 1);
  gtk_scale_set_draw_value (GTK_SCALE (play->seekbar), 0);
  play->seekbar_value_changed_signal_id =
      g_signal_connect (G_OBJECT (play->seekbar), "value-changed",
      G_CALLBACK (seekbar_value_changed_cb), play);

  /* Skip backward button */
  play->prev_button =
      gtk_button_new_from_icon_name ("media-skip-backward",
      GTK_ICON_SIZE_BUTTON);
  g_signal_connect (G_OBJECT (play->prev_button), "clicked",
      G_CALLBACK (skip_prev_clicked_cb), play);
  gtk_widget_set_sensitive (play->prev_button, FALSE);

  /* Skip forward button */
  play->next_button =
      gtk_button_new_from_icon_name ("media-skip-forward",
      GTK_ICON_SIZE_BUTTON);
  g_signal_connect (G_OBJECT (play->next_button), "clicked",
      G_CALLBACK (skip_next_clicked_cb), play);
  gtk_widget_set_sensitive (play->next_button, FALSE);

  /* Volume control button */
  play->volume_button = gtk_volume_button_new ();
  gtk_scale_button_set_value (GTK_SCALE_BUTTON (play->volume_button),
      gst_player_get_volume (play->player));
  g_signal_connect (G_OBJECT (play->volume_button), "value-changed",
      G_CALLBACK (volume_changed_cb), play);

  controls = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_start (GTK_BOX (controls), play->prev_button, FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (controls), play->play_pause_button, FALSE, FALSE,
      2);
  gtk_box_pack_start (GTK_BOX (controls), play->next_button, FALSE, FALSE, 2);
  gtk_box_pack_start (GTK_BOX (controls), play->seekbar, TRUE, TRUE, 2);
  gtk_box_pack_start (GTK_BOX (controls), play->volume_button, FALSE, FALSE, 2);

  main_hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_start (GTK_BOX (main_hbox), play->video_area, TRUE, TRUE, 0);

  main_vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_pack_start (GTK_BOX (main_vbox), main_hbox, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (main_vbox), play->info_bar, FALSE, FALSE, 0);
  gtk_box_pack_start (GTK_BOX (main_vbox), controls, FALSE, FALSE, 0);
  gtk_container_add (GTK_CONTAINER (play->window), main_vbox);

  gtk_widget_realize (play->video_area);

  gtk_widget_show_all (play->window);

  gtk_widget_hide (play->video_area);
}

static void
play_clear (GtkPlay * play)
{
  g_free (play->uri);
  g_list_free_full (play->uris, g_free);
  g_object_unref (play->player);
}

static void
duration_changed_cb (GstPlayer * unused, GstClockTime duration, GtkPlay * play)
{
  gtk_range_set_range (GTK_RANGE (play->seekbar), 0,
      (gdouble) duration / GST_SECOND);
}

static void
position_updated_cb (GstPlayer * unused, GstClockTime position, GtkPlay * play)
{
  g_signal_handler_block (play->seekbar, play->seekbar_value_changed_signal_id);
  gtk_range_set_value (GTK_RANGE (play->seekbar),
      (gdouble) position / GST_SECOND);
  g_signal_handler_unblock (play->seekbar,
      play->seekbar_value_changed_signal_id);
}

static void
video_dimensions_changed_cb (GstPlayer * unused, gint width, gint height,
    GtkPlay * play)
{
  if (width > 0 && height > 0)
    gtk_widget_show (play->video_area);
  else
    gtk_widget_hide (play->video_area);
}

static void
eos_cb (GstPlayer * unused, GtkPlay * play)
{
  if (play->playing) {
    GList *next = NULL;
    gchar *uri;

    next = g_list_find_custom (play->uris,
        gst_player_get_uri (play->player), (GCompareFunc) strcmp);

    g_return_if_fail (next != NULL);

    next = g_list_next (next);
    if (next) {
      if (!gtk_widget_is_sensitive (play->prev_button))
        gtk_widget_set_sensitive (play->prev_button, TRUE);
      gtk_widget_set_sensitive (play->next_button, g_list_next (next) != NULL);

      gst_player_set_uri (play->player, next->data);
      clear_missing_plugins (play);
      gst_player_play (play->player);
      set_title (play, next->data);
    } else {
      GtkWidget *image;

      gst_player_pause (play->player);
      image =
          gtk_image_new_from_icon_name ("media-playback-start",
          GTK_ICON_SIZE_BUTTON);
      gtk_button_set_image (GTK_BUTTON (play->play_pause_button), image);
      play->playing = FALSE;
    }
  }
}

static void
error_cb (GstPlayer * player, GError * err, GtkPlay * play)
{
  char *message;

  if (g_error_matches (err, gst_player_error_quark (),
      GST_PLAYER_ERROR_MISSING_PLUGIN)) {
    // add message to end of any existing message: there may be
    // multiple missing plugins.
    message = g_strdup_printf ("%s%s. ",
        gtk_label_get_text (GTK_LABEL (play->info_label)), err->message);
    gtk_label_set_text (GTK_LABEL (play->info_label), message);
    g_free (message);

    gtk_widget_show (play->info_bar);
  }
}

int
main (gint argc, gchar ** argv)
{
  GtkPlay play;
  gchar **file_names = NULL;
  GOptionContext *ctx;
  GOptionEntry options[] = {
    {G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &file_names,
        "Files to play"},
    {NULL}
  };
  guint list_length = 0;
  GError *err = NULL;
  GList *l;

  memset (&play, 0, sizeof (play));

  g_set_prgname (APP_NAME);

  ctx = g_option_context_new ("FILE|URI");
  g_option_context_add_main_entries (ctx, options, NULL);
  g_option_context_add_group (ctx, gtk_get_option_group (TRUE));
  g_option_context_add_group (ctx, gst_init_get_option_group ());
  if (!g_option_context_parse (ctx, &argc, &argv, &err)) {
    g_print ("Error initializing: %s\n", GST_STR_NULL (err->message));
    return 1;
  }
  g_option_context_free (ctx);

  // FIXME: Add support for playlists and stuff
  /* Parse the list of the file names we have to play. */
  if (!file_names) {
    GtkWidget *chooser;
    int res;

    chooser = gtk_file_chooser_dialog_new ("Select files to play", NULL,
        GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Cancel", GTK_RESPONSE_CANCEL, "_Open", GTK_RESPONSE_ACCEPT, NULL);
    g_object_set (chooser, "local-only", FALSE, "select-multiple", TRUE, NULL);

    res = gtk_dialog_run (GTK_DIALOG (chooser));
    if (res == GTK_RESPONSE_ACCEPT) {
      GSList *l;

      l = gtk_file_chooser_get_uris (GTK_FILE_CHOOSER (chooser));
      while (l) {
        play.uris = g_list_append (play.uris, l->data);
        l = g_slist_delete_link (l, l);
      }
    } else {
      return 0;
    }
    gtk_widget_destroy (chooser);
  } else {
    guint i;

    list_length = g_strv_length (file_names);
    for (i = 0; i < list_length; i++) {
      play.uris =
          g_list_append (play.uris,
          gst_uri_is_valid (file_names[i]) ?
          g_strdup (file_names[i]) : gst_filename_to_uri (file_names[i], NULL));
    }

    g_strfreev (file_names);
    file_names = NULL;
  }

  play.player = gst_player_new ();
  play.playing = TRUE;

  g_object_set (play.player, "dispatch-to-main-context", TRUE, NULL);

  gst_player_set_uri (play.player, g_list_first (play.uris)->data);

  create_ui (&play);

  if (list_length > 1)
    gtk_widget_set_sensitive (play.next_button, TRUE);

  g_signal_connect (play.player, "position-updated",
      G_CALLBACK (position_updated_cb), &play);
  g_signal_connect (play.player, "duration-changed",
      G_CALLBACK (duration_changed_cb), &play);
  g_signal_connect (play.player, "video-dimensions-changed",
      G_CALLBACK (video_dimensions_changed_cb), &play);
  g_signal_connect (play.player, "end-of-stream", G_CALLBACK (eos_cb), &play);
  g_signal_connect (play.player, "error", G_CALLBACK (error_cb), &play);

  /* We have file(s) that need playing. */
  set_title (&play, g_list_first (play.uris)->data);
  gst_player_play (play.player);

  gtk_main ();

  play_clear (&play);

  return 0;
}
