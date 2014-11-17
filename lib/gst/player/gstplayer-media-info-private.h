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

struct _GstPlayerStreamInfo
{
  GObject parent;
};

struct _GstPlayerStreamInfoClass
{
  GObjectClass parent;
};

struct _GstPlayerAudioStreamInfo
{
  GstPlayerStreamInfo parent;

  gint index;
  GstTagList *tags;
};

struct _GstPlayerAudioStreamInfoClass
{
  GstPlayerStreamInfoClass parent;
};

struct _GstPlayerVideoStreamInfo
{
  GstPlayerStreamInfo parent;

  gint index;
  GstTagList *tags;
};

struct _GstPlayerVideoStreamInfoClass
{
  GstPlayerStreamInfoClass parent;
};

struct _GstPlayerTextStreamInfo
{
  GstPlayerStreamInfo parent;

  gint index;
  GstTagList *tags;
};

struct _GstPlayerTextStreamInfoClass
{
  GstPlayerStreamInfoClass parent;
};

struct _GstPlayerMediaInfo
{
  GObject parent;

  gint total;
  gint current;
  gchar *uri;
  GstTagList *tags;

  GPtrArray *array;
};

struct _GstPlayerMediaInfoClass
{
  GObjectClass parent_class;
};
