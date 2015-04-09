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
#include <gst/player/player-media-info.h>

#define APP_NAME "gtk-play"

typedef struct
{
  GstPlayer *player;
  GstPlayerMediaInfo *media_info;
  gchar *uri;

  GList *uris;

  GtkWidget *window;
  GtkWidget *play_pause_button;
  GtkWidget *prev_button, *next_button;
  GtkWidget *seekbar;
  GtkWidget *video_area;
  GtkWidget *volume_button;
  gulong seekbar_value_changed_signal_id;
  gboolean playing;
} GtkPlay;

enum {
  COL_TEXT = 0,
  NUM_COLS
};

enum {
  VIDEO_INFO_START = 0,
  VIDEO_INFO_TYPE_NICK,
  VIDEO_INFO_STREAM_ID,
  VIDEO_INFO_RESOLUTION,
  VIDEO_INFO_FRAMERATE,
  VIDEO_INFO_CODEC,
  VIDEO_INFO_END,
  AUDIO_INFO_START,
  AUDIO_INFO_TYPE_NICK,
  AUDIO_INFO_STREAM_ID,
  AUDIO_INFO_SAMPLE_RATE,
  AUDIO_INFO_CHANNELS,
  AUDIO_INFO_CODEC,
  AUDIO_INFO_LANGUAGE,
  AUDIO_INFO_END,
  SUBTITLE_INFO_START,
  SUBTITLE_INFO_TYPE_NICK,
  SUBTITLE_INFO_STREAM_ID,
  SUBTITLE_INFO_LANGUAGE,
  SUBTITLE_INFO_END
};

enum {
  GTK_AUDIO_POPUP_SUBMENU = 0,
  GTK_VIDEO_POPUP_SUBMENU,
  GTK_SUBTITLE_POPUP_SUBMENU
};

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

typedef struct _tagsdata {
  GtkTreeStore *tree;
  GtkTreeIter parent;
}TagsData;

static void
get_one_tag (const GstTagList *list, const gchar *tag, gpointer user_data)
{
  gint i, num;
  gchar *buffer = NULL;
  TagsData *data = (TagsData*) user_data;
  GtkTreeIter child;

  num = gst_tag_list_get_tag_size (list, tag);
  for (i = 0; i < num; ++i) {
    const GValue *val;

    val = gst_tag_list_get_value_index (list, tag, i);
    if (G_VALUE_HOLDS_STRING (val)) {
      buffer = g_strdup_printf ("%s : %s", tag, g_value_get_string (val));
    }
    else if (G_VALUE_HOLDS_UINT (val)) {
      buffer = g_strdup_printf ("%s : %u", tag, g_value_get_uint (val));
    }
    else if (G_VALUE_HOLDS_DOUBLE (val)) {
      buffer = g_strdup_printf ("%s : %g", tag, g_value_get_double (val));
    }
    else if (G_VALUE_HOLDS_BOOLEAN (val)) {
      buffer = g_strdup_printf ("%s : %s", tag,
                g_value_get_boolean (val) ? "true" : "false");
    }
    else if (GST_VALUE_HOLDS_DATE_TIME (val)) {
      GstDateTime *dt = g_value_get_boxed (val);
      gchar *dt_str = gst_date_time_to_iso8601_string (dt);

      buffer = g_strdup_printf ("%s : %s", tag, dt_str);
      g_free (dt_str);
    }
    else {
      buffer = g_strdup_printf ("%s : tag of type '%s'",
          tag, G_VALUE_TYPE_NAME (val));
    }
  }

  gtk_tree_store_append (data->tree, &child, &data->parent);
  gtk_tree_store_set (data->tree, &child, COL_TEXT, buffer, -1);
  g_free (buffer);
}

static void
media_info_insert_taglist (GstPlayerStreamInfo *info, GtkTreeStore *tree,
  GtkTreeIter parent)
{
  GstTagList  *tags;
  TagsData  data;
  GtkTreeIter child;

  gtk_tree_store_append (tree, &child, &parent);
  gtk_tree_store_set (tree, &child, COL_TEXT, "Taglist", -1);

  data.tree = tree;
  data.parent = child;
  tags = gst_player_stream_info_get_stream_tags (info);
  gst_tag_list_foreach (tags, get_one_tag, &data);
}

