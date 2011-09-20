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

DBUS_BINDING_TOOL = dbus-binding-tool
DBUS_BINDING_MODE = glib-server

AL_DBUS_GLUE_PREFIX = al_dbus
AL_DBUS_GLUE_XML = src/al_dbus.xml
AL_DBUS_GLUE_FILE = inc/al_dbus-glue.h

all: bindings custom_marshallers

bindings: $(AL_DBUS_GLUE_XML)
	$(DBUS_BINDING_TOOL) --mode=$(DBUS_BINDING_MODE) --prefix=$(AL_DBUS_GLUE_PREFIX) $(AL_DBUS_GLUE_XML) > $(AL_DBUS_GLUE_FILE)

custom_marshallers:
	echo "VOID:INT,STRING" | glib-genmarshal --body --prefix=$(AL_DBUS_GLUE_PREFIX) > src/task_info_custom_marshaller.c
	echo "VOID:STRING,STRING" | glib-genmarshal --body --prefix=$(AL_DBUS_GLUE_PREFIX) > src/task_state_change_custom_marshaller.c


if BUILD_WITH_DEBUG
    al_daemon_CFLAGS += -DDEBUG 
endif

# Dirs of external packages
pkgconfigdatadir=$(datadir)/pkgconfig

#Special dirs
rootdir=@rootdir@
rootbindir=$(rootdir)/bin
rootsbindir=$(rootdir)/sbin
