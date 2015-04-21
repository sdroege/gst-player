/* GStreamer
 *
 * Copyright (C) 2014-2015 Sebastian Dröge <sebastian@centricular.com>
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

#ifndef __GST_PLAYER_H__
#define __GST_PLAYER_H__

#include <gst/gst.h>
#include <gst/player/gstplayer-media-info.h>

G_BEGIN_DECLS

GType        gst_player_state_get_type                (void);
#define      GST_TYPE_PLAYER_STATE                    (gst_player_state_get_type ())

typedef enum
{
  GST_PLAYER_STATE_STOPPED,
  GST_PLAYER_STATE_BUFFERING,
  GST_PLAYER_STATE_PAUSED,
  GST_PLAYER_STATE_PLAYING
} GstPlayerState;

const gchar *gst_player_state_get_name                (GstPlayerState state);

GQuark       gst_player_error_quark                   (void);
GType        gst_player_error_get_type                (void);
#define      GST_PLAYER_ERROR                         (gst_player_error_quark ())
#define      GST_TYPE_PLAYER_ERROR                    (gst_player_error_get_type ())

typedef enum {
  GST_PLAYER_ERROR_FAILED = 0
} GstPlayerError;

const gchar *gst_player_error_get_name                (GstPlayerError error);

typedef struct _GstPlayer GstPlayer;
typedef struct _GstPlayerClass GstPlayerClass;
typedef struct _GstPlayerPrivate GstPlayerPrivate;

struct _GstPlayer
{
  GstObject parent;

  GstPlayerPrivate *priv;
};

struct _GstPlayerClass
{
  GstObjectClass parent_class;
};

#define GST_TYPE_PLAYER             (gst_player_get_type ())
#define GST_IS_PLAYER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_PLAYER))
#define GST_IS_PLAYER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_PLAYER))
#define GST_PLAYER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_PLAYER, GstPlayerClass))
#define GST_PLAYER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_PLAYER, GstPlayer))
#define GST_PLAYER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_PLAYER, GstPlayerClass))
#define GST_PLAYER_CAST(obj)        ((GstPlayer*)(obj))

GType        gst_player_get_type                      (void);

GstPlayer *  gst_player_new                           (void);

void         gst_player_play                          (GstPlayer    * player);
void         gst_player_pause                         (GstPlayer    * player);
void         gst_player_stop                          (GstPlayer    * player);

void         gst_player_seek                          (GstPlayer    * player,
                                                       GstClockTime   position);

gboolean     gst_player_get_dispatch_to_main_context  (GstPlayer    * player);
void         gst_player_set_dispatch_to_main_context  (GstPlayer    * player,
                                                       gboolean       val);

gchar *      gst_player_get_uri                       (GstPlayer    * player);
void         gst_player_set_uri                       (GstPlayer    * player,
                                                       const gchar  * uri);

GstClockTime gst_player_get_position                  (GstPlayer    * player);
GstClockTime gst_player_get_duration                  (GstPlayer    * player);

gdouble      gst_player_get_volume                    (GstPlayer    * player);
void         gst_player_set_volume                    (GstPlayer    * player,
                                                       gdouble        val);

gboolean     gst_player_get_mute                      (GstPlayer    * player);
void         gst_player_set_mute                      (GstPlayer    * player,
                                                       gboolean       val);

gpointer     gst_player_get_window_handle             (GstPlayer    * player);
void         gst_player_set_window_handle             (GstPlayer    * player,
                                                       gpointer       val);

GstElement * gst_player_get_pipeline                  (GstPlayer    * player);

void          gst_player_set_video_track_enabled      (GstPlayer    * player,
                                                       gboolean enabled);

void          gst_player_set_audio_track_enabled      (GstPlayer    * player,
                                                       gboolean enabled);

void          gst_player_set_subtitle_track_enabled   (GstPlayer    * player,
                                                       gboolean enabled);

gboolean      gst_player_set_audio_track              (GstPlayer    *player,
                                                       gint stream_index);

gboolean      gst_player_set_video_track              (GstPlayer    *player,
                                                       gint stream_index);

gboolean      gst_player_set_subtitle_track           (GstPlayer    *player,
                                                       gint stream_index);

GstPlayerMediaInfo * gst_player_get_media_info        (GstPlayer    * player);

GstPlayerAudioInfo * gst_player_get_audio_track       (GstPlayer    * player);

GstPlayerVideoInfo * gst_player_get_video_track       (GstPlayer    * player);

GstPlayerSubtitleInfo * gst_player_get_subtitle_track (GstPlayer    * player);

G_END_DECLS

#endif /* __GST_PLAYER_H__ */
