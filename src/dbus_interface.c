/* 
* dbus_interface.c, contains the implementation of the Dbus handler functions
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

#include <ctype.h>
#include <dbus/dbus.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <gconf/gconf-client.h>
#include <getopt.h>
#include <glib/gstdio.h>
#include <glib.h>
#include <grp.h>
#include <libgen.h>
#include <pwd.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/user.h>
#include <unistd.h>

#include "dbus_interface.h"
#include "utils.h"
#include "lum.h"
#include "al-daemon.h"

/* High level interface for the AL Daemon */
void Run(int p_newPID, bool p_isFg, int p_parentPID, char *p_commandLine)
{
  // TODO isFg and parentPID usage when starting applications
  // [out] newPID: INT32, isFg: BOOLEAN, parentPID: INT32, commandLine: STRING
  /* store the return code */
  int l_ret;
  /* application PID */
  int l_pid;
  /* state string */
  char *l_flag = malloc(DIM_MAX*sizeof(l_flag));
  log_message("AL Daemon Run : %s started with run !\n", p_commandLine);
  /* the command line for the application */
  char l_cmd[DIM_MAX];
  /* form the call string for systemd */
  /* check if template / simple service will be started with run */
  if((AppExistsInSystem(p_commandLine))==1){
  	sprintf(l_cmd, "systemctl start %s.service", p_commandLine);
  }
  /* check if target (group of apps) will be started with run */
  if((AppExistsInSystem(p_commandLine))==2){
  	sprintf(l_cmd, "systemctl start %s.target", p_commandLine);
  }
  /* check if the unit has an associated timer and adjust the call string */
  if (strcmp(p_commandLine, "reboot") == 0) {
    sprintf(l_cmd, "systemctl start %s.timer", p_commandLine);
  }
  if (strcmp(p_commandLine, "shutdown") == 0) {
    sprintf(l_cmd, "systemctl start %s.timer", p_commandLine);
  }
  if (strcmp(p_commandLine, "poweroff") == 0) {
    sprintf(l_cmd, "systemctl start %s.timer", p_commandLine);
  }
  
  /* test application state */
  if(p_isFg==TRUE) strcpy(l_flag,"foreground");
  else strcpy(l_flag,"background");
  /* change the state of the application given by pid */
  log_message("AL Daemon Run : Application %s will run in %s \n", p_commandLine, l_flag);
  /* systemd invocation */
  l_ret = system(l_cmd);
  if (l_ret == -1) {
   if((AppExistsInSystem(p_commandLine))==1){
    log_message
	("AL Daemon Run : Application cannot be started with run! Err: %s\n",
	 strerror(errno));
    }
    if((AppExistsInSystem(p_commandLine))==2){
    log_message
	("AL Daemon Run : Applications group cannot be started with run! Err: %s\n",
	 strerror(errno));
    }
    return;
  }
  /* extract the PID from the application name only if the start is valid */
  l_pid = (int)AppPidFromName(p_commandLine);
  p_newPID = l_pid;
  log_message("AL Daemon Run : %s was started with run !\n", p_commandLine);
}

