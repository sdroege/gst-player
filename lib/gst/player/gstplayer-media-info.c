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

#include "gstplayer-media-info.h"
#include "gstplayer-audio-stream-info.h"
#include "gstplayer-video-stream-info.h"
#include "gstplayer-text-stream-info.h"

G_DEFINE_BOXED_TYPE (GstPlayerMediaInfo, gst_player_media_info,
    gst_player_media_info_copy, gst_player_media_info_free);

GstPlayerMediaInfo *
gst_player_media_info_new (GstPlayerMediaType type, const gchar * uri)
{
  GstPlayerMediaInfo *info = NULL;

  info = g_slice_new0 (GstPlayerMediaInfo);
  info->type = type;
  info->uri = uri ? g_strdup (uri) : NULL;

  if (type == GST_PLAYER_MEDIA_TYPE_AUDIO)
          info->array = g_ptr_array_new_with_free_func
                  ((GDestroyNotify)gst_player_audio_stream_info_free);
  else if (type == GST_PLAYER_MEDIA_TYPE_VIDEO)
          info->array = g_ptr_array_new_with_free_func
                  ((GDestroyNotify)gst_player_video_stream_info_free);
  else if (type == GST_PLAYER_MEDIA_TYPE_TEXT)
          info->array = g_ptr_array_new_with_free_func
                  ((GDestroyNotify)gst_player_text_stream_info_free);

  return info;
}

void
gst_player_media_info_free (GstPlayerMediaInfo * info)
{
  if (info->uri)
    g_free (info->uri);

  if (info->array)
    g_ptr_array_free (info->array, TRUE);

  if (info->tags)
    gst_tag_list_unref (info->tags);

  g_slice_free (GstPlayerMediaInfo, info);
}

static void
copy_array (gpointer data, gpointer user_data)
{
  g_ptr_array_add (user_data, data);
}

GstPlayerMediaInfo *
gst_player_media_info_copy (GstPlayerMediaInfo * info)
{
  GstPlayerMediaInfo *ret = NULL;

  g_return_val_if_fail ((info != NULL), NULL);

  ret = gst_player_media_info_new (info->type, info->uri);

  if (info->tags)
    ret->tags = gst_tag_list_copy (info->tags);

  ret->total = info->total;
  ret->current = info->current;

  g_ptr_array_foreach (info->array, copy_array, ret->array);

  return ret;
}

void
gst_player_media_info_append (GstPlayerMediaInfo * info, gpointer data)
{
  g_return_if_fail (info != NULL);

  g_ptr_array_add (info->array, data);
}

struct _GstPlayerMediaInfoIter {
  GstPlayerMediaInfo *info;
  int index;
};

void
gst_player_media_info_iter_init (GstPlayerMediaInfoIter * iter,
                                 GstPlayerMediaInfo * info)
{
  GstPlayerMediaInfoIter *i = iter;

  i->info = info;
  i->index = 0;
}

gboolean
gst_player_media_info_iter_next (GstPlayerMediaInfoIter * iter,
                                 const gpointer **data)
{
  GstPlayerMediaInfoIter *i = iter;

  if (i->index >= i->info->array->len)
    return FALSE;

  *data = i->info->array->pdata[i->index];
  i->index++;

  return TRUE;
}

void
gst_player_media_info_foreach (GstPlayerMediaInfo * info,
                               GstPlayerMediaInfoForEachFunc func,
                               gpointer user_data)
{
  int i;

  g_return_if_fail (info != NULL);

  for (i = 0; i < info->array->len; i++) {
    func (info->array->pdata[i], user_data);
  }
}
