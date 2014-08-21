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

#ifndef __GST_PLAYER_MEDIA_INFO_H__
#define __GST_PLAYER_MEDIA_INFO_H__

#include <gst/gst.h>

#include "gstplayer-audio-stream-info.h"
#include "gstplayer-video-stream-info.h"
#include "gstplayer-text-stream-info.h"

G_BEGIN_DECLS

typedef enum
{
  GST_PLAYER_MEDIA_TYPE_AUDIO,
  GST_PLAYER_MEDIA_TYPE_VIDEO,
  GST_PLAYER_MEDIA_TYPE_TEXT,
} GstPlayerMediaType;

typedef struct _GstPlayerMediaInfo GstPlayerMediaInfo;
typedef struct _GstPlayerMediaInfoIter GstPlayerMediaInfoIter;

struct _GstPlayerMediaInfo
{
  GPtrArray *array;
  GstPlayerMediaType type;

  GstTagList *tags;

  gchar *uri;
  gint total;
  gint current;
};


GType                gst_player_media_info_get_type           (void);
#define GST_PLAYER_TYPE_MEDIA_INFO  (gst_player_media_info_get_type())

typedef void       (*GstPlayerMediaInfoForEachFunc) (gpointer * streaminfo,
                                                     gpointer user_data);

GstPlayerMediaInfo * gst_player_media_info_new      (GstPlayerMediaType type,
                                                     const gchar * uri);
void                 gst_player_media_info_free     (GstPlayerMediaInfo * info);
GstPlayerMediaInfo * gst_player_media_info_copy     (GstPlayerMediaInfo * info);
void                 gst_player_media_info_append   (GstPlayerMediaInfo *info,
                                                     gpointer data);
void                 gst_player_media_info_iter_init (GstPlayerMediaInfoIter * iter,
                                                      GstPlayerMediaInfo * info);
gboolean             gst_player_media_info_iter_next (GstPlayerMediaInfoIter * iter,
                                                      const gpointer **data);
void                 gst_player_media_info_foreach  (GstPlayerMediaInfo * info,
                                                     GstPlayerMediaInfoForEachFunc func,
                                                     gpointer user_data);
G_END_DECLS

#endif /* __GST_PLAYER_MEDIA_INFO_H__ */
