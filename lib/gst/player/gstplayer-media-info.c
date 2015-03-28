#include "gstplayer-media-info.h"
#include "gstplayer-media-info-private.h"

/* Per-stream information */
G_DEFINE_TYPE (GstPlayerStreamInfo, gst_player_stream_info, G_TYPE_OBJECT);

static void
gst_player_stream_info_init (GstPlayerStreamInfo * sinfo)
{
  /* Nothing to do here */
}

static void
gst_player_stream_info_finalize (GObject * object)
{
  GstPlayerStreamInfo *sinfo = GST_PLAYER_STREAM_INFO (object);

  if (sinfo->caps) {
    gst_caps_unref (sinfo->caps);
    sinfo->caps = NULL;
  }

  if (sinfo->tags) {
    gst_tag_list_unref (sinfo->tags);
    sinfo->tags = NULL;
  }
}

static void
gst_player_stream_info_class_init (GObjectClass * klass)
{
  klass->finalize = gst_player_stream_info_finalize;
}

/**
 * gst_player_stream_info_get_stream_id:
 * @info: a #GstPlayerStreamInfo
 *
 * Returns: the stream ID of this stream.
 */
gint
gst_player_stream_info_get_stream_id (const GstPlayerStreamInfo *info)
{
  return info->stream_id;
}

/**
 * gst_player_stream_info_get_stream_type_nick:
 * @info: a #GstPlayerStreamInfo
 *
 * Returns: a human readable name for the stream type of the given @info (ex : "audio",
 * "video",...).
 */
const gchar*
gst_player_stream_info_get_stream_type_nick (const GstPlayerStreamInfo *info)
{
  if (GST_IS_PLAYER_VIDEO_INFO (info))
    return "video";

  if (GST_IS_PLAYER_AUDIO_INFO (info))
    return "audio";

  if (GST_IS_PLAYER_SUBTITLE_INFO (info))
    return "subtitle";

  return NULL;
}

/**
 * gst_player_stream_info_get_tags:
 * @info: a #GstPlayerStreamInfo
 *
 * Returns: the tags contained in this stream. Unref with #gst_tag_list_unref
 */
GstTagList*
gst_player_stream_info_get_stream_tags (const GstPlayerStreamInfo *sinfo)
{
  GstTagList *tags = NULL;

  if (sinfo->tags)
    tags = gst_tag_list_ref (sinfo->tags);

  return tags;
}

/**
 * gst_player_stream_info_get_caps:
 * @info: a #GstPlayerStreamInfo
 *
 * Returns: the #GstCaps of the stream. Unref with #gst_caps_unref after usage.
 */
GstCaps*
gst_player_stream_info_get_stream_caps (const GstPlayerStreamInfo *sinfo)
{
  GstCaps *caps = NULL;

  if (sinfo->caps)
    caps = gst_caps_ref (sinfo->caps);

  return caps;
}

/**
 * gst_player_video_info_get_streams:
 * @info: a #GstPlayerMediaInfo
 *
 * Returns: (transfer full) (element-type GstPlayerVideoInfo): A #GList of
 * matching #GstPlayerVideoInfo. The caller should free it with
 * gst_player_stream_info_list_free().
 */
GList*
gst_player_media_info_get_video_streams (const GstPlayerMediaInfo *info)
{
  GList *result = NULL, *p;

  g_return_val_if_fail (GST_IS_PLAYER_MEDIA_INFO (info), NULL);

  for (p = info->stream_list; p != NULL; p = p->next) {
    GstPlayerStreamInfo  *sinfo = (GstPlayerStreamInfo*) p->data;

    if (GST_IS_PLAYER_VIDEO_INFO (sinfo)) {
      GstPlayerVideoInfo  *vinfo = (GstPlayerVideoInfo*) sinfo;

      result = g_list_append (result, vinfo);
    }
  }

  return result;
}

/**
 * gst_player_subtitle_info_get_streams:
 * @info: a #GstPlayerMediaInfo
 *
 * Returns: (transfer full) (element-type GstPlayerSubtitleInfo): A #GList of
 * matching #GstPlayerSubtitleInfo. The caller should free it with
 * gst_player_stream_info_list_free().
 */
