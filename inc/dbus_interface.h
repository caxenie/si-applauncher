/* 
* dbus_interface.h , contains the declaration of the Dbus handler functions
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

/* High level interface for the AL Daemon */
extern void Run(char *p_commandLine, int p_parentPID, bool p_isFg);
extern void RunAs(char *p_commandLine, int p_parentPID, bool p_isFg, int p_euid, int p_egid);
extern void Suspend(int pid);
extern void Resume(int pid);
extern void Stop(int pid);
extern void StopAs(int pid, int euid, int egid);
extern void ChangeTaskState(int pid, bool isFg);
/* Send start/stop signals over the bus to the clients */
extern void TaskStarted(int p_pid, char *p_imagePath);
extern void TaskStopped(int p_pid, char *p_imagePath);
/* Function responsible with restarting an application when SHM detects an abnormal operation of the application */
extern void Restart(char *app_name);
/* Function responsible to dispatch and emit signals according to context */
extern void al_dbus_signal_dispatcher();
/* Function responsible to monitor signals of interest for the daemon */
extern int al_monitor_signals(DBusConnection *bus);