void RunAs(int p_egid, int p_euid, int p_newPID, bool p_isFg, int p_parentPID,
	   char *p_commandLine)
{
  // TODO egid, euid, isFg and parentPID usage when starting applications 
  // egid: INT32, euid: INT32, [out] newPID: INT32, isFg: BOOLEAN, parentPID: INT32, commandLine: STRING
  /* store the return code */
  int l_ret;
  /* application PID */
  int l_pid;
  /* local handlers for user and group to be written in the service file */
  char *l_user = malloc(DIM_MAX*sizeof(l_user));
  char *l_group = malloc(DIM_MAX*sizeof(l_group));
  log_message("AL Daemon RunAs : %s started with runas !\n", p_commandLine);
  /* the command line for the application */
  char l_cmd[DIM_MAX];
  /* the service file path */
  char l_srv_path[DIM_MAX];
  /* string that will store the state */
  char *l_flag = malloc(DIM_MAX*sizeof(l_flag));
  /* check if template */
  char *l_temp = ExtractUnitNameTemplate(p_commandLine);
  if(l_temp==NULL){
	log_message("AL Daemon RunAs : Cannot allocate template handler for %s !\n", p_commandLine);
	return;
  }
  sprintf(l_srv_path, "/lib/systemd/system/%s.service", l_temp);
  /* form the call string for systemd */
    sprintf(l_cmd, "systemctl start %s.service", l_temp);
  /* check if the unit has an associated timer and adjust the call string */
  if (strcmp(p_commandLine, "reboot") == 0) {
    sprintf(l_cmd, "systemctl start %s.timer", p_commandLine);
  }
  if (strcmp(p_commandLine, "shutdown") == 0) {
    sprintf(l_cmd, "systemctl start %s.timer", p_commandLine);
  }
  if (strcmp(p_commandLine, "poweroff") == 0) {
    sprintf(l_cmd, "systemctl start %s.timer", p_commandLine);
  }
  /* extract user name and group name from uid and gid */
  if(MapUidToUser(p_euid, l_user)!=0){
	log_message("AL Daemon RunAs : Cannot map uid to user for %s\n", p_commandLine);
	return;
  }
  if(MapGidToGroup(p_egid, l_group)!=0){
	log_message("AL Daemon RunAs : Cannot map gid to user for %s\n", p_commandLine);
	return;
  }
  /* SetupUnitFileKey call to setup the key and corresponding values, if not existing will be created */
  /* the service file will be updated with proper Group and User information */
  SetupUnitFileKey(l_srv_path, "User", l_user, p_commandLine);
  SetupUnitFileKey(l_srv_path, "Group", l_group, p_commandLine); 
  log_message("AL Daemon RunAs : Service file was updated with User=%s and Group=%s information !\n", l_user, l_group);
  /* test application state */
  if(p_isFg==TRUE) strcpy(l_flag,"foreground");
  else strcpy(l_flag,"background");
  /* issue daemon reload to apply and acknowledge modifications to the service file on the disk */
  l_ret = system("systemctl daemon-reload --system"); 
  if (l_ret == -1) {
    log_message
	("AL Daemon RunAs : After setting the service file reload systemd manager configuration failed ! Err: %s\n",
	 strerror(errno));
     return;
  }
  /* change the state of the application given by pid */
  log_message("AL Daemon RunAs : Application %s will runas %s in %s \n", p_commandLine, l_user, l_flag);

  /* systemd invocation */
  l_ret = system(l_cmd);
  if (l_ret == -1) {
    log_message
	("AL Daemon RunAs : Application cannot be started with runas! Err: %s\n",
	 strerror(errno));
     return;
  }
  /* extract the PID from the application name only if the start is valid */
  l_pid = (int)AppPidFromName(p_commandLine);
  p_newPID = l_pid;
  log_message("AL Daemon RunAs : %s was started with runas !\n", p_commandLine);
}

void Suspend(int p_pid)
{
  /* return code */
  int l_ret;
  /* to suspend the application a SIGSTOP signal is sent */
  if((l_ret=kill(p_pid, SIGSTOP))==-1){
	log_message("AL Daemon Suspend : %s cannot be suspended ! Err : %s\n", p_pid, strerror(errno));
   }
}

void Resume(int p_pid)
{
   /* return code */
  int l_ret;
  /* to suspend the application a SIGSTOP signal is sent */
  if((l_ret=kill(p_pid, SIGCONT))==-1){
	log_message("AL Daemon Resume : %s cannot be resumed ! Err : %s\n", p_pid, strerror(errno));
   }
}

void Stop(int p_pid)
{
  /* store the return code */
  int l_ret;
  /* stores the application name */
  char l_app_name[DIM_MAX];
  /* command line for the application */
  char *l_commandLine;
  if(AppNameFromPid(p_pid, l_app_name)!=0){
  l_commandLine = l_app_name;
  char l_cmd[DIM_MAX];
  log_message("AL Daemon Stop : %s stopped with stop !\n", l_commandLine);
  /* form the systemd command line string */
   /* check if single service will be stopped */
  if((AppExistsInSystem(l_commandLine))==1){
  	sprintf(l_cmd, "systemctl stop %s.service", l_commandLine);
  }
  /* call systemd */
  l_ret = system(l_cmd);
  if (l_ret != 0) {
    log_message
	("AL Daemon Stop : Application cannot be stopped with stop! Err: %s\n",
	 strerror(errno));
   }
  }else{
    	log_message("AL Daemon Stop : Application %s cannot be stopped because is already stopped !\n", l_commandLine); 
 }
}