GList*
gst_player_media_info_get_subtitle_streams (const GstPlayerMediaInfo *info)
{
  GList *result = NULL, *p;

  g_return_val_if_fail (GST_IS_PLAYER_MEDIA_INFO (info), NULL);

  for (p = info->stream_list; p != NULL; p = p->next) {
    GstPlayerStreamInfo  *sinfo = (GstPlayerStreamInfo*) p->data;

    if (GST_IS_PLAYER_SUBTITLE_INFO (sinfo)) {
      GstPlayerSubtitleInfo  *tinfo = (GstPlayerSubtitleInfo*) sinfo;

      result = g_list_append (result, tinfo);
    }
  }

  return result;
}

/**
 * gst_player_audio_info_get_streams:
 * @info: a #GstPlayerMediaInfo
 *
 * Returns: (transfer full) (element-type GstPlayerAudioInfo): A #GList of
 * matching #GstPlayerAudioInfo. The caller should free it with
 * gst_player_stream_info_list_free().
 */
GList*
gst_player_media_info_get_audio_streams (const GstPlayerMediaInfo *info)
{
  GList *result = NULL, *p;

  g_return_val_if_fail (GST_IS_PLAYER_MEDIA_INFO (info), NULL);

  for (p = info->stream_list; p != NULL; p = p->next) {
    GstPlayerStreamInfo  *sinfo = (GstPlayerStreamInfo*) p->data;

    if (GST_IS_PLAYER_AUDIO_INFO (sinfo)) {
      GstPlayerAudioInfo  *ainfo = (GstPlayerAudioInfo*) sinfo;

      result = g_list_append (result, ainfo);
    }
  }

  return result;
}

/**
 * gst_player_get_current_video:
 * @info: a #GstPlayerMediaInfo
 *
 * Returns: the current audio stream #GstPlayerVideoInfo or NULL if unknown.
 */
GstPlayerVideoInfo*
gst_player_media_info_get_current_video (const GstPlayerMediaInfo *info)
{
  GstPlayerVideoInfo  *vinfo = NULL;

  g_return_val_if_fail (GST_IS_PLAYER_MEDIA_INFO (info), NULL);

  if (info->current_video)
    vinfo = (GstPlayerVideoInfo*) info->current_video;

  return vinfo;
}

/**
 * gst_player_get_current_audio:
 * @info: a #GstPlayerMediaInfo
 *
 * Returns: the current audio stream #GstPlayerAudioInfo or NULL if unknown.
 */
GstPlayerAudioInfo*
gst_player_media_info_get_current_audio (const GstPlayerMediaInfo *info)
{
  GstPlayerAudioInfo  *ainfo = NULL;

  g_return_val_if_fail (GST_IS_PLAYER_MEDIA_INFO (info), NULL);

  if (info->current_audio)
    ainfo = (GstPlayerAudioInfo*) info->current_audio;

  return ainfo;
}

/**
 * gst_player_get_current_subtitle:
 * @info: a #GstPlayerMediaInfo
 *
 * Returns: the current subtitle stream #GstPlayerSubtitleInfo or NULL if unknown.
 */
GstPlayerSubtitleInfo*
gst_player_media_info_get_current_subtitle (const GstPlayerMediaInfo *info)
{
  GstPlayerSubtitleInfo  *tinfo = NULL;

  g_return_val_if_fail (GST_IS_PLAYER_MEDIA_INFO (info), NULL);

  if (info->current_subtitle)
    tinfo = (GstPlayerSubtitleInfo*) info->current_subtitle;

  return tinfo;
}

/**
 * gst_player_audio_info_get_language:
 * @info: a #GstPlayerAudioInfo
 *
 * Returns: the language of the stream, or NULL if unknown.
 */
const gchar*
gst_player_audio_info_get_language(const GstPlayerAudioInfo* info)
{
  gchar *language = NULL;

  g_return_val_if_fail (GST_IS_PLAYER_MEDIA_INFO (info), NULL);

  if (info->language)
    language = g_strdup (info->language);

 return language;
}

/**
 * gst_player_media_stream_info_list_free:
 * @infos: a #GList of #GstPlayerStreamInfo
 *
 * frees the #GList.
 */
void
gst_player_media_info_stream_info_list_free (GList *infos)
{
  g_list_free (infos);
}

/* Video information */
G_DEFINE_TYPE (GstPlayerVideoInfo, gst_player_video_info,
    GST_TYPE_PLAYER_MEDIA_INFO);

static void
gst_player_video_info_init (GstPlayerVideoInfo * info)
{
  /* Nothing to do here */
}

/**
 * gst_player_video_info_get_width:
 * @info: a #GstPlayerVideoInfo
 *
 * Returns: the width of video in #GstPlayerVideoInfo.
 */
