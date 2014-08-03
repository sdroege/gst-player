lib_LTLIBRARIES = libgstplayer-@GST_PLAYER_API_VERSION@.la

libgstplayer_@GST_PLAYER_API_VERSION@_la_SOURCES = \
	gstplayer.c

libgstplayer_@GST_PLAYER_API_VERSION@_la_CFLAGS = \
	-I$(top_srcdir)/lib \
	-I$(top_builddir)/lib \
	$(GSTREAMER_CFLAGS) \
	$(GLIB_CFLAGS) \
	$(WARNING_CFLAGS)

libgstplayer_@GST_PLAYER_API_VERSION@_la_LDFLAGS = \
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE) \
	-export-symbols-regex '^gst_.*$$' \
	-no-undefined

libgstplayer_@GST_PLAYER_API_VERSION@_la_LIBADD = \
	$(LIBM) \
	$(GSTREAMER_LIBS) \
	$(GLIB_LIBS)

libgstplayerdir = $(includedir)/gst-player-@GST_PLAYER_API_VERSION@/gst/player

libgstplayer_HEADERS = \
	gstplayer.h

