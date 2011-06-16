/* 
* utils.h, contains the declarations for various utility functions used in the daemon code
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

/* Function responsible with the daemonization procedure */
extern void AlDaemonize();
/* Function responsible to shutdown the daemon process */
extern void AlDaemonShutdown();
/* Function to extract PID value using the name of an application */
extern pid_t AppPidFromName(char *app_name);
/* Find application name from PID */
extern int AppNameFromPid(int pid, char *app_name);
/* Function responsible to test if a given application exists in the system. */
extern int AppExistsInSystem(char *app_name);
/* Function responsible to parse the .timer unit and extract the triggering key */
extern GKeyFile *ParseUnitFile(char *file);
/* Function responsible to parse the .timer unit and setup the triggering key value */
extern void SetupUnitFileKey(char *file, char *key, char *val, char *unit);
/* Function responsible to setup the (fg/bg) state when starting the application
 * for the first time using Run or RunAs */
extern int SetupApplicationStartupState(DBusConnection *p_conn, char *p_app, bool l_fg_state);