guint
gst_player_video_info_get_width (const GstPlayerVideoInfo *info)
{
  g_return_val_if_fail (GST_IS_PLAYER_VIDEO_INFO (info), -1);

  return info->width;
}

/**
 * gst_player_video_info_get_height:
 * @info: a #GstPlayerVideoInfo
 *
 * Returns: the height of video in #GstPlayerVideoInfo.
 */
guint
gst_player_video_info_get_height (const GstPlayerVideoInfo *info)
{
  g_return_val_if_fail (GST_IS_PLAYER_VIDEO_INFO (info), -1);

  return info->height;
}

/**
 * gst_player_video_info_get_framerate_num:
 * @info: a #GstPlayerVideoInfo
 *
 * Returns: the framerate numerator of video in #GstPlayerVideoInfo.
 */
guint
gst_player_video_info_get_framerate_num (const GstPlayerVideoInfo *info)
{
  g_return_val_if_fail (GST_IS_PLAYER_VIDEO_INFO (info), -1);

  return info->framerate_num;
}

/**
 * gst_player_video_info_get_framerate_denom:
 * @info: a #GstPlayerVideoInfo
 *
 * Returns: the framerate denominator of video in #GstPlayerVideoInfo.
 */
guint
gst_player_video_info_get_framerate_denom (const GstPlayerVideoInfo *info)
{
  g_return_val_if_fail (GST_IS_PLAYER_VIDEO_INFO (info), -1);

  return info->framerate_denom;
}

/**
 * gst_player_video_info_get_bitrate:
 * @info: a #GstPlayerVideoInfo
 *
 * Returns: the current bitrate of video in #GstPlayerVideoInfo.
 */
guint
gst_player_video_info_get_bitrate (const GstPlayerVideoInfo* info)
{
  g_return_val_if_fail (GST_IS_PLAYER_VIDEO_INFO (info), -1);

  return info->bitrate;
}

/**
 * gst_player_video_info_get_max_bitrate:
 * @info: a #GstPlayerVideoInfo
 *
 * Returns: the maximum bitrate of video in #GstPlayerVideoInfo.
 */
guint
gst_player_video_info_get_max_bitrate (const GstPlayerVideoInfo* info)
{
  g_return_val_if_fail (GST_IS_PLAYER_VIDEO_INFO (info), -1);

  return info->max_bitrate;
}

static void
gst_player_video_info_finalize (GObject * object)
{
  /* nothing to do */
}

static void
gst_player_video_info_class_init (GObjectClass * klass)
{
  klass->finalize = gst_player_video_info_finalize;
}

/* Audio information */
G_DEFINE_TYPE (GstPlayerAudioInfo, gst_player_audio_info,
    GST_TYPE_PLAYER_MEDIA_INFO);

static void
gst_player_audio_info_init (GstPlayerAudioInfo * info)
{
  /* Nothing to do here */
}

static void
gst_player_audio_info_finalize (GObject * object)
{
  GstPlayerAudioInfo *ainfo = GST_PLAYER_AUDIO_INFO (object);

  if (ainfo->language) {
    g_free (ainfo->language);
    ainfo->language = NULL;
  }
}

static void
gst_player_audio_info_class_init (GObjectClass * klass)
{
  klass->finalize = gst_player_audio_info_finalize;
}

/**
 * gst_player_audio_info_get_channels:
 * @info: a #GstPlayerAudioInfo
 *
 * Returns: the number of audio channels in #GstPlayerAudioInfo.
 */
guint
gst_player_audio_info_get_channels (const GstPlayerAudioInfo *info)
{
  g_return_val_if_fail (GST_IS_PLAYER_AUDIO_INFO (info), -1);

  return info->channels;
}

/**
 * gst_player_audio_info_get_sample_rate:
 * @info: a #GstPlayerAudioInfo
 *
 * Returns: the audio sample rate in #GstPlayerAudioInfo.
 */
guint
gst_player_audio_info_get_sample_rate (const GstPlayerAudioInfo *info)
{
  g_return_val_if_fail (GST_IS_PLAYER_AUDIO_INFO (info), -1);

  return info->sample_rate;
}

/**
 * gst_player_audio_info_get_bitrate:
 * @info: a #GstPlayerAudioInfo
 *
 * Returns: the audio bitrate in #GstPlayerAudioInfo.
 */
guint
gst_player_audio_info_get_bitrate (const GstPlayerAudioInfo* info)
{
  g_return_val_if_fail (GST_IS_PLAYER_AUDIO_INFO (info), -1);

  return info->bitrate;
}

