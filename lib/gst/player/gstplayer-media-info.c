#include "gstplayer-media-info.h"
#include "gstplayer-media-info-private.h"

/* Per-stream information */
G_DEFINE_TYPE (GstPlayerStreamInfo, gst_player_stream_info, G_TYPE_OBJECT);

static void
gst_player_stream_info_init (GstPlayerStreamInfo * sinfo)
{
  sinfo->stream_index = -1;
}

static void
gst_player_stream_info_finalize (GObject * object)
{
  GstPlayerStreamInfo *sinfo = GST_PLAYER_STREAM_INFO (object);

  if (sinfo->caps)
    gst_caps_unref (sinfo->caps);

  if (sinfo->tags)
    gst_tag_list_unref (sinfo->tags);

  G_OBJECT_CLASS (gst_player_stream_info_parent_class)->finalize (object);
}

static void
gst_player_stream_info_class_init (GObjectClass * klass)
{
  klass->finalize = gst_player_stream_info_finalize;
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
  g_return_val_if_fail (GST_IS_PLAYER_STREAM_INFO (info), -1);

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
 * Returns: (transfer none): the tags contained in this stream.
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
 * Returns: (transfer none): the #GstCaps of the stream.
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
  info->width = -1;
  info->height = -1;
  info->framerate_num = 0;
  info->framerate_denom = 1;
  info->par_num = 1;
  info->par_denom = 1;
}

static void
gst_player_video_info_class_init (GObjectClass * klass)
{
  /* nothing to do here */
}

/**
 * gst_player_video_info_get_width:
 * @info: a #GstPlayerVideoInfo
 *
 * Returns: the width of video in #GstPlayerVideoInfo.
 */
gint
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
gint
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
  gint *fps_n, gint *fps_d)
{
  g_return_if_fail (GST_IS_PLAYER_VIDEO_INFO (info));

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
  g_return_if_fail (GST_IS_PLAYER_VIDEO_INFO (info));

  *par_n = info->par_num;
  *par_d = info->par_denom;
}

/**
 * gst_player_video_info_get_bitrate:
 * @info: a #GstPlayerVideoInfo
 *
 * Returns: the current bitrate of video in #GstPlayerVideoInfo.
 */
gint
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
gint
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
  info->channels = 0;
  info->sample_rate = 0;
  info->bitrate = -1;
  info->max_bitrate = -1;
}

static void
gst_player_audio_info_finalize (GObject * object)
{
  GstPlayerAudioInfo *info = GST_PLAYER_AUDIO_INFO (object);

  g_free (info->language);

  G_OBJECT_CLASS (gst_player_audio_info_parent_class)->finalize (object);
}

static void
gst_player_audio_info_class_init (GObjectClass * klass)
{
  klass->finalize = gst_player_audio_info_finalize;
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
  g_return_val_if_fail (GST_IS_PLAYER_AUDIO_INFO (info), NULL);

 return info->language;
}

/**
 * gst_player_audio_info_get_channels:
 * @info: a #GstPlayerAudioInfo
 *
 * Returns: the number of audio channels in #GstPlayerAudioInfo.
 */
gint
gst_player_audio_info_get_channels (const GstPlayerAudioInfo *info)
{
  g_return_val_if_fail (GST_IS_PLAYER_AUDIO_INFO (info), 0);

  return info->channels;
}

/**
 * gst_player_audio_info_get_sample_rate:
 * @info: a #GstPlayerAudioInfo
 *
 * Returns: the audio sample rate in #GstPlayerAudioInfo.
 */
gint
gst_player_audio_info_get_sample_rate (const GstPlayerAudioInfo *info)
{
  g_return_val_if_fail (GST_IS_PLAYER_AUDIO_INFO (info), 0);

  return info->sample_rate;
}

/**
 * gst_player_audio_info_get_bitrate:
 * @info: a #GstPlayerAudioInfo
 *
 * Returns: the audio bitrate in #GstPlayerAudioInfo.
 */
gint
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
gint
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
  /* nothing to do */
}

