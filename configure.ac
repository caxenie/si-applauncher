#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.65])

AC_INIT([si-applauncher], [1.6], [cristian.axenie@windriver.com])
AC_CONFIG_SRCDIR([src/al-daemon.c])
AC_CONFIG_HEADERS([config.h])

#AM inclusions
AM_INIT_AUTOMAKE([foreign 1.0 -Wall tar-pax dist-bzip2])
AM_MAINTAINER_MODE
#AM_PROG_LIBTOOL


# Checks for programs.
AC_PROG_MKDIR_P
AC_PROG_LN_S
AC_PROG_SED

AC_PROG_CC
AC_PROG_CC_C99
AM_PROG_CC_C_O
AC_PROG_GCC_TRADITIONAL
AC_USE_SYSTEM_EXTENSIONS
AC_HEADER_STDC
AC_ARG_PROGRAM
AC_CHECK_TOOL(OBJCOPY, objcopy)
AC_CHECK_TOOL(STRINGS, strings)

AC_SUBST(CFLAGS)
AC_SUBST(CPPFLAGS)
AC_SUBST(LDFLAGS)

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h unistd.h])


# This makes sure pkg.m4 is available.
m4_pattern_forbid([^_?PKG_[A-Z_]+$],[*** pkg.m4 missing, please install pkg-config])

PKG_CHECK_MODULES(DBUS, [ dbus-1 >= 1.2.16 ])
AC_SUBST(DBUS_CFLAGS)
AC_SUBST(DBUS_LIBS)

PKG_CHECK_MODULES(DBUSGLIB, [ dbus-glib-1 ])
AC_SUBST(DBUSGLIB_CFLAGS)
AC_SUBST(DBUSGLIB_LIBS)

PKG_CHECK_MODULES(GLIB2, [ glib-2.0 ])
AC_SUBST(GLIB2_CFLAGS)
AC_SUBST(GLIB2_LIBS)

PKG_CHECK_MODULES(GCONF, [ gconf-2.0 ])
AC_SUBST(GCONF_CFLAGS)
AC_SUBST(GCONF_LIBS)

AC_ARG_ENABLE([debug],
	    AS_HELP_STRING([--enable-debug],[Build al-daemon enabling syslog debug messages]),
	    [case "${enableval}" in
	    yes) debug=true ;;
	    no) debug=false ;;
	    *) AC_MSG_ERROR([bad value ${enableval} for --enable-debug]) ;;
	esac],[debug=false])
AM_CONDITIONAL([BUILD_WITH_DEBUG], [test "x$debug" = "xtrue"])

# Checks for libraries.
#AC_CHECK_HEADERS([glib.h],
#                [],
#                [AC_MSG_ERROR([*** glib headers not found.])
#		])

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.
AC_CHECK_FUNCS([memset sysinfo])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