/**
 * gst_player_audio_info_get_max_bitrate:
 * @info: a #GstPlayerAudioInfo
 *
 * Returns: the audio maximum bitrate in #GstPlayerAudioInfo.
 */
guint
gst_player_audio_info_get_max_bitrate (const GstPlayerAudioInfo* info)
{
  g_return_val_if_fail (GST_IS_PLAYER_AUDIO_INFO (info), -1);

  return info->max_bitrate;
}

/* Subtitle information */
G_DEFINE_TYPE (GstPlayerSubtitleInfo, gst_player_subtitle_info,
    GST_TYPE_PLAYER_MEDIA_INFO);

static void
gst_player_subtitle_info_init (GstPlayerSubtitleInfo * info)
{
  /* Nothing to do here */
}

static void
gst_player_subtitle_info_finalize (GObject * object)
{
  GstPlayerSubtitleInfo *tinfo = GST_PLAYER_SUBTITLE_INFO (object);

  if (tinfo->language) {
    g_free (tinfo->language);
    tinfo->language = NULL;
  }
}

static void
gst_player_subtitle_info_class_init (GObjectClass * klass)
{
  klass->finalize = gst_player_subtitle_info_finalize;
}

/**
 * gst_player_subtitle_info_get_language:
 * @info: a #GstPlayerSubtitleInfo
 *
 * Returns: the language of the stream, or NULL if unknown.
 */
const gchar*
gst_player_subtitle_info_get_language( const GstPlayerSubtitleInfo* info)
{
  gchar *language = NULL;

  g_return_val_if_fail (GST_IS_PLAYER_SUBTITLE_INFO (info), NULL);

  if (info->language)
    language = g_strdup (info->language);

  return language;
}

/* Global media information */
G_DEFINE_TYPE (GstPlayerMediaInfo, gst_player_media_info, G_TYPE_OBJECT);

static void
gst_player_media_info_init (GstPlayerMediaInfo * info)
{
  /* nothing to do here */
}

static void
gst_player_media_info_finalize (GObject * object)
{
  GstPlayerMediaInfo  *info = GST_PLAYER_MEDIA_INFO (object);

  if (info->uri) {
    g_free (info->uri);
    info->uri = NULL;
  }

  if (info->current_video) {
    info->current_video = NULL;
  }

  if (info->current_audio) {
    info->current_audio = NULL;
  }

  if (info->current_subtitle) {
    info->current_subtitle = NULL;
  }

  if (info->stream_list) {
    GList *list;

    for (list = info->stream_list; list != NULL; list = list->next)
      g_object_unref (list->data);

    g_list_free (info->stream_list);
    info->stream_list = NULL;
  }
}

static void
gst_player_media_info_class_init (GstPlayerMediaInfoClass * klass)
{
  GObjectClass  *oclass = (GObjectClass *) klass;

  oclass->finalize = gst_player_media_info_finalize;
}

/**
 * gst_player_media_info_new:
 * @uri: a uri
 *
 * Returns: create a new #GstPlayerMediaInfo.
 */
GstPlayerMediaInfo *
gst_player_media_info_new (const gchar *uri)
{
  GstPlayerMediaInfo *info;

  info = g_object_new (GST_TYPE_PLAYER_MEDIA_INFO, NULL);
  if (info == NULL)
    goto error;

  info->uri = g_strdup (uri);

  return info;

error:
  return NULL;
}

/**
 * gst_player_media_info_get_uri:
 * @info: a #GstPlayerMediaInfo
 *
 * Returns: the URI associated with #GstPlayerMediaInfo.
 */
const gchar *
gst_player_media_info_get_uri (const GstPlayerMediaInfo *info)
{
  g_return_val_if_fail (GST_IS_PLAYER_MEDIA_INFO (info), NULL);

  return info->uri;
}

/**
 * gst_player_media_info_get_stream_list:
 * @info: a #GstPlayerMediaInfo
 *
 * Returns: (transfer full) (element-type GstPlayerStreamInfo): A #GList of
 * matching #GstPlayerStreamInfo. The caller should free it with
 * gst_player_stream_info_list_free().
 */
GList *
gst_player_media_info_get_stream_list (const GstPlayerMediaInfo *info)
{
  GList *list = NULL, *l;;

  g_return_val_if_fail (GST_IS_PLAYER_MEDIA_INFO (info), NULL);

  for (l = info->stream_list; l != NULL; l = l->next)
    list = g_list_append (list, l->data);

  return list;
}