static gchar*
media_info_get_string (GstPlayerStreamInfo *info, gint type,
  gboolean  label)
{
  gchar *buffer = NULL;

  switch (type) {
    case VIDEO_INFO_RESOLUTION:
    {
      GstPlayerVideoInfo  *i = (GstPlayerVideoInfo*) info;
      buffer = g_strdup_printf ("%s%u x %u",
                label ? "Resolution : ":"",
                gst_player_video_info_get_width (i),
                gst_player_video_info_get_height (i));
      break;
    }
    case VIDEO_INFO_FRAMERATE:
    {
      GstPlayerVideoInfo  *i = (GstPlayerVideoInfo*) info;
      buffer = g_strdup_printf ("%s%.2f",
                label ? "Framerate : ":"", (gdouble)
                gst_player_video_info_get_framerate_num (i) /
                gst_player_video_info_get_framerate_denom (i));
      break;
    }
    case VIDEO_INFO_CODEC:
    {
      gchar *codec;
      GstTagList *tags = gst_player_stream_info_get_stream_tags (info);
      if (tags) {
        gst_tag_list_get_string (tags, GST_TAG_VIDEO_CODEC, &codec);
        buffer = g_strdup_printf ("%s%s",
                    label ? "Codec : ":"",
                    codec);
        g_free (codec);
        gst_tag_list_free (tags);
      }
      break;
    }
    case AUDIO_INFO_SAMPLE_RATE:
    {
      GstPlayerAudioInfo  *i = (GstPlayerAudioInfo*) info;
      buffer = g_strdup_printf ("%s%u",
                label ? "Sample rate : ":"",
                gst_player_audio_info_get_sample_rate (i));
      break;
    }
    case AUDIO_INFO_CHANNELS:
    {
      GstPlayerAudioInfo  *i = (GstPlayerAudioInfo*) info;
      buffer = g_strdup_printf ("%s%u",
                label ? "Channels : ":"",
                gst_player_audio_info_get_channels (i));
      break;
    }
    case AUDIO_INFO_CODEC:
    {
      gchar *codec;
      GstTagList *tags = gst_player_stream_info_get_stream_tags (info);
      if (tags) {
        gst_tag_list_get_string (tags, GST_TAG_AUDIO_CODEC, &codec);
        buffer = g_strdup_printf ("%s%s",
                    label ? "Codec : ":"",
                    codec);
        g_free (codec);
        gst_tag_list_free (tags);
      }
      break;
    }
    case AUDIO_INFO_LANGUAGE:
    {
      GstPlayerAudioInfo  *i = (GstPlayerAudioInfo*) info;
      buffer = g_strdup_printf ("%s%s",
                  label ? "Language : ":"",
                  gst_player_audio_info_get_language (i));
      break;
    }
    case SUBTITLE_INFO_LANGUAGE:
    {
      GstPlayerSubtitleInfo  *i = (GstPlayerSubtitleInfo*) info;
      buffer = g_strdup_printf ("%s%s",
                  label ? "Language : ":"",
                  gst_player_subtitle_info_get_language (i));
      break;
    }
    case VIDEO_INFO_TYPE_NICK:
    case AUDIO_INFO_TYPE_NICK:
    case SUBTITLE_INFO_TYPE_NICK:
    {
      buffer = g_strdup_printf ("%s%s",
                  label ? "Type : ":"",
                  gst_player_stream_info_get_stream_type_nick (info));
      break;
    }
    case VIDEO_INFO_STREAM_ID:
    case AUDIO_INFO_STREAM_ID:
    case SUBTITLE_INFO_STREAM_ID:
    {
      buffer = g_strdup_printf ("%s%d",
                  label ? "ID : ":"",
                gst_player_stream_info_get_stream_id (info));
      break;
    }
    default:
      break;
  }

  return buffer;
}

