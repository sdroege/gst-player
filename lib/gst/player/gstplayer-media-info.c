#include "gstplayer-media-info.h"
#include "gstplayer-media-info-private.h"

/* Per-stream information */
G_DEFINE_TYPE (GstPlayerStreamInfo, gst_player_stream_info, G_TYPE_OBJECT);

static void
gst_player_stream_info_init (GstPlayerStreamInfo * sinfo)
{
  sinfo->caps = NULL;
  sinfo->tags = NULL;
  sinfo->stream_index = -1;
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

  G_OBJECT_CLASS (gst_player_stream_info_parent_class)->finalize (object);
}

static void
gst_player_stream_info_class_init (GObjectClass * klass)
{
  klass->finalize = gst_player_stream_info_finalize;
}

GstPlayerStreamInfo*
gst_player_stream_info_new (GstPlayerStreamInfo *ref)
{
  GstPlayerStreamInfo *ret = NULL;

  if (GST_IS_PLAYER_VIDEO_INFO (ref))
    ret = (GstPlayerStreamInfo*) gst_player_video_info_new
                                    ((GstPlayerVideoInfo*)ref);
  if (GST_IS_PLAYER_AUDIO_INFO (ref))
    ret = (GstPlayerStreamInfo*) gst_player_audio_info_new
                                    ((GstPlayerAudioInfo*)ref);
  if (GST_IS_PLAYER_SUBTITLE_INFO (ref))
    ret = (GstPlayerStreamInfo*) gst_player_subtitle_info_new
                                    ((GstPlayerSubtitleInfo*)ref);

  ret->stream_index = gst_player_stream_info_get_stream_index (ref);
  ret->tags = gst_player_stream_info_get_stream_tags (ref);
  ret->caps = gst_player_stream_info_get_stream_caps (ref);

  return ret;
}

/**
 * gst_player_stream_info_get_stream_index:
 * @info: a #GstPlayerStreamInfo
 *
 * Returns: the stream index of this stream.
 */
