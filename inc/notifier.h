/* 
* notifier.h, contains the declarations for the daemon notification system
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

/* Function to extract the status of an application after starting it or that is already running in the system */
extern int AlGetAppState(DBusConnection * bus, char *app_name, char *state_info);
/* Function responsible to broadcast the state of an application that 
 * started execution or an application already running in the system  */
extern void AlAppStateNotifier(DBusConnection * bus, char *app_name);
/* Function responsible to issue a notification after a task chnages state (fg/bg) */
void AlChangeTaskStateNotifier(DBusConnection *p_conn, char *p_app_name, char *p_app_state);
/* Connect to the DBUS bus and send a broadcast signal regarding application state */
extern void AlSendAppSignal(DBusConnection * bus, char *name);

