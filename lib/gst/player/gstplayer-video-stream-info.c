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

#include "gstplayer-video-stream-info.h"

struct _GstPlayerVideoStreamInfo
{
  gint index;
  GstTagList *tags;
};

G_DEFINE_BOXED_TYPE (GstPlayerVideoStreamInfo, gst_player_video_stream_info,
    gst_player_video_stream_info_copy, gst_player_video_stream_info_free);

GstPlayerVideoStreamInfo *
gst_player_video_stream_info_new (gint index, GstTagList * tags)
{
  GstPlayerVideoStreamInfo *info = NULL;

  info  = g_slice_new0 (GstPlayerVideoStreamInfo);
  info->index = index;
  info->tags = tags;

  return info;
}

void
gst_player_video_stream_info_free (GstPlayerVideoStreamInfo * videoinfo)
{
  videoinfo->index = -1;
  gst_tag_list_unref (videoinfo->tags);
  g_slice_free (GstPlayerVideoStreamInfo, videoinfo);
}

GstPlayerVideoStreamInfo *
gst_player_video_stream_info_copy (GstPlayerVideoStreamInfo * videoinfo)
{
  GstPlayerVideoStreamInfo *info = NULL;

  g_return_val_if_fail (videoinfo != NULL, NULL);

  info = gst_player_video_stream_info_new (videoinfo->index, videoinfo->tags);

  return info;
}

gint
gst_player_video_stream_info_get_index (GstPlayerVideoStreamInfo * videoinfo)
{
  g_return_val_if_fail (videoinfo != NULL, -1);

  return videoinfo->index;
}