static void
add_stream_info (GtkTreeStore *tree, GtkTreeIter parent,
  GstPlayerStreamInfo *stream)
{
  gint i;
  gchar *buffer = NULL;
  gint start = 0, end = 0;
  GtkTreeIter child;

  if (GST_IS_PLAYER_VIDEO_INFO (stream)) {
    start = VIDEO_INFO_START + 1;
    end = VIDEO_INFO_END;
  }

  if (GST_IS_PLAYER_AUDIO_INFO (stream)) {
    start = AUDIO_INFO_START + 1;
    end = AUDIO_INFO_END;
  }

  if (GST_IS_PLAYER_SUBTITLE_INFO (stream)) {
    start = SUBTITLE_INFO_START + 1;
    end = SUBTITLE_INFO_END;
  }

  for (i = start; i < end; i++) {
    buffer = media_info_get_string (stream, i, TRUE);
    gtk_tree_store_append (tree, &child, &parent);
    gtk_tree_store_set (tree, &child, COL_TEXT, buffer, -1);
    g_free (buffer);
  }

  media_info_insert_taglist (stream, tree, parent);
}

static GtkTreeModel *
create_and_fill_model (GstPlayerMediaInfo *info,
  GstPlayerStreamInfo *sinfo)
{
  guint count = 0;
  GList *list, *l;
  gchar *buffer;
  GtkTreeStore *tree = NULL;
  GtkTreeIter iter, parent;

  tree = gtk_tree_store_new (NUM_COLS, G_TYPE_STRING);

  list = gst_player_media_info_get_stream_list (info);
  for (l = list; l != NULL; l = l->next) {
    GstPlayerStreamInfo *stream = (GstPlayerStreamInfo*) l->data;

    /* check for matching stream */
    if (sinfo) {
      const gchar *type = gst_player_stream_info_get_stream_type_nick (sinfo);
      gint id = gst_player_stream_info_get_stream_id (sinfo);
      if (strcmp(gst_player_stream_info_get_stream_type_nick(stream), type) ||
            (gst_player_stream_info_get_stream_id(stream) != id)) {
        count++;
        continue;
      }
    }

    buffer = g_strdup_printf ("Stream %d", count++);
    gtk_tree_store_append (tree, &iter, NULL);
    gtk_tree_store_set (tree, &iter, COL_TEXT, buffer, -1);
    g_free (buffer);

    add_stream_info (tree, iter, stream);
  }

  return GTK_TREE_MODEL(tree);
}

static GtkWidget*
create_view_and_model (GstPlayerMediaInfo *info,
  GstPlayerStreamInfo *stream)
{
  GtkTreeViewColumn *col;
  GtkCellRenderer *renderer;
  GtkWidget *view;
  GtkTreeModel *model;

  view = gtk_tree_view_new ();
  col = gtk_tree_view_column_new ();
  gtk_tree_view_append_column (GTK_TREE_VIEW(view), col);
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW(view), FALSE);

  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (col, renderer, TRUE);
  gtk_tree_view_column_add_attribute (col, renderer,
                                      "text", COL_TEXT);

  model = create_and_fill_model (info, stream);
  gtk_tree_view_set_model (GTK_TREE_VIEW(view), model);
  g_object_unref(model);

  return view;
}

static void
media_information_dialog_create (GtkWidget *unused, GtkPlay *play,
  GstPlayerStreamInfo *stream, gchar *title, gchar *msg)
{
  GtkWidget *sw;
  GtkWidget *vbox, *hbox;
  GtkWidget *view;
  GtkWidget *window;
  GtkWidget *label, *label1;
  GtkWidget *location_view;
  GtkTextBuffer *buffer;
  GtkTextIter iter;

  if (play->media_info) {

    window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title (GTK_WINDOW(window), title);
    gtk_window_set_default_size (GTK_WINDOW(window), 650, 400);
    gtk_window_set_position (GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    gtk_container_set_border_width (GTK_CONTAINER(window), 8);

    vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_add (GTK_CONTAINER(window), vbox);

    label = gtk_label_new (msg);
    gtk_label_set_justify (GTK_LABEL(label), GTK_JUSTIFY_LEFT);
    gtk_misc_set_alignment (GTK_MISC(label),0.0,0.5);
    gtk_box_pack_start (GTK_BOX(vbox), label, FALSE, FALSE, 0);

    sw = gtk_scrolled_window_new (NULL, NULL);
    gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW(sw),
                  GTK_SHADOW_ETCHED_IN);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(sw),
                  GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start (GTK_BOX(vbox), sw, TRUE, TRUE, 0);

    view = create_view_and_model (play->media_info, stream);
    gtk_tree_selection_set_mode (
        gtk_tree_view_get_selection(GTK_TREE_VIEW(view)),
        GTK_SELECTION_MULTIPLE);

    gtk_container_add (GTK_CONTAINER(sw), view);
    g_signal_connect (view, "realize",
        G_CALLBACK(gtk_tree_view_expand_all), NULL);

    hbox = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start (GTK_BOX(vbox), hbox, FALSE, FALSE, 10);

    label1 = gtk_label_new (" Location: ");
    gtk_box_pack_start (GTK_BOX(hbox), label1, FALSE, FALSE, 0);

    buffer = gtk_text_buffer_new (NULL);
    gtk_text_buffer_get_start_iter (buffer, &iter);
    gtk_text_buffer_insert (buffer, &iter,
            gst_player_media_info_get_uri (play->media_info), -1);
    location_view = gtk_text_view_new_with_buffer (buffer);
    gtk_box_pack_start (GTK_BOX(hbox), location_view, FALSE, FALSE, 0);
    gtk_text_view_set_editable (GTK_TEXT_VIEW(location_view), FALSE);
    g_object_unref (buffer);

    gtk_widget_show_all (window);
    return;
  }

  g_print ("ERROR: media information is not available \n");
}

