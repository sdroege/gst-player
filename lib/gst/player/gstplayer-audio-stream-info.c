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

#include "gstplayer-audio-stream-info.h"

struct _GstPlayerAudioStreamInfo
{
  gint index;
  GstTagList *tags;
};

G_DEFINE_BOXED_TYPE (GstPlayerAudioStreamInfo, gst_player_audio_stream_info,
    gst_player_audio_stream_info_copy, gst_player_audio_stream_info_free);


GstPlayerAudioStreamInfo *
gst_player_audio_stream_info_new (gint index, GstTagList * tags)
{
  GstPlayerAudioStreamInfo *info = NULL;

  info  = g_slice_new0 (GstPlayerAudioStreamInfo);
  info->index = index;

  if (tags)
    info->tags = gst_tag_list_ref (tags);

  return info;
}

void
gst_player_audio_stream_info_free (GstPlayerAudioStreamInfo * audioinfo)
{
  audioinfo->index = -1;
  gst_tag_list_unref (audioinfo->tags);
  g_slice_free (GstPlayerAudioStreamInfo, audioinfo);
}

GstPlayerAudioStreamInfo *
gst_player_audio_stream_info_copy (GstPlayerAudioStreamInfo * audioinfo)
{
  GstPlayerAudioStreamInfo *info = NULL;

  g_return_val_if_fail (audioinfo != NULL, NULL);

  info = gst_player_audio_stream_info_new (audioinfo->index, audioinfo->tags);

  return info;
}

gboolean
gst_player_audio_stream_info_get_title (GstPlayerAudioStreamInfo * audioinfo,
                                        gchar ** val)
{
  gboolean ret;

  g_return_val_if_fail (audioinfo != NULL, FALSE);

  ret = gst_tag_list_get_string (audioinfo->tags, GST_TAG_TITLE, val);

  return ret;
}

gboolean
gst_player_audio_stream_info_get_lang_code (GstPlayerAudioStreamInfo * audioinfo,
                                            gchar ** val)
{
  gboolean ret;

  g_return_val_if_fail (audioinfo != NULL, FALSE);

  ret = gst_tag_list_get_string (audioinfo->tags, GST_TAG_LANGUAGE_CODE, val);

  return ret;
}

gint
gst_player_audio_stream_info_get_index (GstPlayerAudioStreamInfo * audioinfo)
{
  g_return_val_if_fail (audioinfo != NULL, -1);

  return audioinfo->index;
}
