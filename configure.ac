AC_INIT([gst-player], [0.0.1])
AC_CONFIG_SRCDIR([Makefile.am])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS(config.h)

AM_INIT_AUTOMAKE([-Wno-portability 1.11 no-dist-gzip dist-xz tar-ustar subdir-objects])
AM_MAINTAINER_MODE

LT_PREREQ([2.2.6])
LT_INIT([dlopen win32-dll pic-only])

m4_ifdef([AM_SILENT_RULES],
  [AM_SILENT_RULES([yes])],
  [
    AM_DEFAULT_VERBOSITY=1
    AC_SUBST(AM_DEFAULT_VERBOSITY)
  ]
)

AC_PROG_CC
AC_PROG_CC_STDC
AC_PROG_CPP
AC_PROG_INSTALL
AM_PROG_CC_C_O

AC_HEADER_STDC
AC_C_CONST
AC_C_INLINE
AC_ISC_POSIX
AC_SYS_LARGEFILE

AC_C_BIGENDIAN

AC_CHECK_LIBM
AC_SUBST(LIBM)

PKG_PROG_PKG_CONFIG

PKG_CHECK_MODULES(GLIB, [glib-2.0 >= 2.38.0 gobject-2.0 >= 2.38.0])
PKG_CHECK_MODULES(GSTREAMER, [gstreamer-1.0 >= 1.7.1 gstreamer-tag-1.0 gstreamer-player-1.0])

GLIB_PREFIX="`$PKG_CONFIG --variable=prefix glib-2.0`"
AC_SUBST(GLIB_PREFIX)
GST_PREFIX="`$PKG_CONFIG --variable=prefix gstreamer-1.0`"
AC_SUBST(GST_PREFIX)

PKG_CHECK_MODULES(GTK, [gtk+-3.0 >= 3.14], [have_gtk="yes"], [have_gtk="no"])
AM_CONDITIONAL(HAVE_GTK, test "x$have_gtk" != "xno")

PKG_CHECK_MODULES(GMODULE, [gmodule-2.0], [have_gmodules="yes"], [have_modules="no"])
AM_CONDITIONAL(HAVE_GMODULE, test "x$have_gmodules" != "xno")

PKG_CHECK_MODULES(GTK_X11, [gtk+-x11-3.0 x11], [have_gtk_x11="yes"], [have_gtk_x11="no"])
AM_CONDITIONAL(HAVE_GTK_X11, test "x$have_gtk_x11" != "xno")

WARNING_CFLAGS="-Wall"

AC_ARG_ENABLE(more-warnings,
AC_HELP_STRING([--enable-more-warnings], [Enable more compiler warnings]),
set_more_warnings="$enableval", set_more_warnings=no)

AC_MSG_CHECKING(for more warnings)
if test "$GCC" = "yes" -a "$set_more_warnings" != "no"; then
  AC_MSG_RESULT(yes)

  for option in -Wchar-subscripts -Wmissing-declarations -Wmissing-prototypes -Wnested-externs -Wpointer-arith -Wredundant-decls -Wundef -Wwrite-strings -Wformat-nonliteral -Wformat-security -Wold-style-definition -Winit-self -Wmissing-include-dirs -Waddress -Waggregate-return -Wno-multichar -Wunused-variable; do
    SAVE_CFLAGS="$CFLAGS"
    CFLAGS="$CFLAGS $WARNING_CFLAGS $option"
    AC_MSG_CHECKING([whether gcc understands $option])
    AC_TRY_COMPILE([], [],
      has_option=yes,
      has_option=no,)
    if test x"$has_option" = "xyes"; then
      WARNING_CFLAGS="$WARNING_CFLAGS $option"
    fi
    CFLAGS="$SAVE_CFLAGS"
    AC_MSG_RESULT($has_option)
    unset has_option
    unset SAVE_CFLAGS
  done
  unset option
else
  AC_MSG_RESULT(no)
fi

AC_ARG_ENABLE(warnings-as-errors,
AC_HELP_STRING([--enable-warnings-as-errors], [Handle compiler warnings as errors]),
set_werror="$enableval", set_werror=no)

AC_MSG_CHECKING(for handling compiler warnings as errors)
if test "$GCC" = "yes" -a "$set_werror" != "no"; then
  AC_MSG_RESULT(yes)
  WARNING_CFLAGS="$WARNING_CFLAGS -Werror"
else
  AC_MSG_RESULT(no)
fi

AC_SUBST(WARNING_CFLAGS)

AC_CONFIG_FILES([
  Makefile
  gst-play/Makefile
  gtk/Makefile
  android/Makefile
  ios/Makefile
])

AC_OUTPUT
