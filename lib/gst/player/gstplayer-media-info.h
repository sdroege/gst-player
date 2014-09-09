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

#ifndef __GST_PLAYER_MEDIA_INFO_H__
#define __GST_PLAYER_MEDIA_INFO_H__

#include <gst/gst.h>

G_BEGIN_DECLS

/*
  GstPlayerStreamInfo
 */
#define GST_TYPE_PLAYER_STREAM_INFO     (gst_player_stream_info_get_type ())
#define GST_IS_PLAYER_STREAM_INFO(obj)  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_PLAYER_STREAM_INFO))
#define GST_PLAYER_STREAM_INFO(obj)     (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_PLAYER_STREAM_INFO, GstPlayerStreamInfo))

typedef struct _GstPlayerStreamInfo GstPlayerStreamInfo;
typedef struct _GstPlayerStreamInfoClass GstPlayerStreamInfoClass;
GType      gst_player_stream_info_get_type (void);

/*
  GstPlayerAudioStreamInfo
 */
#define GST_TYPE_PLAYER_AUDIO_STREAM_INFO     (gst_player_audio_stream_info_get_type ())
#define GST_IS_PLAYER_AUDIO_STREAM_INFO(obj)  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_PLAYER_AUDIO_STREAM_INFO))
#define GST_PLAYER_AUDIO_STREAM_INFO(obj)     (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_PLAYER_AUDIO_STREAM_INFO, GstPlayerAudioStreamInfo))

typedef struct _GstPlayerAudioStreamInfo GstPlayerAudioStreamInfo;
typedef struct _GstPlayerAudioStreamInfoClass GstPlayerAudioStreamInfoClass;
GType      gst_player_audio_stream_info_get_type (void);

/*
  GstPlayerVideoStreaminfo
 */
#define GST_TYPE_PLAYER_VIDEO_STREAM_INFO     (gst_player_video_stream_info_get_type ())
#define GST_IS_PLAYER_VIDEO_STREAM_INFO(obj)  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_PLAYER_VIDEO_STREAM_INFO))
#define GST_PLAYER_VIDEO_STREAM_INFO(obj)     (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_PLAYER_VIDEO_STREAM_INFO, GstPlayerVideoStreamInfo))

typedef struct _GstPlayerVideoStreamInfo GstPlayerVideoStreamInfo;
typedef struct _GstPlayerVideoStreamInfoClass GstPlayerVideoStreamInfoClass;
GType       gst_player_video_stream_info_get_type (void);

/*
  GstPlayerTextStreaminfo
 */
#define GST_TYPE_PLAYER_TEXT_STREAM_INFO     (gst_player_text_stream_info_get_type ())
#define GST_IS_PLAYER_TEXT_STREAM_INFO(obj)  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_PLAYER_TEXT_STREAM_INFO))
#define GST_PLAYER_TEXT_STREAM_INFO(obj)     (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_PLAYER_TEXT_STREAM_INFO, GstPlayerTextStreamInfo))

typedef struct _GstPlayerTextStreamInfo GstPlayerTextStreamInfo;
typedef struct _GstPlayerTextStreamInfoClass GstPlayerTextStreamInfoClass;
GType        gst_player_text_stream_info_get_type (void);

/*
  GstPlayerMediaInfo
 */
#define GST_TYPE_PLAYER_MEDIA_INFO     (gst_player_media_info_get_type ())
#define GST_IS_PLAYER_MEDIA_INFO(obj)  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_PLAYER_MEDIA_INFO))
#define GST_PLAYER_MEDIA_INFO(obj)     (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_PLAYER_MEDIA_INFO, GstPlayerMediaInfo))

typedef struct _GstPlayerMediaInfo GstPlayerMediaInfo;
typedef struct _GstPlayerMediaInfoClass GstPlayerMediaInfoClass;
GType        gst_player_media_info_get_type      (void);

gint         gst_player_media_info_get_total     (GstPlayerMediaInfo * info);
gint         gst_player_media_info_get_current   (GstPlayerMediaInfo * info);
GPtrArray *  gst_player_media_info_get_streams   (GstPlayerMediaInfo * info);
gboolean     gst_player_media_info_get_title     (GstPlayerMediaInfo * info,
                                                  gchar **val);

G_END_DECLS

#endif /* __GST_PLAYER_MEDIA_INFO_H__ */