static void
show_media_information_cb (GtkWidget *unused, GtkPlay *play)
{
  media_information_dialog_create (unused, play, NULL,
    "Current media information",
    "Information about all the streams contained in your current media.\n");
}

static void
current_video_information_cb (GtkWidget *unused, GtkPlay *play)
{
  GstPlayerStreamInfo *stream;

  stream = (GstPlayerStreamInfo*)
            gst_player_media_info_get_current_video (play->media_info);
  media_information_dialog_create (unused, play, stream,
    "Current video",
    "Information about the current video stream used.\n");
}

static void
current_audio_information_cb (GtkWidget *unused, GtkPlay *play)
{
  GstPlayerStreamInfo *stream;

  stream = (GstPlayerStreamInfo*)
            gst_player_media_info_get_current_audio (play->media_info);
  media_information_dialog_create (unused, play, stream,
    "Current audio",
    "Information about the current audio stream used.\n");
}

static void
current_subtitle_information_cb (GtkWidget *unused, GtkPlay *play)
{
  GstPlayerStreamInfo *stream;

  stream = (GstPlayerStreamInfo*)
            gst_player_media_info_get_current_subtitle (play->media_info);
  media_information_dialog_create (unused, play, stream,
    "Current subtitle",
    "Information about the current subtitle stream used.\n");
}

static void
track_disable_cb (GtkWidget *widget, GtkPlay *play)
{
  guint menu_type;

  menu_type = GPOINTER_TO_INT(g_object_get_data (G_OBJECT(widget), "menu-type"));

  if (menu_type == GTK_AUDIO_POPUP_SUBMENU)
    gst_player_audio_track_disable (play->player);

  if (menu_type == GTK_VIDEO_POPUP_SUBMENU)
    gst_player_video_track_disable (play->player);

  if (menu_type == GTK_SUBTITLE_POPUP_SUBMENU)
    gst_player_subtitle_track_disable (play->player);
}

static void
track_selection_cb (GtkWidget *widget, GtkPlay *play)
{
  guint id;
  guint menu_type;
  GList *list = NULL, *l;
  GstPlayerStreamInfo *stream;
  GstPlayerMediaInfo *info = play->media_info;

  id = GPOINTER_TO_UINT(g_object_get_data (G_OBJECT(widget), "stream-id"));
  menu_type = GPOINTER_TO_INT(g_object_get_data (G_OBJECT(widget), "menu-type"));

  if (menu_type == GTK_AUDIO_POPUP_SUBMENU)
    list = gst_player_media_info_get_audio_streams (info);

  if (menu_type == GTK_VIDEO_POPUP_SUBMENU)
    list = gst_player_media_info_get_video_streams (info);

  if (menu_type == GTK_SUBTITLE_POPUP_SUBMENU)
    list = gst_player_media_info_get_subtitle_streams (info);

  for (l = list; l != NULL; l = l->next) {
    guint stream_id;

    stream = (GstPlayerStreamInfo*) l->data;
    stream_id = gst_player_stream_info_get_stream_id (stream);

    if (stream_id == id) {
      gst_player_set_stream (play->player, stream);

      /* if track is disabled then enable it */
      if (menu_type == GTK_AUDIO_POPUP_SUBMENU)
        gst_player_audio_track_enable (play->player);
      else if (menu_type == GTK_VIDEO_POPUP_SUBMENU)
        gst_player_video_track_enable (play->player);
      else
        gst_player_subtitle_track_enable (play->player);

      break;
    }
  }

  gst_player_media_info_stream_info_list_free (list);
}