void StopAs(int p_pid, int p_euid, int p_egid){ 
  /* store the return code */
  int l_ret;
  /* stores the application name */
  char l_app_name[DIM_MAX];
  /* command line for the application */
  char *l_commandLine = malloc(DIM_MAX*sizeof(l_commandLine));
  /* extracted user and group values from service file */
  char *l_group = malloc(DIM_MAX*sizeof(l_group));
  char *l_user = malloc(DIM_MAX*sizeof(l_user));
  /* application service fiel path */
  char l_srv_path[DIM_MAX];
   /* group and user strings */
  char *l_str_egid = malloc(DIM_MAX*sizeof(l_str_egid));
  char *l_str_euid = malloc(DIM_MAX*sizeof(l_str_euid));
  /* test if application runs in the system */
  if(AppNameFromPid(p_pid, l_app_name)!=0){
  l_commandLine = l_app_name;
  /* for the path to the application service */
  sprintf(l_srv_path, "/lib/systemd/system/%s.service", l_commandLine);
  /* low level command to systemd */
  char l_cmd[DIM_MAX];
  log_message("AL Daemon StopAs : %s stopped with stopas !\n", l_app_name);
  /* form the systemd command line string */
  sprintf(l_cmd, "systemctl stop %s.service", l_commandLine);
  /* test ownership and rights before stopping application */
  log_message("AL Daemon StopAs : Extracting ownership info for %s\n", l_commandLine);
  ExtractOwnershipInfo(l_user, l_group, l_srv_path);
  log_message("AL Daemon StopAs : The ownership information was extracted properly [ user : %s ] and [ group : %s ]\n", l_user, l_group);
  /* extract user name and group name from uid and gid */
  if(MapUidToUser(p_euid, l_str_euid)!=0){
	log_message("AL Daemon RunAs : Cannot map uid to user for %s\n", l_commandLine);
	return;
  }
  if(MapGidToGroup(p_egid, l_str_egid)!=0){
	log_message("AL Daemon RunAs : Cannot map gid to user for %s\n", l_commandLine);
	return;
  }
  log_message("AL Daemon StopAs : The input ownership information [ user: %s ] and [ group : %s ]\n", l_str_euid, l_str_egid);
  if((strcmp(l_user, l_str_euid)!=0) && (strcmp(l_group, l_str_egid)!=0)){
	log_message("AL Daemon StopAs : The current user doesn't have permissions to stopas %s!\n", l_commandLine); 
 	return;
  }
  /* call systemd */
  l_ret = system(l_cmd);
  if (l_ret != 0) {
    log_message("AL Daemon StopAs : Application cannot be stopped! Err:%s\n",
		strerror(errno));
   }
  }else{
    	log_message("AL Daemon StopAs : Application %s cannot be stopped because is already stopped !\n", l_commandLine); 
 }
}

void TaskStarted(char *p_imagePath, int p_pid)
{
  log_message
      ("AL Daemon TaskStarted Signal : Task %d %s was started and signal %s was emitted!\n",
       p_pid, p_imagePath, AL_SIGNAME_TASK_STARTED);
}

void TaskStopped(char *p_imagePath, int p_pid)
{
  log_message
      ("AL Daemon TaskStopped Signal : Task %d %s was stopped and signal %s was emitted!\n",
       p_pid, p_imagePath, AL_SIGNAME_TASK_STOPPED);
}

void ChangeTaskState(int p_pid, bool p_isFg)
{
  char *l_flag = malloc(DIM_MAX*sizeof(l_flag));
  if(p_isFg==TRUE) strcpy(l_flag,"foreground");
  else strcpy(l_flag,"background");
  /* change the state of the application given by pid */
 log_message("AL Daemon ChangeTaskState : Application with pid %d changed state to %s \n", p_pid, l_flag);
}

/* Function responsible with restarting an application when the SHM component detects
 * an abnormal operation of the application
 */
void Restart(char *p_app_name)
{
  int l_ret;
  /* the command line for the application */
  char l_cmd[DIM_MAX];
  log_message("AL Daemon Restart : %s will be restarted !\n", p_app_name);
   /* check if single service will be restarted */
  if((AppExistsInSystem(p_app_name))==1){
  	sprintf(l_cmd, "systemctl restart %s.service", p_app_name);
  }
  /* check if target (group of apps) will be restarted */
  if((AppExistsInSystem(p_app_name))==2){
  	sprintf(l_cmd, "systemctl restart %s.target", p_app_name);
  }
  /* systemd invocation */
  l_ret = system(l_cmd);
  if (l_ret != 0) {
    if((AppExistsInSystem(p_app_name))==1){
    log_message
	("AL Daemon Restart : Application cannot be restarted with restart! Err: %s\n",
	 strerror(errno));
    }
    if((AppExistsInSystem(p_app_name))==2){
    log_message
	("AL Daemon Restart : Applications group cannot be restarted with restart! Err: %s\n",
	 strerror(errno));
    }
  }
} 

