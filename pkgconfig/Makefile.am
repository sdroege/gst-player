pcfiles = \
	gstreamer-player-@GST_PLAYER_API_VERSION@.pc
	
pcfiles_uninstalled = \
	gstreamer-player-@GST_PLAYER_API_VERSION@-uninstalled.pc

all-local: $(pcfiles) $(pcfiles_uninstalled)

### how to generate pc files
%-@GST_PLAYER_API_VERSION@.pc: %.pc
	cp $< $@
%-@GST_PLAYER_API_VERSION@-uninstalled.pc: %-uninstalled.pc
	cp $< $@

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(pcfiles)

EXTRA_DIST = \
	gstreamer-player.pc.in \
	gstreamer-player-uninstalled.pc.in
CLEANFILES = $(pcfiles) $(pcfiles_uninstalled)
