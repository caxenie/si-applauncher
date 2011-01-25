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
#define AL_CALLER_NAME "org.GENIVI.AppL.caller"
#define AL_SERVER_NAME "org.GENIVI.AppL.server"
#define AL_SIG_TRIGGER "org.GENIVI.AppL"
#define AL_SIG_LISTENER "org.GENIVI.AppL"
#define AL_METHOD_INTERFACE "org.GENIVI.AppL.method"
#define AL_SIGNAL_INTERFACE "org.GENIVI.AppL.signal"
#define SRM_OBJECT_PATH "/org/GENIVI/AppL"
#define AL_SIGNAME_TASK_STARTED "TaskStarted"
#define AL_SIGNAME_TASK_STOPPED "TaskStopped"
#define AL_SIGNAME_NOTIFICATION "GlobalStateNotification"
#define DIM_MAX 200

/* Tracing support */
unsigned char g_verbose = 0;
#define log_message(format,args...) \
			do{ \
			    if(g_verbose) \
				    fprintf(stdout,format,args); \
			  } while(0);

/* Function to extract PID value using the name of an application */
extern pid_t AppPidFromName(char *app_name);
/* Find application name from PID */
extern void AppNameFromPid(int pid, char *app_name);
/* High level interface for the AL Daemon */
extern void Run(bool isFg, int parentPID, char *commandLine);
extern void RunAs(int egid, int euid, bool isFg, int parentPID,
		  char *commandLine);
extern void Suspend(int pid);
extern void Resume(int pid);
extern void Stop(int egid, int euid, int pid);
/* Send start/stop signals over the bus to the clients */
extern void TaskStarted(char *imagePath, int pid);
extern void TaskStopped(char *imagePath, int pid);
/* Connect to the DBUS bus and send a broadcast signal regarding application state */
extern void AlSendAppSignal(char *name);
/* Receive the method calls and reply */
extern void AlReplyToMethodCall(DBusMessage * msg, DBusConnection * conn);
/* Server that exposes a method call and waits for it to be called */
extern void AlListenToMethodCall();
/* Function responsible to parse the .timer unit and extract the triggering key */
extern GKeyFile *ParseUnitFile(char *file);
/* Function responsible to parse the .timer unit and setup the triggering key value */
extern void SetupUnitFileKey(char *file, char *key, char *val, char *unit);
/* Function to extract the status of an application after starting it or that is already running in the system */
extern int AlGetAppState(DBusConnection * bus, char *app_name,
			 char *state_info);
/* Function responsible to broadcast the state of an application that started execution or an application already running in the system  */
extern void AlAppStateNotifier(char *app_name);
/* Function responsible to test if a given application exists in the system. */
extern int AppExistsInSystem(char *app_name);
#endif