/* Receive the method calls and reply */
void AlReplyToMethodCall(DBusMessage * p_msg, DBusConnection * p_conn)
{
  /* reply message */
  DBusMessage *l_reply;
  /* reply arguments */
  DBusMessageIter l_args;
  dbus_bool_t l_stat = TRUE;
  /* reply status */
  dbus_uint32_t l_level = 21614;
  dbus_uint32_t l_serial = 0;
  /* initialized parameters */
  char *l_param = "";
  int l_param_int = 0;
  /* arg type switch according to api call parameters */
  bool l_name_switch = FALSE, l_pid_switch = FALSE;
  /* read the arguments */
  if (!dbus_message_iter_init(p_msg, &l_args)) {
    log_message
	("AL Daemon Reply to Method Call : Message has no arguments!%s",
	 "\n");
  } else{ 
 
  if ((DBUS_TYPE_STRING == dbus_message_iter_get_arg_type(&l_args))) {
     /* get argument from the message (app name)*/
     dbus_message_iter_get_basic(&l_args, &l_param);
     /* setup the arg type switch */
     l_name_switch = TRUE;
     log_message
	("AL Daemon Reply to Method Call : Method called for %s\n",
	 l_param);
  }else if ((DBUS_TYPE_UINT32 == dbus_message_iter_get_arg_type(&l_args))) {
   /* get argument from the message (app pid)*/
    dbus_message_iter_get_basic(&l_args, &l_param_int);
   /* setup the arg type switch */
     l_pid_switch = TRUE;
     log_message
	("AL Daemon Reply to Method Call : Method called for %d\n",
	 l_param_int);
  }else if ((DBUS_TYPE_INT32 == dbus_message_iter_get_arg_type(&l_args))) {
   /* get argument from the message (app pid)*/
    dbus_message_iter_get_basic(&l_args, &l_param_int);
   /* setup the arg type switch */
     l_pid_switch = TRUE;
     log_message
	("AL Daemon Reply to Method Call : Method called for %d\n",
	 l_param_int);
  }else{
     log_message
	("AL Daemon Reply to Method Call : Argument is not string nor an int !%s",
	 "\n");
     }
  }
  /* create a reply from the message */
  l_reply = dbus_message_new_method_return(p_msg);
  if(l_name_switch){
	  log_message
		("AL Daemon Reply to Method Call : Reply created for %s\n",
		 l_param);
  }
  if(l_pid_switch){
	 log_message
		("AL Daemon Reply to Method Call : Reply created for %d\n",
		 l_param_int);
  }
  /* add the arguments to the reply */
  dbus_message_iter_init_append(l_reply, &l_args);
  if(l_name_switch){
	  log_message
		("AL Daemon Reply to Method Call : Args iter appended for application %s\n",
		 l_param);
  }
  if(l_pid_switch){
	 log_message
		("AL Daemon Reply to Method Call : Args iter appended for application with pid %d\n",
		 l_param_int);
  }
  if (!dbus_message_iter_append_basic(&l_args, DBUS_TYPE_BOOLEAN, &l_stat)) {
    log_message
	("AL Daemon Reply to Method Call : Arg Bool Out Of Memory!%s",
	 "\n");
    return;
  }
  if (!dbus_message_iter_append_basic(&l_args, DBUS_TYPE_UINT32, &l_level)) {
    log_message
	("AL Daemon Reply to Method Call : Arg Int Out Of Memory!%s",
	 "\n");
    return;
  }
  /* send the reply && flush the connection */
  if (!dbus_connection_send(p_conn, l_reply, &l_serial)) {
    log_message
	("AL Daemon Reply to Method Call : Connection Out Of Memory!%s",
	 "\n");
    return;
  }
  /* flush the connection */
  dbus_connection_flush(p_conn);

  /* free the reply */
  dbus_message_unref(l_reply);
  if(l_name_switch){
	  log_message
		("AL Daemon Reply to Method Call : Reply sent for application %s\n",
		 l_param);
  }
  if(l_pid_switch){
	 log_message
		("AL Daemon Reply to Method Call : Reply sent for application with pid %d\n",
		 l_param_int);
  }
}

/*
 *  Replies to Introspection request
 */
void ReplyToIntrospect(DBusMessage *p_msg, DBusConnection *p_conn)
{
    /* reply message */
    DBusMessage *l_reply;
    /* reply arguments */
    DBusMessageIter l_args;
    dbus_uint32_t l_serial = 0;
    char * l_introspect_xml = DBUS_SRM_INTROSPECT_XML;

    // create a reply from the message
    l_reply = dbus_message_new_method_return(p_msg);

    // add the arguments to the reply
    dbus_message_iter_init_append(l_reply, &l_args);
    if (!dbus_message_iter_append_basic(&l_args, DBUS_TYPE_STRING, &l_introspect_xml)) {
        printf("Reply to introspect : Arg String Out Of Memory!\n");
        exit(1);
    }
    // send the reply && flush the connection
    if (!dbus_connection_send(p_conn, l_reply, &l_serial)) {
        printf("Reply to introspect : Connection Out Of Memory!\n");
        exit(1);
    }
    dbus_connection_flush(p_conn);

    // free the reply
    dbus_message_unref(l_reply);
}