static GtkWidget*
tracks_popup_menu_create (GtkPlay *play, gint type)
{
  gchar *buffer;
  GList *list, *l;
  GtkWidget *menu;
  GtkWidget *track, *disable, *sep;
  GstPlayerStreamInfo *stream;
  GstPlayerMediaInfo  *info = play->media_info;

  list = gst_player_media_info_get_stream_list (info);

  menu = gtk_menu_new ();

  for (l = list; l != NULL; l = l->next) {
    buffer = NULL;
    stream = (GstPlayerStreamInfo*) l->data;

    if (GST_IS_PLAYER_AUDIO_INFO (stream) &&
        (type == GTK_AUDIO_POPUP_SUBMENU)) {
      gchar *p1, *p2;
      p1 = media_info_get_string (stream, AUDIO_INFO_CODEC, FALSE);
      p2 = media_info_get_string (stream, AUDIO_INFO_LANGUAGE, FALSE);
      buffer = g_strdup_printf ("%s [%s]", p1, p2);
      g_free (p1);
      g_free (p2);
    }

    if (GST_IS_PLAYER_VIDEO_INFO (stream) &&
        (type == GTK_VIDEO_POPUP_SUBMENU)) {
      buffer = media_info_get_string (stream, VIDEO_INFO_CODEC, FALSE);
    }

    if (GST_IS_PLAYER_SUBTITLE_INFO (stream) &&
        (type == GTK_SUBTITLE_POPUP_SUBMENU)) {
      buffer = media_info_get_string (stream, SUBTITLE_INFO_LANGUAGE, FALSE);
    }

    if (buffer) {
      track = gtk_menu_item_new_with_label (buffer);
      g_object_set_data (G_OBJECT(track), "stream-id",
          GUINT_TO_POINTER(gst_player_stream_info_get_stream_id (stream)));
      g_object_set_data (G_OBJECT(track), "menu-type",
          GINT_TO_POINTER(type));
      g_signal_connect (G_OBJECT(track), "activate",
          G_CALLBACK (track_selection_cb), play);
      gtk_menu_shell_append (GTK_MENU_SHELL(menu), track);

      g_free (buffer);
    }
  }

  sep = gtk_separator_menu_item_new ();
  disable = gtk_menu_item_new_with_label ("Disable");
  g_object_set_data (G_OBJECT(disable), "menu-type",
      GINT_TO_POINTER(type));
  g_signal_connect (G_OBJECT(disable), "activate",
    G_CALLBACK (track_disable_cb), play);
  gtk_menu_shell_append (GTK_MENU_SHELL(menu), sep);
  gtk_menu_shell_append (GTK_MENU_SHELL(menu), disable);

  gst_player_media_info_stream_info_list_free (list);

  return menu;
}

static GtkWidget*
popup_submenu_create (GtkPlay *play, gint type)
{
  GtkWidget *menu;
  GtkWidget *tracks;
  GtkWidget *current;

  menu = gtk_menu_new ();

  if (type == GTK_AUDIO_POPUP_SUBMENU) {
    tracks = gtk_menu_item_new_with_label ("Audio tracks");
    current = gtk_menu_item_new_with_label ("Current audio");
    g_signal_connect (G_OBJECT(current), "activate",
        G_CALLBACK (current_audio_information_cb), play);
  }

  if (type == GTK_VIDEO_POPUP_SUBMENU) {
    tracks = gtk_menu_item_new_with_label ("Video tracks");
    current = gtk_menu_item_new_with_label ("Current video");
    g_signal_connect (G_OBJECT(current), "activate",
        G_CALLBACK (current_video_information_cb), play);
  }

  if (type == GTK_SUBTITLE_POPUP_SUBMENU) {
    tracks = gtk_menu_item_new_with_label ("Subtitle tracks");
    current = gtk_menu_item_new_with_label ("Current subtitle");
    g_signal_connect (G_OBJECT(current), "activate",
        G_CALLBACK (current_subtitle_information_cb), play);
  }

  gtk_menu_item_set_submenu (GTK_MENU_ITEM(tracks),
    tracks_popup_menu_create (play, type));

  gtk_menu_shell_append (GTK_MENU_SHELL(menu), tracks);
  gtk_menu_shell_append (GTK_MENU_SHELL(menu), current);

  return menu;
}

