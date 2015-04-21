/* GStreamer
 *
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

#ifndef __GST_PLAYER_MEDIA_INFO_H__
#define __GST_PLAYER_MEDIA_INFO_H__

#include <gst/gst.h>

G_BEGIN_DECLS

#define GST_TYPE_PLAYER_STREAM_INFO \
  (gst_player_stream_info_get_type ())
#define GST_PLAYER_STREAM_INFO(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_PLAYER_STREAM_INFO,GstPlayerStreamInfo))
#define GST_IS_PLAYER_STREAM_INFO(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_PLAYER_STREAM_INFO))

typedef struct _GstPlayerStreamInfo GstPlayerStreamInfo;
typedef GObjectClass GstPlayerStreamInfoClass;
GType gst_player_stream_info_get_type (void);

gint          gst_player_stream_info_get_stream_index
                (const GstPlayerStreamInfo *info);
const gchar*  gst_player_stream_info_get_stream_type_nick
                (const GstPlayerStreamInfo *info);
GstTagList*   gst_player_stream_info_get_stream_tags
                (const GstPlayerStreamInfo *info);
GstCaps*      gst_player_stream_info_get_stream_caps
                (const GstPlayerStreamInfo *info);

#define GST_TYPE_PLAYER_VIDEO_INFO \
  (gst_player_video_info_get_type ())
#define GST_PLAYER_VIDEO_INFO(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_PLAYER_VIDEO_INFO, GstPlayerVideoInfo))
#define GST_IS_PLAYER_VIDEO_INFO(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_PLAYER_VIDEO_INFO))

typedef struct _GstPlayerVideoInfo GstPlayerVideoInfo;
typedef GObjectClass GstPlayerVideoInfoClass;
GType gst_player_video_info_get_type (void);

gint          gst_player_video_info_get_bitrate
                (const GstPlayerVideoInfo* info);
gint          gst_player_video_info_get_max_bitrate
                (const GstPlayerVideoInfo* info);
gint          gst_player_video_info_get_width
                (const GstPlayerVideoInfo* info);
gint          gst_player_video_info_get_height
                (const GstPlayerVideoInfo* info);
void          gst_player_video_info_get_framerate
                (const GstPlayerVideoInfo* info, gint *fps_n, gint *fps_d);
void          gst_player_video_info_get_pixel_aspect_ratio
                (const GstPlayerVideoInfo* info, guint *par_n, guint *par_d);

#define GST_TYPE_PLAYER_AUDIO_INFO \
  (gst_player_audio_info_get_type ())
#define GST_PLAYER_AUDIO_INFO(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_PLAYER_AUDIO_INFO, GstPlayerAudioInfo))
#define GST_IS_PLAYER_AUDIO_INFO(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_PLAYER_AUDIO_INFO))

typedef struct _GstPlayerAudioInfo GstPlayerAudioInfo;
typedef GObjectClass GstPlayerAudioInfoClass;
GType gst_player_audio_info_get_type (void);

gint          gst_player_audio_info_get_channels
                (const GstPlayerAudioInfo* info);
gint          gst_player_audio_info_get_sample_rate
                (const GstPlayerAudioInfo* info);
gint          gst_player_audio_info_get_bitrate
                (const GstPlayerAudioInfo* info);
gint          gst_player_audio_info_get_max_bitrate
                (const GstPlayerAudioInfo* info);
const gchar*  gst_player_audio_info_get_language
                (const GstPlayerAudioInfo* info);

#define GST_TYPE_PLAYER_SUBTITLE_INFO \
  (gst_player_subtitle_info_get_type ())
#define GST_PLAYER_SUBTITLE_INFO(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_PLAYER_SUBTITLE_INFO, GstPlayerSubtitleInfo))
#define GST_IS_PLAYER_SUBTITLE_INFO(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_PLAYER_SUBTITLE_INFO))

typedef struct _GstPlayerSubtitleInfo GstPlayerSubtitleInfo;
typedef GObjectClass GstPlayerSubtitleInfoClass;
GType gst_player_subtitle_info_get_type (void);

const gchar*  gst_player_subtitle_get_language
                (const GstPlayerSubtitleInfo* info);

#define GST_TYPE_PLAYER_MEDIA_INFO \
  (gst_player_media_info_get_type())
#define GST_PLAYER_MEDIA_INFO(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_PLAYER_MEDIA_INFO,GstPlayerMediaInfo))
#define GST_PLAYER_MEDIA_INFO_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_PLAYER_MEDIA_INFO,GstPlayerMediaInfoClass))
#define GST_IS_PLAYER_MEDIA_INFO(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_PLAYER_MEDIA_INFO))
#define GST_IS_PLAYER_MEDIA_INFO_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_PLAYER_MEDIA_INFO))

typedef struct _GstPlayerMediaInfo GstPlayerMediaInfo;
typedef GObjectClass GstPlayerMediaInfoClass;
GType gst_player_media_info_get_type (void);

const gchar*  gst_player_media_info_get_uri
                (const GstPlayerMediaInfo *info);
GstClockTime  gst_player_media_info_get_duration
                (const GstPlayerMediaInfo *info);
GList*        gst_player_media_info_get_stream_list
                (const GstPlayerMediaInfo *info);
void          gst_player_stream_info_list_free
                (GList *list);
GList*        gst_player_get_video_streams
                (const GstPlayerMediaInfo *info);
GList*        gst_player_get_audio_streams
                (const GstPlayerMediaInfo *info);
GList*        gst_player_get_subtitle_streams
                (const GstPlayerMediaInfo *info);

G_END_DECLS

#endif // __GST_PLAYER_MEDIA_INFO_H
