ACLOCAL_AMFLAGS = -I m4

if HAVE_CHECK
tests_dir = tests
endif

if HAVE_GTK
gtk_dir = gtk
endif

SUBDIRS = lib gst-play $(gtk_dir) $(tests_dir) docs pkgconfig
DIST_SUBDIRS = android gst-play gtk ios lib tests docs pkgconfig