gint
gst_player_stream_info_get_stream_index (const GstPlayerStreamInfo *info)
{
  g_return_val_if_fail (GST_IS_PLAYER_STREAM_INFO (info), NULL);

  return info->stream_index;
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
  g_return_val_if_fail (GST_IS_PLAYER_STREAM_INFO (info), NULL);

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
 * Returns: (transfer full): the tags contained in this stream.
 */
GstTagList*
gst_player_stream_info_get_stream_tags (const GstPlayerStreamInfo *info)
{
  g_return_val_if_fail (GST_IS_PLAYER_STREAM_INFO (info), NULL);

  return info->tags;
}

/**
 * gst_player_stream_info_get_caps:
 * @info: a #GstPlayerStreamInfo
 *
 * Returns: (transfer full): the #GstCaps of the stream.
 */
GstCaps*
gst_player_stream_info_get_stream_caps (const GstPlayerStreamInfo *info)
{
  g_return_val_if_fail (GST_IS_PLAYER_STREAM_INFO (info), NULL);

  return info->caps;
}

/* Video information */
G_DEFINE_TYPE (GstPlayerVideoInfo, gst_player_video_info,
    GST_TYPE_PLAYER_STREAM_INFO);

static void
gst_player_video_info_init (GstPlayerVideoInfo * info)
{
  /* Nothing to do here */
}

static void
gst_player_video_info_finalize (GObject * gobject)
{
  G_OBJECT_CLASS (gst_player_video_info_parent_class)->finalize (gobject);
}

static void
gst_player_video_info_class_init (GObjectClass * klass)
{
  klass->finalize = gst_player_video_info_finalize;
}

GstPlayerVideoInfo*
gst_player_video_info_new (GstPlayerVideoInfo *ref)
{
  GstPlayerVideoInfo *ret = NULL;

  ret = g_object_new (GST_TYPE_PLAYER_VIDEO_INFO, NULL);

  if (ref) {
    ret->width = ref->width;
    ret->height  = ref->height;
    ret->framerate_num = ref->framerate_num;
    ret->framerate_denom = ref->framerate_denom;
    ret->par_num = ref->par_num;
    ret->par_denom = ref->par_denom;
    ret->bitrate = ref->bitrate;
    ret->max_bitrate = ref->max_bitrate;
  }

  return ret;
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
 */
void
gst_player_video_info_get_framerate (const GstPlayerVideoInfo *info,
  guint *fps_n, guint *fps_d)
{
  g_return_val_if_fail (GST_IS_PLAYER_VIDEO_INFO (info), NULL);

  *fps_n = info->framerate_num;
  *fps_d = info->framerate_denom;
}

/**
 * gst_player_video_info_get_pixel_aspect_ratio:
 * @info: a #GstPlayerVideoInfo
 *
 */
void
gst_player_video_info_get_pixel_aspect_ratio (const GstPlayerVideoInfo *info,
  guint *par_d, guint *par_n)
{
  g_return_val_if_fail (GST_IS_PLAYER_VIDEO_INFO (info), NULL);

  *par_n = info->par_num;
  *par_d = info->par_denom;
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

/* Audio information */
G_DEFINE_TYPE (GstPlayerAudioInfo, gst_player_audio_info,
    GST_TYPE_PLAYER_STREAM_INFO);

static void
gst_player_audio_info_init (GstPlayerAudioInfo * info)
{
  info->language = NULL;
}

static void
gst_player_audio_info_finalize (GObject * object)
{
  GstPlayerAudioInfo *info = GST_PLAYER_AUDIO_INFO (object);

  g_free (info->language);
  info->language = NULL;

  G_OBJECT_CLASS (gst_player_audio_info_parent_class)->finalize (object);
}

static void
gst_player_audio_info_class_init (GObjectClass * klass)
{
  klass->finalize = gst_player_audio_info_finalize;
}

GstPlayerAudioInfo*
gst_player_audio_info_new (GstPlayerAudioInfo *ref)
{
  GstPlayerAudioInfo *ret = NULL;

  ret = g_object_new (GST_TYPE_PLAYER_AUDIO_INFO, NULL);

  if (ref) {
    ret->sample_rate = ref->sample_rate;
    ret->language = g_strdup (ref->language);
    ret->channels = ref->channels;
    ret->bitrate = ref->bitrate;
    ret->max_bitrate = ref->max_bitrate;
  }

  return ret;
}

/**
 * gst_player_audio_info_get_language:
 * @info: a #GstPlayerAudioInfo
 *
 * Returns: the language of the stream, or NULL if unknown.
 */
gchar*
gst_player_audio_info_get_language(const GstPlayerAudioInfo* info)
{
  g_return_val_if_fail (GST_IS_PLAYER_AUDIO_INFO (info), NULL);

 return info->language;
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
    GST_TYPE_PLAYER_STREAM_INFO);

static void
gst_player_subtitle_info_init (GstPlayerSubtitleInfo * info)
{
  info->language = NULL;
}

static void
gst_player_subtitle_info_finalize (GObject * object)
{
  GstPlayerSubtitleInfo *info = GST_PLAYER_SUBTITLE_INFO (object);

  g_free (info->language);

  G_OBJECT_CLASS (gst_player_subtitle_info_parent_class)->finalize (object);
}

static void
gst_player_subtitle_info_class_init (GObjectClass * klass)
{
  klass->finalize = gst_player_subtitle_info_finalize;
}

GstPlayerSubtitleInfo*
gst_player_subtitle_info_new (GstPlayerSubtitleInfo *ref)
{
  GstPlayerSubtitleInfo *ret = NULL;

  ret = g_object_new (GST_TYPE_PLAYER_SUBTITLE_INFO, NULL);

  if (ref)
    ret->language = g_strdup (ref->language);

  return ret;
}

/**
 * gst_player_subtitle_get_language:
 * @info: a #GstPlayerSubtitleInfo
 *
 * Returns: the language of the stream, or NULL if unknown.
 */
gchar*
gst_player_subtitle_get_language( const GstPlayerSubtitleInfo* info)
{
  g_return_val_if_fail (GST_IS_PLAYER_SUBTITLE_INFO (info), NULL);

  return info->language;
}

/* Global media information */
G_DEFINE_TYPE (GstPlayerMediaInfo, gst_player_media_info, G_TYPE_OBJECT);

static void
gst_player_media_info_init (GstPlayerMediaInfo * info)
{
  info->uri = NULL;
  info->stream_list = NULL;
}

static void
gst_player_media_info_finalize (GObject * object)
{
  GstPlayerMediaInfo  *info = GST_PLAYER_MEDIA_INFO (object);

  g_free (info->uri);
  info->uri = NULL;

  if (info->stream_list) {
    g_list_free (info->stream_list);
    info->stream_list = NULL;
  }

  G_OBJECT_CLASS (gst_player_media_info_parent_class)->finalize (object);
}

static void
gst_player_media_info_class_init (GstPlayerMediaInfoClass * klass)
{
  GObjectClass  *oclass = (GObjectClass *) klass;

  oclass->finalize = gst_player_media_info_finalize;
}

/**
 * gst_player_media_info_get_uri:
 * @info: a #GstPlayerMediaInfo
 *
 * Returns: the URI associated with #GstPlayerMediaInfo.
 */
gchar *
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
    list = g_list_append (list, g_object_ref(l->data));

  return list;
}

/**
 * gst_player_get_video_streams:
 * @info: a #GstPlayerMediaInfo
 *
 * Returns: (transfer full) (element-type GstPlayerVideoInfo): A #GList of
 * matching #GstPlayerVideoInfo. The caller should free it with
 * gst_player_stream_info_list_free().
 */
GList*
gst_player_get_video_streams (const GstPlayerMediaInfo *info)
{
  GList *result = NULL, *p;

  g_return_val_if_fail (GST_IS_PLAYER_MEDIA_INFO (info), NULL);

  for (p = info->stream_list; p != NULL; p = p->next) {
    GstPlayerStreamInfo  *sinfo = (GstPlayerStreamInfo*) p->data;

    if (GST_IS_PLAYER_VIDEO_INFO (sinfo)) {
      GstPlayerVideoInfo  *vinfo = (GstPlayerVideoInfo*) sinfo;

      result = g_list_append (result, g_object_ref(vinfo));
    }
  }

  return result;
}

/**
 * gst_player_get_subtitle_streams:
 * @info: a #GstPlayerMediaInfo
 *
 * Returns: (transfer full) (element-type GstPlayerSubtitleInfo): A #GList of
 * matching #GstPlayerSubtitleInfo. The caller should free it with
 * gst_player_stream_info_list_free().
 */
GList*
gst_player_get_subtitle_streams (const GstPlayerMediaInfo *info)
{
  GList *result = NULL, *p;

  g_return_val_if_fail (GST_IS_PLAYER_MEDIA_INFO (info), NULL);

  for (p = info->stream_list; p != NULL; p = p->next) {
    GstPlayerStreamInfo  *sinfo = (GstPlayerStreamInfo*) p->data;

    if (GST_IS_PLAYER_SUBTITLE_INFO (sinfo)) {
      GstPlayerSubtitleInfo  *tinfo = (GstPlayerSubtitleInfo*) sinfo;

      result = g_list_append (result, g_object_ref(tinfo));
    }
  }

  return result;
}

/**
 * gst_player_get_audio_streams:
 * @info: a #GstPlayerMediaInfo
 *
 * Returns: (transfer full) (element-type GstPlayerAudioInfo): A #GList of
 * matching #GstPlayerAudioInfo. The caller should free it with
 * gst_player_stream_info_list_free().
 */
GList*
gst_player_get_audio_streams (const GstPlayerMediaInfo *info)
{
  GList *result = NULL, *p;

  g_return_val_if_fail (GST_IS_PLAYER_MEDIA_INFO (info), NULL);

  for (p = info->stream_list; p != NULL; p = p->next) {
    GstPlayerStreamInfo  *sinfo = (GstPlayerStreamInfo*) p->data;

    if (GST_IS_PLAYER_AUDIO_INFO (sinfo)) {
      GstPlayerAudioInfo  *ainfo = (GstPlayerAudioInfo*) sinfo;

      result = g_list_append (result, g_object_ref(ainfo));
    }
  }

  return result;
}

/**
 * gst_player_stream_info_list_free:
 * @list: (element-type GstPlayerStreamInfo): A #GList of #GstPlayerStreamInfo
 *
 * frees the #GList.
 */
void
gst_player_stream_info_list_free (GList *list)
{
  g_list_free_full (list, g_object_unref);
}

/**
 * gst_player_media_info_get_duration:
 * @info: a #GstPlayerMediaInfo
 *
 * Returns: duration of the media.
 */
GstClockTime
gst_player_media_info_get_duration (const GstPlayerMediaInfo *info)
{
  g_return_val_if_fail (GST_IS_PLAYER_MEDIA_INFO (info), -1);

  return info->duration;
}