static GtkWidget*
right_press_popup_menu_create (GtkPlay *play)
{
  GtkWidget *menu;
  GtkWidget *info_menu;
  GtkWidget *audio_menu, *audio_submenu;
  GtkWidget *video_menu, *video_submenu;
  GtkWidget *subtitle_menu, *subtitle_submenu;

  menu = gtk_menu_new ();

  /* media information */
  info_menu = gtk_menu_item_new_with_label ("Media Information");
  g_signal_connect (G_OBJECT(info_menu), "activate",
                    G_CALLBACK (show_media_information_cb), play);

  /* audio */
  audio_menu = gtk_menu_item_new_with_label ("Audio");
  audio_submenu = popup_submenu_create (play, GTK_AUDIO_POPUP_SUBMENU);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM(audio_menu), audio_submenu);

  /* video */
  video_menu = gtk_menu_item_new_with_label ("Video");
  video_submenu = popup_submenu_create (play, GTK_VIDEO_POPUP_SUBMENU);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM(video_menu), video_submenu);

  /* subtitle */
  subtitle_menu = gtk_menu_item_new_with_label ("Subtitle");
  subtitle_submenu = popup_submenu_create (play, GTK_SUBTITLE_POPUP_SUBMENU);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM(subtitle_menu), subtitle_submenu);

  gtk_menu_shell_append (GTK_MENU_SHELL(menu), audio_menu);
  gtk_menu_shell_append (GTK_MENU_SHELL(menu), video_menu);
  gtk_menu_shell_append (GTK_MENU_SHELL(menu), subtitle_menu);
  gtk_menu_shell_append (GTK_MENU_SHELL(menu), info_menu);

  return menu;
}

static void
video_area_button_pressed_cb (GtkWidget *unused, GdkEventButton *event,
  GtkPlay *play)
{
  GtkWidget *menu = NULL;

  if ((event->type == GDK_BUTTON_PRESS) && (event->button == 3)) {
    menu = right_press_popup_menu_create (play);

    if (menu) {
      gtk_widget_show_all (menu);
      gtk_menu_popup (GTK_MENU(menu), NULL, NULL, NULL, NULL,
                      (event != NULL) ? event->button : 0,
                      gdk_event_get_time((GdkEvent*)event));
    }
  }
}

static void
media_info_cb (GstPlayer *unused, GstPlayerMediaInfo *info, GtkPlay *play)
{
  play->media_info  = info;
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

static void
create_ui (GtkPlay * play)
{
  GtkWidget *controls, *main_hbox, *main_vbox;

  play->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  g_signal_connect (G_OBJECT (play->window), "delete-event",
      G_CALLBACK (delete_event_cb), play);
  set_title (play, APP_NAME);

  play->video_area = gtk_drawing_area_new ();
  gtk_widget_set_double_buffered (play->video_area, FALSE);
  g_signal_connect (play->video_area, "realize",
      G_CALLBACK (video_area_realize_cb), play);
  g_signal_connect (play->video_area, "button-press-event",
      G_CALLBACK (video_area_button_pressed_cb), play);
  gtk_widget_set_events (play->video_area, GDK_EXPOSURE_MASK
                        | GDK_LEAVE_NOTIFY_MASK
                        | GDK_BUTTON_PRESS_MASK
                        | GDK_POINTER_MOTION_MASK
                        | GDK_POINTER_MOTION_HINT_MASK);

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
  g_signal_connect (play.player, "media-info-updated",
      G_CALLBACK (media_info_cb), &play);

  /* We have file(s) that need playing. */
  set_title (&play, g_list_first (play.uris)->data);
  gst_player_play (play.player);

  gtk_main ();

  play_clear (&play);

  return 0;
}