static void
gst_player_subtitle_info_finalize (GObject * object)
{
  GstPlayerSubtitleInfo *info = GST_PLAYER_SUBTITLE_INFO (object);

  if (info->language)
    g_free (info->language);

  G_OBJECT_CLASS (gst_player_subtitle_info_parent_class)->finalize (object);
}

static void
gst_player_subtitle_info_class_init (GObjectClass * klass)
{
  klass->finalize = gst_player_subtitle_info_finalize;
}

/**
 * gst_player_subtitle_get_language:
 * @info: a #GstPlayerSubtitleInfo
 *
 * Returns: the language of the stream, or NULL if unknown.
 */
const gchar*
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
  info->duration = -1;
}

static void
gst_player_media_info_finalize (GObject * object)
{
  GstPlayerMediaInfo  *info = GST_PLAYER_MEDIA_INFO (object);

  g_free (info->uri);

  if (info->audio_cachelist)
    g_list_free (info->audio_cachelist);

  if (info->video_cachelist)
    g_list_free (info->video_cachelist);

  if (info->subtitle_cachelist)
    g_list_free (info->subtitle_cachelist);

  if (info->stream_list)
    g_list_free_full (info->stream_list, g_object_unref);

  G_OBJECT_CLASS (gst_player_media_info_parent_class)->finalize (object);
}

static void
gst_player_media_info_class_init (GstPlayerMediaInfoClass * klass)
{
  GObjectClass  *oclass = (GObjectClass *) klass;

  oclass->finalize = gst_player_media_info_finalize;
}

static GstPlayerVideoInfo*
gst_player_video_info_new (void)
{
  return g_object_new (GST_TYPE_PLAYER_VIDEO_INFO, NULL);
}

static GstPlayerAudioInfo*
gst_player_audio_info_new (void)
{
  return g_object_new (GST_TYPE_PLAYER_AUDIO_INFO, NULL);
}

static GstPlayerSubtitleInfo*
gst_player_subtitle_info_new (void)
{
  return g_object_new (GST_TYPE_PLAYER_SUBTITLE_INFO, NULL);
}

static GstPlayerStreamInfo*
gst_player_video_info_copy (GstPlayerVideoInfo *ref)
{
  GstPlayerVideoInfo *ret;

  ret = gst_player_video_info_new ();

  ret->width = ref->width;
  ret->height  = ref->height;
  ret->framerate_num = ref->framerate_num;
  ret->framerate_denom = ref->framerate_denom;
  ret->par_num = ref->par_num;
  ret->par_denom = ref->par_denom;
  ret->bitrate = ref->bitrate;
  ret->max_bitrate = ref->max_bitrate;

  return (GstPlayerStreamInfo*) ret;
}

static GstPlayerStreamInfo*
gst_player_audio_info_copy (GstPlayerAudioInfo *ref)
{
  GstPlayerAudioInfo *ret;

  ret = gst_player_audio_info_new ();

  ret->sample_rate = ref->sample_rate;
  ret->channels = ref->channels;
  ret->bitrate = ref->bitrate;
  ret->max_bitrate = ref->max_bitrate;

  if (ref->language)
    ret->language = g_strdup (ref->language);

  return (GstPlayerStreamInfo*) ret;
}

static GstPlayerStreamInfo*
gst_player_subtitle_info_copy (GstPlayerSubtitleInfo *ref)
{
  GstPlayerSubtitleInfo *ret;

  ret = gst_player_subtitle_info_new ();
  if (ref->language)
    ret->language = g_strdup (ref->language);

  return (GstPlayerStreamInfo*) ret;
}

GstPlayerStreamInfo*
gst_player_stream_info_copy (GstPlayerStreamInfo *ref)
{
  GstPlayerStreamInfo *info = NULL;

  if (!ref)
    return NULL;

  if (GST_IS_PLAYER_VIDEO_INFO(ref))
    info = gst_player_video_info_copy ((GstPlayerVideoInfo*)ref);
  else if (GST_IS_PLAYER_AUDIO_INFO(ref))
    info = gst_player_audio_info_copy ((GstPlayerAudioInfo*)ref);
  else
    info = gst_player_subtitle_info_copy ((GstPlayerSubtitleInfo*)ref);

  info->stream_index = ref->stream_index;
  if (ref->tags)
    info->tags = gst_tag_list_copy (ref->tags);
  if (ref->caps)
    info->caps = gst_caps_copy (ref->caps);

  return info;
}

