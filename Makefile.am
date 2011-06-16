ACLOCAL_AMFLAGS = -I m4

AM_CPPFLAGS = -I$(top_srcdir)/inc

bin_PROGRAMS = al-daemon
al_daemon_SOURCES = src/al-daemon.c \
		    src/dbus_interface.c \
		    src/lum.c \
		    src/utils.c \
		    src/notifier.c \
		    inc/al-daemon.h \
		    inc/dbus_interface.h \
		    inc/lum.h \
		    inc/utils.h \
		    inc/notifier.h \
		    config.h 
al_daemon_LDADD = $(INTLLIBS) $(DBUS_LIBS) $(DBUSGLIB_LIBS) $(GLIB2_LIBS)
al_daemon_CFLAGS = \
		$(AM_CFLAGS) \
		$(DBUS_CFLAGS) \
		$(DBUS_LIBS) \
		$(DBUS_GLIB_FLAGS) \
		$(GLIB2_CFLAGS) \
		$(GLIB2_LIBS) \
		$(GCONF_LIBS) \
		$(GCONF_CFLAGS)

# Dirs of external packages
pkgconfigdatadir=$(datadir)/pkgconfig

#Special dirs
rootdir=@rootdir@
rootbindir=$(rootdir)/bin
rootsbindir=$(rootdir)/sbin
