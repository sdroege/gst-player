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

#ifndef __GST_PLAYER_AUDIO_STREAM_INFO_H__
#define __GST_PLAYER_AUDIO_STREAM_INFO_H__

#include <gst/gst.h>

G_BEGIN_DECLS

typedef struct _GstPlayerAudioStreamInfo GstPlayerAudioStreamInfo;

GType                gst_player_audio_stream_info_get_type           (void);
#define GST_PLAYER_TYPE_AUDIO_STREAM_INFO  (gst_player_audio_stream_info_get_type())
#define GST_IS_PLAYER_TYPE_AUDIO_STREAM_INFO(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_PLAYER_TYPE_AUDIO_STREAM_INFO))

GstPlayerAudioStreamInfo * gst_player_audio_stream_info_new         (gint index, GstTagList * tags);
void                       gst_player_audio_stream_info_free        (GstPlayerAudioStreamInfo * audioinfo);
GstPlayerAudioStreamInfo * gst_player_audio_stream_info_copy        (GstPlayerAudioStreamInfo * audioinfo);
gint                       gst_player_audio_stream_info_get_index   (GstPlayerAudioStreamInfo * audioinfo);
gboolean                   gst_player_audio_stream_info_get_title   (GstPlayerAudioStreamInfo * audioinfo,
                                                                     gchar ** val);
gboolean                   gst_player_audio_stream_info_get_lang_code (GstPlayerAudioStreamInfo * audioinfo,
                                                                       gchar ** val);
G_END_DECLS

#endif /* __GST_PLAYER_AUDIO_STREAM_INFO_H__ */