GstPlayerMediaInfo*
gst_player_media_info_copy (GstPlayerMediaInfo* ref)
{
  GList *l;
  GstPlayerMediaInfo *info;

  if (!ref)
    return NULL;

  info = gst_player_media_info_new (ref->uri);
  info->duration = ref->duration;

  for (l = ref->stream_list; l != NULL; l = l->next) {
    GstPlayerStreamInfo *s;

    s = gst_player_stream_info_copy((GstPlayerStreamInfo*)l->data);
    info->stream_list = g_list_append (info->stream_list, s);

    if (GST_IS_PLAYER_AUDIO_INFO(s))
      info->audio_cachelist = g_list_append (info->audio_cachelist, s);
    else if (GST_IS_PLAYER_VIDEO_INFO(s))
      info->video_cachelist = g_list_append (info->video_cachelist, s);
    else
      info->subtitle_cachelist = g_list_append (info->subtitle_cachelist, s);
  }

  return info;
}

GstPlayerStreamInfo*
gst_player_stream_info_new (gint stream_index, GType type)
{
  GstPlayerStreamInfo *info = NULL;

  if (type == GST_TYPE_PLAYER_AUDIO_INFO)
    info = (GstPlayerStreamInfo*) gst_player_audio_info_new ();
  else if (type == GST_TYPE_PLAYER_VIDEO_INFO)
    info = (GstPlayerStreamInfo*) gst_player_video_info_new ();
  else
    info = (GstPlayerStreamInfo*) gst_player_subtitle_info_new ();

  info->stream_index = stream_index;

  return info;
}

GstPlayerMediaInfo*
gst_player_media_info_new (const gchar *uri)
{
  GstPlayerMediaInfo *info;

  g_return_val_if_fail (uri != NULL, NULL);

  info = g_object_new (GST_TYPE_PLAYER_MEDIA_INFO, NULL);
  if (uri)
    info->uri = g_strdup (uri);

  return info;
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
 * Returns: (transfer none) (element-type GstPlayerStreamInfo): A #GList of
 * matching #GstPlayerStreamInfo.
 */
GList *
gst_player_media_info_get_stream_list (const GstPlayerMediaInfo *info)
{
  g_return_val_if_fail (GST_IS_PLAYER_MEDIA_INFO (info), NULL);

  return info->stream_list;
}

/**
 * gst_player_get_video_streams:
 * @info: a #GstPlayerMediaInfo
 *
 * Returns: (transfer none) (element-type GstPlayerVideoInfo): A #GList of
 * matching #GstPlayerVideoInfo.
 */
GList*
gst_player_get_video_streams (const GstPlayerMediaInfo *info)
{
  g_return_val_if_fail (GST_IS_PLAYER_MEDIA_INFO (info), NULL);

  return info->video_cachelist;
}

/**
 * gst_player_get_subtitle_streams:
 * @info: a #GstPlayerMediaInfo
 *
 * Returns: (transfer none) (element-type GstPlayerSubtitleInfo): A #GList of
 * matching #GstPlayerSubtitleInfo.
 */
GList*
gst_player_get_subtitle_streams (const GstPlayerMediaInfo *info)
{
  g_return_val_if_fail (GST_IS_PLAYER_MEDIA_INFO (info), NULL);

  return info->subtitle_cachelist;
}

/**
 * gst_player_get_audio_streams:
 * @info: a #GstPlayerMediaInfo
 *
 * Returns: (transfer none) (element-type GstPlayerAudioInfo): A #GList of
 * matching #GstPlayerAudioInfo.
 */
GList*
gst_player_get_audio_streams (const GstPlayerMediaInfo *info)
{
  g_return_val_if_fail (GST_IS_PLAYER_MEDIA_INFO (info), NULL);

  return info->audio_cachelist;
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

