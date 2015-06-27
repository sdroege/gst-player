lib_LTLIBRARIES = libgstplayer-@GST_PLAYER_API_VERSION@.la

libgstplayer_@GST_PLAYER_API_VERSION@_la_SOURCES = \
	gstplayer.c  \
	gstplayer-media-info.c

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

noinst_HEADERS = gstplayer-media-info-private.h

libgstplayer_HEADERS = \
	player.h \
	gstplayer.h \
	gstplayer-media-info.h

CLEANFILES =

if HAVE_INTROSPECTION
BUILT_GIRSOURCES = GstPlayer-@GST_PLAYER_API_VERSION@.gir

gir_headers=$(patsubst %,$(srcdir)/%, $(libgstplayer_HEADERS))
gir_sources=$(patsubst %,$(srcdir)/%, $(libgstplayer_@GST_PLAYER_API_VERSION@_la_SOURCES))

GstPlayer-@GST_PLAYER_API_VERSION@.gir: $(INTROSPECTION_SCANNER) libgstplayer-@GST_PLAYER_API_VERSION@.la
	$(AM_V_GEN)$(INTROSPECTION_SCANNER) -v --namespace GstPlayer \
		--nsversion=@GST_PLAYER_API_VERSION@ \
		--warn-all \
		--strip-prefix=Gst \
		-I$(top_srcdir)/lib \
		-I$(top_builddir)/lib \
		--c-include "gst/player/player.h" \
		--add-include-path=$(top_builddir)/lib \
		--add-include-path=`$(PKG_CONFIG) --variable=girdir gstreamer-1.0` \
		--library-path=$(top_builddir)/lib \
		--library=libgstplayer-@GST_PLAYER_API_VERSION@.la \
		--include=GObject-2.0 \
		--include=Gst-1.0 \
		--libtool="${LIBTOOL}" \
		--pkg gobject-2.0 \
		--pkg gstreamer-1.0 \
		--pkg gstreamer-audio-1.0 \
		--pkg gstreamer-video-1.0 \
		--pkg gstreamer-tag-1.0 \
		--pkg gstreamer-pbutils-1.0 \
		--pkg-export gstreamer-player-@GST_PLAYER_API_VERSION@ \
		--add-init-section="gst_init(NULL,NULL);" \
		--output $@ \
		$(gir_headers) \
		$(gir_sources)

# INTROSPECTION_GIRDIR/INTROSPECTION_TYPELIBDIR aren't the right place to
# install anything - we need to install inside our prefix.
girdir = $(datadir)/gir-1.0
gir_DATA = $(BUILT_GIRSOURCES)

typelibsdir = $(libdir)/girepository-1.0/

typelibs_DATA = $(BUILT_GIRSOURCES:.gir=.typelib)

%.typelib: %.gir $(INTROSPECTION_COMPILER)
	$(AM_V_GEN)$(INTROSPECTION_COMPILER) \
		--includedir=$(srcdir)/lib \
		--includedir=$(builddir)/lib \
		--includedir=`$(PKG_CONFIG) --variable=girdir gstreamer-1.0` \
		$(INTROSPECTION_COMPILER_OPTS) $< -o $(@F)

CLEANFILES += $(BUILT_GIRSOURCES) $(typelibs_DATA)
endif
