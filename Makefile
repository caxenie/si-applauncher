CFLAGS=-Wall -g -ldbus-1 -I/usr/include/dbus-1.0 -I/usr/lib/dbus-1.0/include
PKGCONFIG=`pkg-config --libs --cflags dbus-1`
daemon_SOURCES=src/al-daemon.c
daemon_EXEC=al-daemon
EXECUTABLES=al-daemon

all: $(daemon_EXEC)

al-daemon:
	$(CC) $(CFLAGS) $(PKGCONFIG) $(daemon_SOURCES) -o $(daemon_EXEC)

clean:
	rm -rf $(EXECUTABLES)

