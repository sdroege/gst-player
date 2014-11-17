/* GStreamer
 *
 * Copyright (C) 2014 Partha Susarla <ajaysusarla@gmail.com>
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
#include "gstplayer-media-info-private.h"

/**
   GstPlayerStreamInfo
 */
G_DEFINE_TYPE (GstPlayerStreamInfo, gst_player_stream_info, G_TYPE_OBJECT)

static void
gst_player_stream_info_init (GstPlayerStreamInfo * info)
{
}

static void
gst_player_stream_info_finalize (GObject * object)
{
}

static void
gst_player_stream_info_class_init (GstPlayerStreamInfoClass * klass)
{
  GObjectClass *oclass = (GObjectClass *) klass;

  oclass->finalize = gst_player_stream_info_finalize;
}

/**
   GstPlayerAudioStreamInfo
 */
G_DEFINE_TYPE (GstPlayerAudioStreamInfo, gst_player_audio_stream_info,
    GST_TYPE_PLAYER_STREAM_INFO)

static void
gst_player_audio_stream_info_init (GstPlayerAudioStreamInfo * info)
{
}

static void
gst_player_audio_stream_info_finalize (GObject * object)
{
  GstPlayerAudioStreamInfo *info = GST_PLAYER_AUDIO_STREAM_INFO(object);

  info->index = -1;

  if (info->tags)
    g_clear_pointer(&info->tags, gst_tag_list_unref);

  G_OBJECT_CLASS (gst_player_audio_stream_info_parent_class)->finalize (object);
}

static void
gst_player_audio_stream_info_class_init (GstPlayerAudioStreamInfoClass * klass)
{
  GObjectClass *oclass = (GObjectClass *) klass;

  oclass->finalize = gst_player_audio_stream_info_finalize;
}

#if 0
static GstPlayerAudioStreamInfo *
gst_player_audio_stream_info_new (void)
{
  return (GstPlayerAudioStreamInfo *)
    g_object_new (GST_TYPE_PLAYER_AUDIO_STREAM_INFO, NULL);
}
#endif

/**
   GstPlayerVideoStreamInfo
 */
G_DEFINE_TYPE (GstPlayerVideoStreamInfo, gst_player_video_stream_info,
    GST_TYPE_PLAYER_STREAM_INFO)

static void
gst_player_video_stream_info_init (GstPlayerVideoStreamInfo * info)
{
}

static void
gst_player_video_stream_info_finalize (GObject * object)
{
  GstPlayerVideoStreamInfo *info = GST_PLAYER_VIDEO_STREAM_INFO(object);

  info->index = -1;

  if (info->tags)
    g_clear_pointer(&info->tags, gst_tag_list_unref);

  G_OBJECT_CLASS (gst_player_video_stream_info_parent_class)->finalize (object);
}

static void
gst_player_video_stream_info_class_init (GstPlayerVideoStreamInfoClass * klass)
{
  GObjectClass *oclass = (GObjectClass *) klass;

  oclass->finalize = gst_player_video_stream_info_finalize;
}

/**
   GstPlayerTextStreamInfo
 */
G_DEFINE_TYPE (GstPlayerTextStreamInfo, gst_player_text_stream_info,
    GST_TYPE_PLAYER_STREAM_INFO)

static void
gst_player_text_stream_info_init (GstPlayerTextStreamInfo * info)
{
}

static void
gst_player_text_stream_info_finalize (GObject * object)
{
  GstPlayerTextStreamInfo *info = GST_PLAYER_TEXT_STREAM_INFO(object);

  info->index = -1;

  if (info->tags)
    g_clear_pointer(&info->tags, gst_tag_list_unref);

  G_OBJECT_CLASS (gst_player_text_stream_info_parent_class)->finalize (object);
}

static void
gst_player_text_stream_info_class_init (GstPlayerTextStreamInfoClass * klass)
{
  GObjectClass *oclass = (GObjectClass *) klass;

  oclass->finalize = gst_player_text_stream_info_finalize;
}

/**
   GstPlayerMediaInfo
 */
G_DEFINE_TYPE (GstPlayerMediaInfo, gst_player_media_info, G_TYPE_OBJECT)

static void
gst_player_media_info_init (GstPlayerMediaInfo * info)
{
  info->total = -1;
  info->current = -1;
  info->tags = NULL;
  info->array = NULL;

  info->array = g_ptr_array_new_with_free_func
    ((GDestroyNotify) g_object_unref);
}

static void
gst_player_media_info_finalize (GObject * object)
{
  GstPlayerMediaInfo *info = GST_PLAYER_MEDIA_INFO(object);

  info->total = -1;
  info->current = -1;

  if (info->tags)
    g_clear_pointer(&info->tags, gst_tag_list_unref);

  if (info->array)
    g_ptr_array_free (info->array, TRUE);

  G_OBJECT_CLASS (gst_player_media_info_parent_class)->finalize (object);
}

static void
gst_player_media_info_class_init (GstPlayerMediaInfoClass * klass)
{
  GObjectClass *oclass = (GObjectClass *) klass;

  oclass->finalize = gst_player_media_info_finalize;
}

gint
gst_player_media_info_get_total (GstPlayerMediaInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAYER_MEDIA_INFO (info), -1);

  return info->total;
}

gint
gst_player_media_info_get_current (GstPlayerMediaInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAYER_MEDIA_INFO (info), -1);

  return info->current;
}

GPtrArray *
gst_player_media_info_get_streams (GstPlayerMediaInfo * info)
{
  g_return_val_if_fail (GST_IS_PLAYER_MEDIA_INFO (info), NULL);

  return info->array;
}

gboolean
gst_player_media_info_get_title (GstPlayerMediaInfo * info,
                                 gchar **val)
{
  gboolean ret;

  g_return_val_if_fail (GST_IS_PLAYER_MEDIA_INFO (info), FALSE);

  ret = gst_tag_list_get_string_index (info->tags, GST_TAG_TITLE, 0, val);

  return ret;
}
