TESTS = \
	test-player

noinst_PROGRAMS = $(TESTS)

TESTS_CFLAGS = \
	$(CHECK_CFLAGS) \
	$(GSTREAMER_CFLAGS) \
	$(GLIB_CFLAGS) \
	-I$(top_srcdir)/lib \
	-I$(top_builddir)/lib \
	$(WARNING_CFLAGS)
TESTS_LDADD = \
	$(CHECK_LIBS) \
	$(GSTREAMER_LIBS) \
	$(GLIB_LIBS) \
	$(top_builddir)/lib/gst/player/.libs/libgstplayer-@GST_PLAYER_API_VERSION@.la

test_player_SOURCES = player.c
test_player_CFLAGS = $(TESTS_CFLAGS) -DTEST_PATH=\"$(srcdir)/media\"
test_player_LDADD = $(TESTS_LDADD)

EXTRA_DIST = \
	media/audio.ogg \
	media/audio-video.ogg \
	media/audio-short.ogg \
	media/audio-video-short.ogg

include $(top_srcdir)/check.mk

