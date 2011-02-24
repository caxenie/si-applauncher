ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = al-daemon
al_daemon_SOURCES = src/al-daemon.c \
			src/al-daemon.h
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
