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
extern void Run(int newPID, bool isFg, int parentPID, char *commandLine);
extern void RunAs(int euid, int egid, int newPID, bool isFg, int parentPID, char *commandLine);
extern void Suspend(int pid);
extern void Resume(int pid);
extern void Stop(int pid);
extern void StopAs(int pid, int euid, int egid);
extern void ChangeTaskState(int pid, bool isFg);
/* Send start/stop signals over the bus to the clients */
extern void TaskStarted(char *imagePath, int pid);
extern void TaskStopped(char *imagePath, int pid);
/* Function responsible with restarting an application when SHM detects an abnormal operation of the application */
extern void Restart(char *app_name);
/* Receive the method calls and reply */
extern void AlReplyToMethodCall(DBusMessage * msg, DBusConnection * conn);/*
/* Replies to Introspection request */
void ReplyToIntrospect(DBusMessage *p_msg, DBusConnection *p_conn);
