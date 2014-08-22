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

#ifndef __GST_PLAYER_TEXT_STREAM_INFO_H__
#define __GST_PLAYER_TEXT_STREAM_INFO_H__

#include <gst/gst.h>

G_BEGIN_DECLS

typedef struct _GstPlayerTextStreamInfo GstPlayerTextStreamInfo;

GType                gst_player_text_stream_info_get_type           (void);
#define GST_PLAYER_TYPE_TEXT_STREAM_INFO  (gst_player_text_stream_info_get_type())

GstPlayerTextStreamInfo * gst_player_text_stream_info_new         (gint index, GstTagList * tags);
void                      gst_player_text_stream_info_free        (GstPlayerTextStreamInfo * textinfo);
GstPlayerTextStreamInfo * gst_player_text_stream_info_copy        (GstPlayerTextStreamInfo * textinfo);
gint                      gst_player_text_stream_info_get_index   (GstPlayerTextStreamInfo * textinfo);

G_END_DECLS

#endif /* __GST_PLAYER_TEXT_STREAM_INFO_H__ */
