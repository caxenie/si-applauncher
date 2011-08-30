/* 
* al-daemon.h, contains the main declarations of the main application launcher loop
* 
* Copyright (c) 2010 Wind River Systems, Inc. 
* 
* This program is free software; you can redistribute it and/or modify 
* it under the terms of the GNU General Public License version 2 as 
* published by the Free Software Foundation. 
* 
* This program is distributed in the hope that it will be useful, 
* but WITHOUT ANY WARRANTY; without even the implied warranty of 
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 
* See the GNU General Public License for more details. 
* 
* You should have received a copy of the GNU General Public License 
* along with this program; if not, write to the Free Software 
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA 
* 
*/

/* Application launcher daemon header file */
#ifndef __AL_H
#define __AL_H

/* Application Launcher DBus interface */
#define AL_DBUS_SERVICE "org.GENIVI.AppL"
#define AL_SERVER_NAME "org.GENIVI.AppL"
#define AL_SIG_TRIGGER "org.GENIVI.AppL"
#define AL_SIG_LISTENER "org.GENIVI.AppL"
#define AL_METHOD_INTERFACE "org.GENIVI.AppL"
#define AL_SIGNAL_INTERFACE "org.GENIVI.AppL"
#define SRM_OBJECT_PATH "/org/GENIVI/AppL"
#define AL_SIGNAME_TASK_STARTED "TaskStarted"
#define AL_SIGNAME_TASK_STOPPED "TaskStopped"
#define AL_SIGNAME_NOTIFICATION "GlobalStateNotification"
#define DIM_MAX 200
#define AL_VERSION "2.0"
#define AL_GCONF_CURRENT_USER_KEY "/current_user"
#define AL_GCONF_LAST_USER_MODE_KEY "/last_mode"
#define AL_PID_FILE "/var/run/al-daemon.pid"
#define SYSTEMD_SERVICE_NAME         "org.freedesktop.systemd1"
#define SYSTEMD_INTERFACE            "org.freedesktop.systemd1.Manager"
#define SYSTEMD_PATH                 "/org/freedesktop/systemd1"
#define SYSTEMD_UNIT_INFO_TIMEOUT 2000

/* add logging support */
#include "al-log.h"
#include <dbus/dbus.h>
#include <dbus/dbus-glib-bindings.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <pthread.h>

/* signal dispatcher thread defines */
static pthread_t signal_dispatch_thread_id;

/* Structure representing the AL Daemon DBus object */
typedef struct 
{
    GObject parent;
} ALDbus;

/* list of signals for the AL Daemon */
enum
{
AL_SIG_TASK_STARTED,
AL_SIG_TASK_STOPPED,
AL_SIG_GLOBAL_NOTIFICATION,
AL_SIG_CHANGE_STATE_COMPLETE,
AL_SIG_COUNT
};

/* Structure representing the class for the AL Daemon DBus object */
typedef struct
{
    GObjectClass parent;
    guint ALSignals[AL_SIG_COUNT];
} ALDbusClass;

/*
 * This function is provided by the G_DEFINE_TYPE macro. It is used
 * when creating a new object and registering it on the system bus.
 */
GType al_dbus_get_type(void);

/* AL Daemon API callbacks */

/* method calls */

gboolean al_dbus_run(
		ALDbus *server,
		gchar *command_line,
		gint parent_pid,
		gboolean foreground,
		DBusGMethodInvocation *context
);

gboolean al_dbus_run_as(
		ALDbus *server,
		gchar *command_line,
		gint parent_pid,
		gboolean foreground,
		gint app_uid,
		gint app_gid,
		DBusGMethodInvocation *context
);

gboolean al_dbus_stop(
		ALDbus *server,
		gint app_pid,
		DBusGMethodInvocation *context
);

gboolean al_dbus_resume(
		ALDbus *server,
		gint app_pid,
		DBusGMethodInvocation *context
);

gboolean al_dbus_suspend(
		ALDbus *server,
		gint app_pid,
		DBusGMethodInvocation *context
);

gboolean al_dbus_stop_as(
		ALDbus *server,
		gint app_pid,
		gint app_uid,
		gint app_gid,
		DBusGMethodInvocation *context
);

gboolean al_dbus_restart(
		ALDbus *server,
		gchar* app_name,
		DBusGMethodInvocation *context
);

gboolean al_dbus_change_task_state(
		ALDbus *server,
		gint app_pid,
		gboolean foreground,
		DBusGMethodInvocation *context
);

/* signals */

gboolean al_dbus_global_state_notification(
		ALDbus *server,
		gchar *app_status
);

gboolean al_dbus_task_started(
		ALDbus *server,
		gint app_pid,
		gchar *image_path
);

gboolean al_dbus_task_stopped(
		ALDbus *server,
		gint app_pid,
		gchar *image_path
);

gboolean al_dbus_change_task_state_complete(
			ALDbus *server,
			gchar *app_name,
			gchar *app_state
);

/* Function responsible to initialize the AL Daemon DBus interface */

gboolean initialize_al_dbus();

/* Function responsible to cleanup the resources associated with the AL Daemon DBus interface 
 */

gboolean terminate_al_dbus();

/* Function responsible with the command line interface output */
extern void AlPrintCLI();
/* Function responsible with command line options parsing */
extern void AlParseCLIOptions(int argc, char *const *argv);
/* Signal handler for the daemon */
extern void AlSignalHandler(int sig);

#endif
