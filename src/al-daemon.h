/* 
* al-daemon.h, contains the declarations of the message handler functions for the org.GENIVI.AppL.method DBus interface 
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
#define AL_VERSION "1.2"
#define AL_GCONF_CURRENT_USER_KEY "/current_user"
#define AL_GCONF_LAST_USER_MODE_KEY "/last_mode"
#define AL_PID_FILE "/var/run/al-daemon.pid"

#define DBUS_SRM_INTROSPECT_XML "" \
        "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD "\
        "D-BUS Object Introspection 1.0//EN\" "\
        "\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"\
        "<node name=\"" SRM_OBJECT_PATH "\">\n"\
        "  <interface name=\"" AL_METHOD_INTERFACE "\">\n"\
        "    <method name=\"Run\">\n"\
        "      <arg name=\"app_name\" type=\"s\" direction=\"in\"/>\n"\
        "      <arg name=\"status\" type=\"b\" direction=\"out\"/>\n"\
        "      <arg name=\"level\" type=\"u\" direction=\"out\"/>\n"\
        "    </method>\n"\
        "    <method name=\"RunAs\">\n"\
        "      <arg name=\"app_name\" type=\"s\" direction=\"in\"/>\n"\
        "      <arg name=\"app_uid\" type=\"i\" direction=\"in\"/>\n"\
        "      <arg name=\"app_gid\" type=\"i\" direction=\"in\"/>\n"\
        "      <arg name=\"status\" type=\"b\" direction=\"out\"/>\n"\
        "      <arg name=\"level\" type=\"u\" direction=\"out\"/>\n"\
        "    </method>\n"\
        "    <method name=\"Stop\">\n"\
        "      <arg name=\"app_name\" type=\"s\" direction=\"in\"/>\n"\
        "      <arg name=\"status\" type=\"b\" direction=\"out\"/>\n"\
        "      <arg name=\"level\" type=\"u\" direction=\"out\"/>\n"\
        "    </method>\n"\
        "    <method name=\"StopAs\">\n"\
        "      <arg name=\"app_name\" type=\"s\" direction=\"in\"/>\n"\
        "      <arg name=\"app_uid\" type=\"i\" direction=\"in\"/>\n"\
        "      <arg name=\"app_gid\" type=\"i\" direction=\"in\"/>\n"\
        "      <arg name=\"status\" type=\"b\" direction=\"out\"/>\n"\
        "      <arg name=\"level\" type=\"u\" direction=\"out\"/>\n"\
        "    </method>\n"\
        "    <method name=\"Resume\">\n"\
        "      <arg name=\"app_name\" type=\"s\" direction=\"in\"/>\n"\
        "      <arg name=\"status\" type=\"b\" direction=\"out\"/>\n"\
        "      <arg name=\"level\" type=\"u\" direction=\"out\"/>\n"\
        "    </method>\n"\
        "    <method name=\"Suspend\">\n"\
        "      <arg name=\"app_name\" type=\"s\" direction=\"in\"/>\n"\
        "      <arg name=\"status\" type=\"b\" direction=\"out\"/>\n"\
        "      <arg name=\"level\" type=\"u\" direction=\"out\"/>\n"\
        "    </method>\n"\
        "    <method name=\"Restart\">\n"\
        "      <arg name=\"app_name\" type=\"s\" direction=\"in\"/>\n"\
        "      <arg name=\"status\" type=\"b\" direction=\"out\"/>\n"\
        "      <arg name=\"level\" type=\"u\" direction=\"out\"/>\n"\
        "    </method>\n"\
        "    <method name=\"ChangeTaskState\">\n"\
        "      <arg name=\"app_pid\" type=\"i\" direction=\"in\"/>\n"\
        "      <arg name=\"foreground\" type=\"b\" direction=\"in\"/>\n"\
        "    </method>\n"\
        "  </interface>\n"\
        "  <interface name=\"" AL_SIGNAL_INTERFACE "\">\n"\
        "    <signal name=\"" AL_SIGNAME_NOTIFICATION "\">\n"\
        "      <arg name=\"app_status\" type=\"s\"/>\n"\
        "    </signal>\n"\
        "    <signal name=\"" AL_SIGNAME_TASK_STARTED "\">\n"\
        "      <arg name=\"app_state\" type=\"s\"/>\n"\
        "    </signal>\n"\
        "    <signal name=\"" AL_SIGNAME_TASK_STOPPED "\">\n"\
        "      <arg name=\"app_state\" type=\"s\"/>\n"\
        "    </signal>\n"\
        "  </interface>\n"\
        "</node>\n"

/* Tracing support */
unsigned char g_verbose = 0;
/* CLI commands */
unsigned char g_stop = 0;
unsigned char g_start = 0;
/* macro for logging */
#define log_message(format,args...) \
			do{ \
			    if(g_verbose) \
				    fprintf(stdout,format,args); \
			  } while(0);

/* Function to extract PID value using the name of an application */
extern pid_t AppPidFromName(char *app_name);
/* Find application name from PID */
extern int AppNameFromPid(int pid, char *app_name);
/* High level interface for the AL Daemon */
extern void Run(int newPID, bool isFg, int parentPID, char *commandLine);
extern void RunAs(int euid, int egid, int newPID, bool isFg, int parentPID,
		  char *commandLine);
extern void Suspend(int pid);
extern void Resume(int pid);
extern void Stop(int pid);
extern void StopAs(int pid, int euid, int egid);
extern void ChangeTaskState(int pid, bool isFg);
/* Send start/stop signals over the bus to the clients */
extern void TaskStarted(char *imagePath, int pid);
extern void TaskStopped(char *imagePath, int pid);
/* Connect to the DBUS bus and send a broadcast signal regarding application state */
extern void AlSendAppSignal(DBusConnection * bus, char *name);
/* Receive the method calls and reply */
extern void AlReplyToMethodCall(DBusMessage * msg, DBusConnection * conn);
/* Server that exposes a method call and waits for it to be called */
extern void AlListenToMethodCall();
/* Function responsible to parse the .timer unit and extract the triggering key */
extern GKeyFile *ParseUnitFile(char *file);
/* Function responsible to parse the service unit and extract ownership info */
extern void ExtractOwnershipInfo(char *euid, char *egid, char *file);
/* Function responsible to parse the .timer unit and setup the triggering key value */
extern void SetupUnitFileKey(char *file, char *key, char *val, char *unit);
/* Function to extract the status of an application after starting it or that is already running in the system */
extern int AlGetAppState(DBusConnection * bus, char *app_name,
			 char *state_info);
/* Function responsible to broadcast the state of an application that started execution or an application already running in the system  */
extern void AlAppStateNotifier(DBusConnection * bus, char *app_name);
/* Function responsible to test if a given application exists in the system. */
extern int AppExistsInSystem(char *app_name);
/* Function responsible with restarting an application when SHM detects an abnormal operation of the application */
extern void Restart(char *app_name);
/* Function responsible to extract the user name from the uid */
extern void MapUidToUser(int uid, char *user);
/* Function responsible to extract the group name from the gid */
extern void MapGidToGroup(int gid, char *group);
/* Function responsible to get the current user as specified in the current_user GConf key and start the last user mode apps */
extern int GetCurrentUser(GConfClient* client, GConfEntry* key, char *user);
/* Function responsible to start the specific applications for the current user mode */
extern void StartUserModeApps(GConfClient *client, char *user);
/* Function responsible to initialize the last user mode at daemon startup */
extern void InitializeLastUserMode();
/* Signal handler for the daemon */
extern void AlSignalHandler(int sig);
/* Function responsible with the daemonization procedure */
extern void AlDaemonize();
#endif
