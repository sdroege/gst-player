/* GStreamer
 * Copyright (C) 2015 Brijes Singh <brijesh.ksingh@gmail.com>
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

struct _GstPlayerStreamInfo
{
  GObject parent;

  GstCaps *caps;
  gint stream_id;
  GstTagList  *tags;

  gpointer _gst_reserved[GST_PADDING];
};

struct _GstPlayerSubtitleInfo
{
  GstPlayerStreamInfo  parent;

  gchar *language;

  gpointer _gst_reserved[GST_PADDING];
};

struct _GstPlayerAudioInfo
{
  GstPlayerStreamInfo  parent;

  guint channels;
  guint sample_rate;
  guint depth;

  guint bitrate;
  guint max_bitrate;

  gchar *language;

  gpointer _gst_reserved[GST_PADDING];
};

struct _GstPlayerVideoInfo
{
  GstPlayerStreamInfo  parent;

  guint width;
  guint height;
  guint framerate_num;
  guint framerate_denom;

  guint bitrate;
  guint max_bitrate;

  gpointer _gst_reserved[GST_PADDING];
};

struct _GstPlayerMediaInfo
{
  GObject parent;

  gchar *uri;

  /* all the streams */
  GList *stream_list;

  GstClockTime  duration;
};

