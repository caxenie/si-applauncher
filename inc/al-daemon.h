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
#define AL_VERSION "1.6"
#define AL_GCONF_CURRENT_USER_KEY "/current_user"
#define AL_GCONF_LAST_USER_MODE_KEY "/last_mode"
#define AL_PID_FILE "/var/run/al-daemon.pid"
/* add logging support */
#include "al-log.h"

#define DBUS_SRM_INTROSPECT_XML "" \
        "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD "\
        "D-BUS Object Introspection 1.0//EN\" "\
        "\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"\
        "<node name=\"" SRM_OBJECT_PATH "\">\n"\
        "  <interface name=\"" AL_METHOD_INTERFACE "\">\n"\
        "    <method name=\"Run\">\n"\
        "      <arg name=\"app_name\" type=\"s\" direction=\"in\"/>\n"\
	"      <arg name=\"foreground\" type=\"b\" direction=\"in\"/>\n"\
        "      <arg name=\"status\" type=\"b\" direction=\"out\"/>\n"\
        "      <arg name=\"level\" type=\"u\" direction=\"out\"/>\n"\
        "    </method>\n"\
        "    <method name=\"RunAs\">\n"\
        "      <arg name=\"app_name\" type=\"s\" direction=\"in\"/>\n"\
        "      <arg name=\"app_uid\" type=\"i\" direction=\"in\"/>\n"\
        "      <arg name=\"app_gid\" type=\"i\" direction=\"in\"/>\n"\
	"      <arg name=\"foreground\" type=\"b\" direction=\"in\"/>\n"\
        "      <arg name=\"status\" type=\"b\" direction=\"out\"/>\n"\
        "      <arg name=\"level\" type=\"u\" direction=\"out\"/>\n"\
        "    </method>\n"\
        "    <method name=\"Stop\">\n"\
        "      <arg name=\"app_pid\" type=\"u\" direction=\"in\"/>\n"\
        "      <arg name=\"status\" type=\"b\" direction=\"out\"/>\n"\
        "      <arg name=\"level\" type=\"u\" direction=\"out\"/>\n"\
        "    </method>\n"\
        "    <method name=\"StopAs\">\n"\
        "      <arg name=\"app_pid\" type=\"u\" direction=\"in\"/>\n"\
        "      <arg name=\"app_uid\" type=\"i\" direction=\"in\"/>\n"\
        "      <arg name=\"app_gid\" type=\"i\" direction=\"in\"/>\n"\
        "      <arg name=\"status\" type=\"b\" direction=\"out\"/>\n"\
        "      <arg name=\"level\" type=\"u\" direction=\"out\"/>\n"\
        "    </method>\n"\
        "    <method name=\"Resume\">\n"\
        "      <arg name=\"app_pid\" type=\"u\" direction=\"in\"/>\n"\
        "      <arg name=\"status\" type=\"b\" direction=\"out\"/>\n"\
        "      <arg name=\"level\" type=\"u\" direction=\"out\"/>\n"\
        "    </method>\n"\
        "    <method name=\"Suspend\">\n"\
        "      <arg name=\"app_pid\" type=\"u\" direction=\"in\"/>\n"\
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
	"      <arg name=\"status\" type=\"b\" direction=\"out\"/>\n"\
        "      <arg name=\"level\" type=\"u\" direction=\"out\"/>\n"\
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

/* Function responsible with the command line interface output */
extern void AlPrintCLI();
/* Function responsible with command line options parsing */
extern void AlParseCLIOptions(int argc, char *const *argv);
/* Server that exposes methods and waits for it to be called */
extern void AlListenToMethodCall();
/* Signal handler for the daemon */
extern void AlSignalHandler(int sig);

#endif
