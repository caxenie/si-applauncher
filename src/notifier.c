/* 
* notifier.c, contains the implementation of the daemon notification system 
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

#include "al-daemon.h"
#include "notifier.h"
#include "dbus_interface.h"

extern ALDbus *g_al_dbus;

/* 
 * Function responsible to extract the status of an application after starting it or that is already running in the system. 
 * This refers to extracting : Load State, Active State and Sub State.
 */

int AlGetAppState(DBusConnection * p_bus, char *p_app_name,
		  char *p_state_info)
{
  /* initialize message and reply */
  DBusMessage *l_msg = NULL, *l_reply = NULL;
  /* define interface to use and properties to fetch */
  const char
  *interface = "org.freedesktop.systemd1.Unit",
      *l_as_property = "ActiveState",
      *l_ls_property = "LoadState", *l_ss_property = "SubState";
  /* store the state */
  char *l_state;
  /* return code */
  int l_ret = 0;
  /* error definition and initialization */
  DBusError l_error;
  dbus_error_init(&l_error);
  /* initialize the path */
  char *l_path = NULL;
  /* store the current global state attributes */
  char *l_as_state = NULL, *l_ls_state = NULL, *l_ss_state = NULL;
  DBusMessageIter l_iter, l_sub_iter;
  /* image path and pid */
  char *l_imagePath;
  int l_pid;
  /* get unit object path */
  if (NULL == (l_path = GetUnitObjectPath(p_bus, p_app_name)))
  {
          log_error_message
                  ("Active State Extractor : Unable to extract object path for %s", p_app_name);
          l_ret = -1;
          goto free_res;
  }
  log_debug_message
          ("Active State Extractor : Extracted object path for %s\n",
           p_app_name);
  /* issue a new method call to extract properties for the unit */
  if (!(l_msg = dbus_message_new_method_call("org.freedesktop.systemd1",
					     l_path,
					     "org.freedesktop.DBus.Properties",
					     "Get"))) {
    log_error_message
	("Active State Extractor : Could not allocate message for %s \n",
	 p_app_name);
    l_ret = -ENOMEM;
    goto free_res;
  }
  /* append the arguments; the active state will be needed */
  if (!dbus_message_append_args(l_msg,
				DBUS_TYPE_STRING, &interface,
				DBUS_TYPE_STRING, &l_as_property,
				DBUS_TYPE_INVALID)) {
    log_error_message
	("Active State Extractor : Could not append arguments to message for %s \n",
	 p_app_name);
    l_ret = -ENOMEM;
    goto free_res;
  }
  /* send the message over the systemd bus and wait for a reply */
  if (!(l_reply =
	dbus_connection_send_with_reply_and_block(p_bus, l_msg, -1,
						  &l_error))) {
    log_error_message
	("Active State Extractor : Failed to issue method call for %s\n",
	 p_app_name);
    l_ret = -EIO;
    goto free_res;
  }
  /* parse reply and extract arguments */
  if (!dbus_message_iter_init(l_reply, &l_iter) ||
      dbus_message_iter_get_arg_type(&l_iter) != DBUS_TYPE_VARIANT) {
    log_error_message
	("Active State Extractor : Failed to parse reply for %s\n",
	 p_app_name);
    l_ret = -EIO;
    goto free_res;
  }
  /* sub iterator for reply  */
  dbus_message_iter_recurse(&l_iter, &l_sub_iter);
  /* argument type check */
  if (dbus_message_iter_get_arg_type(&l_sub_iter) != DBUS_TYPE_STRING) {
    log_error_message
	("Active State Extractor : Failed to get arg type for %s when fetching active state\n",
	 p_app_name);
    l_ret = -EIO;
    goto free_res;
  }
  /* extract the argument as a basic type */
  dbus_message_iter_get_basic(&l_sub_iter, &l_as_state);
  /* allow reply unreference */
  l_as_state = strdup(l_as_state);
  /* unreference the message */
  dbus_message_unref(l_msg);
  /* new method call to fetch the application's properties */
  if (!(l_msg = dbus_message_new_method_call("org.freedesktop.systemd1",
					     l_path,
					     "org.freedesktop.DBus.Properties",
					     "Get"))) {
    log_error_message
	("Load State Extractor : Could not allocate message for %s \n",
	 p_app_name);
    l_ret = -ENOMEM;
    goto free_res;
  }
  /* append arguments to request load state information */
  if (!dbus_message_append_args(l_msg,
				DBUS_TYPE_STRING, &interface,
				DBUS_TYPE_STRING, &l_ls_property,
				DBUS_TYPE_INVALID)) {
    log_error_message
	("Load State Extractor : Could not append arguments to message for %s \n",
	 p_app_name);
    l_ret = -ENOMEM;
    goto free_res;
  }
  /* unreference reply */
  dbus_message_unref(l_reply);
  /* sends the message and waits for a reply */
  if (!(l_reply =
	dbus_connection_send_with_reply_and_block(p_bus, l_msg, -1,
						  &l_error))) {
    log_error_message
	("Load State Extractor : Failed to issue method call for %s\n",
	 p_app_name);
    l_ret = -EIO;
    goto free_res;
  }
  /* initialize iterator and verify argument type */
  if (!dbus_message_iter_init(l_reply, &l_iter) ||
      dbus_message_iter_get_arg_type(&l_iter) != DBUS_TYPE_VARIANT) {
    log_error_message
	("Load State Extractor : Failed to parse reply for %s\n",
	 p_app_name);
    l_ret = -EIO;
    goto free_res;
  }
  /* recursive iterator for arguments */
  dbus_message_iter_recurse(&l_iter, &l_sub_iter);
  /* extracted argument type verification */
  if (dbus_message_iter_get_arg_type(&l_sub_iter) != DBUS_TYPE_STRING) {
    log_error_message
	("Load State Extractor : Failed to get arg type for %s when fetching load state\n",
	 p_app_name);
    l_ret = -EIO;
    goto free_res;
  }
  /* get the argument and store it */
  dbus_message_iter_get_basic(&l_sub_iter, &l_ls_state);
  /* allow reply unreference */
  l_ls_state = strdup(l_ls_state);
  /* unreference message */
  dbus_message_unref(l_msg);
  /* issues a new method call to fetch application's properties */
  if (!(l_msg = dbus_message_new_method_call("org.freedesktop.systemd1",
					     l_path,
					     "org.freedesktop.DBus.Properties",
					     "Get"))) {
    log_error_message
	("Sub State Extractor : Could not allocate message for %s \n",
	 p_app_name);
    l_ret = -ENOMEM;
    goto free_res;
  }
  /* append arguments */
  if (!dbus_message_append_args(l_msg,
				DBUS_TYPE_STRING, &interface,
				DBUS_TYPE_STRING, &l_ss_property,
				DBUS_TYPE_INVALID)) {
    log_error_message
	("Sub State Extractor : Could not append arguments to message for %s \n",
	 p_app_name);
    l_ret = -ENOMEM;
    goto free_res;
  }
  /* unreference the reply */
  dbus_message_unref(l_reply);
  /* send the message and wait for a reply */
  if (!(l_reply =
	dbus_connection_send_with_reply_and_block(p_bus, l_msg, -1,
						  &l_error))) {
    log_error_message
	("Sub State Extractor : Failed to issue method call for %s\n",
	 p_app_name);
    l_ret = -EIO;
    goto free_res;
  }
  /* create iterator for reply parsing and verify argument type */
  if (!dbus_message_iter_init(l_reply, &l_iter) ||
      dbus_message_iter_get_arg_type(&l_iter) != DBUS_TYPE_VARIANT) {
    log_error_message
	("Sub State Extractor : Failed to parse reply for %s\n",
	 p_app_name);
    l_ret = -EIO;
    goto free_res;
  }
  /* recurse sub-iterator for reply parsing */
  dbus_message_iter_recurse(&l_iter, &l_sub_iter);
  /* check extracted argument type validity */
  if (dbus_message_iter_get_arg_type(&l_sub_iter) != DBUS_TYPE_STRING) {
    log_error_message
	("Sub State Extractor : Failed to get arg type for %s when fetching sub state\n",
	 p_app_name);
    l_ret = -EIO;
    goto free_res;
  }
  /* extract the sub state information for the application */
  dbus_message_iter_get_basic(&l_sub_iter, &l_ss_state);
  /* allow reply unreference */
  l_ss_state = strdup(l_ss_state);
  /* allocate the global state information string */
  l_state = malloc(DIM_MAX * sizeof(l_state));
  /* form the global state string */
  strcpy(l_state, p_app_name);
  strcat(l_state, " ");
  strcat(l_state, l_ls_state);
  strcat(l_state, " ");
  strcat(l_state, l_as_state);
  strcat(l_state, " ");
  strcat(l_state, l_ss_state);

  log_debug_message
      ("State Extractor : State information for %s is given by next string [ %s ] \n",
       p_app_name, l_state);
  /* copy into return variable */
  strcpy(p_state_info, l_state);

  log_debug_message
      ("State Extractor : State information for %s was extracted ! Returning to notifier function !\n",
       p_app_name);

free_res:
  /* free the messages */
  if (l_msg)
    dbus_message_unref(l_msg);

  if (l_reply)
    dbus_message_unref(l_reply);
  /* free the error */
  dbus_error_free(&l_error);

  if (l_ls_state)
          free(l_ls_state);
  if (l_as_state)
          free(l_as_state);
  if (l_ss_state)
          free(l_ss_state);
  if (l_path)
          free(l_path);

  return l_ret;
}

/* 
 * Function responsible to broadcast the state of an application that started execution
 * or an application already running in the system. 
 */

void AlAppStateNotifier(DBusConnection *p_conn, char *p_app_name)
{

  /* message to be sent */
  DBusMessage *l_msg;
  /* message arguments */
  DBusMessageIter l_args;
  /* return code */
  int l_ret;
  /* reply information */
  dbus_uint32_t l_serial = 0;
  /* global state info */
  char l_state_info[DIM_MAX];
  char *l_app_status;

  log_debug_message
      ("Send Notification : Sending signal with value %s\n",
       p_app_name);

  /* extract the information to broadcast */
  log_debug_message
      ("Send Notification : Getting application state for %s \n",
       p_app_name);


  /* extract the application state  */
  if (AlGetAppState(p_conn, p_app_name, l_state_info) == 0) {
    /* notify clients about tasks global state changes */
    al_dbus_global_state_notification(g_al_dbus, l_state_info);
  }
}

/* Connect to the DBUS bus and send a broadcast signal about the state of the application */
void AlSendAppSignal(DBusConnection * p_conn, char *p_app_name)
{
  /* message to be sent */
  DBusMessage *l_msg, *l_reply;
  /* message arguments */
  DBusMessageIter l_args;
  /* error */
  DBusError l_err;
  /* return code */
  int l_ret;
  /* reply information */
  dbus_uint32_t l_serial = 0;
  /* global state info */
  char l_state_info[DIM_MAX];
  char *l_app_status;
  /* application start/stop auxiliary vars : active state, sub state, app name, 
     global state, service name, message to send */
  char *l_active_state = malloc(DIM_MAX * sizeof(l_active_state));
  char *l_sub_state = malloc(DIM_MAX * sizeof(l_sub_state));
  char *l_app_name = malloc(DIM_MAX * sizeof(l_app_name));
  char *l_service_name = malloc(DIM_MAX * sizeof(l_service_name));
  char *l_state_msg = malloc(DIM_MAX * sizeof(l_state_msg));
  /* delimiters for service / application name extraction */
  char l_delim_serv[] = " ";
  char l_delim_app[] = ".";
  /* application pid */
  int l_pid;
  char l_pid_string[DIM_MAX];
  /* initialize the path */
  char *l_path = NULL;
  /* message iterators for pid extraction */
  DBusMessageIter l_iter, l_sub_iter;

  /* interface and method to acces application pid */
  const char *l_interface =
      "org.freedesktop.systemd1.Service", *l_pid_property = "ExecMainPID";

  log_debug_message
      ("Send Active State Notification : Sending signal for  %s\n",
       p_app_name);

  /* initialise the error value */
  dbus_error_init(&l_err);

  /* extract the information to broadcast */
  log_debug_message
      ("Send Active State Notification : Getting application state for %s \n",
       p_app_name);

  /* extract the PID for the application that will be stopped because it won't be available in /proc anymore */

  /* get unit object path */
  if (NULL == (l_path = GetUnitObjectPath(p_conn, p_app_name)))
  {
          log_error_message
                  ("Send Active State Notification : Unable to extract object path for %s", p_app_name);
          l_ret = -1;
          goto free_res;
  }
  log_debug_message
          ("Send Active State Notification : Extracted object path for %s\n",
           p_app_name);

  /* issue a new method call to extract properties for the unit */
  if (!(l_msg = dbus_message_new_method_call("org.freedesktop.systemd1",
					     l_path,
					     "org.freedesktop.DBus.Properties",
					     "Get"))) {
    log_error_message
	("Send Active State Notification : Could not allocate message for %s \n",
	 p_app_name);
    l_ret = -ENOMEM;
    goto free_res;
  }
  /* append the arguments; the main pid will be needed */
  if (!dbus_message_append_args(l_msg,
				DBUS_TYPE_STRING, &l_interface,
				DBUS_TYPE_STRING, &l_pid_property,
				DBUS_TYPE_INVALID)) {
    log_error_message
	("Send Active State Notification : Could not append arguments to message for %s \n",
	 p_app_name);
    l_ret = -ENOMEM;
    goto free_res;
  }
  /* send the message over the systemd bus and wait for a reply */
  if (!(l_reply =
	dbus_connection_send_with_reply_and_block(p_conn, l_msg, -1,
						  &l_err))) {
    log_error_message
	("Send Active State Notification : Failed to issue method call for %s\n",
	 p_app_name);
    l_ret = -EIO;
    goto free_res;
  }
  /* parse reply and extract arguments */
  if (!dbus_message_iter_init(l_reply, &l_iter) ||
      dbus_message_iter_get_arg_type(&l_iter) != DBUS_TYPE_VARIANT) {
    log_error_message
	("Send Active State Notification : Failed to parse reply for %s\n",
	 p_app_name);
    l_ret = -EIO;
    goto free_res;
  }
  /* sub iterator for reply  */
  dbus_message_iter_recurse(&l_iter, &l_sub_iter);
  /* argument type check */
  if (dbus_message_iter_get_arg_type(&l_sub_iter) != DBUS_TYPE_UINT32) {
    log_error_message
	("Send Active State Notification : Failed to get arg type for %s when fetching pid!\n",
	 p_app_name);
    l_ret = -EIO;
    goto free_res;
  }
  /* extract the argument as a basic type */
  dbus_message_iter_get_basic(&l_sub_iter, &l_pid);

  /* unreference the message */
  dbus_message_unref(l_msg);

  /* extract the application state  */
  if (AlGetAppState(p_conn, p_app_name, l_state_info) == 0) {

    log_debug_message
	("Send Active State Notification : Received application state for %s \n",
	 p_app_name);

    /* copy the state */
    l_app_status = strdup(l_state_info);

    log_debug_message
	("Send Active State Notification : Global State information for %s -> [ %s ] \n",
	 p_app_name, l_app_status);

    log_debug_message
	("Send Active State Notification : Application state is %s \n",
	 l_app_status);

    /* service name extraction from global state info */
    l_service_name = strtok(l_app_status, l_delim_serv);
    /* active state extraaction from global state info */
    l_active_state = strtok(NULL, l_delim_serv);
    l_active_state = strtok(NULL, l_delim_serv);
    l_app_name = strtok(l_service_name, l_delim_app);

    /* test if application was started and signal this event */
    if (strcmp(l_active_state, "active") == 0) {
      /* emit signal */
      al_dbus_task_started(g_al_dbus, l_pid, l_app_name);
    }

    /* test if application was stopped and became inactive and signal this event */
    if (strcmp(l_active_state, "inactive") == 0) {
      /* emit signal */
      al_dbus_task_stopped(g_al_dbus, l_pid, l_app_name);
    }

    /* test if application failed and stopped and signal this event */
    if (strcmp(l_active_state, "failed") == 0) {
       /* emit signal */
      al_dbus_task_stopped(g_al_dbus, l_pid, l_app_name);
    }

    /* test if application is in a transitional state to activation */
    if (strcmp(l_active_state, "activating") == 0) {
      /* the task is in a transitional state to active */
      log_debug_message
	  ("Send Active State Notification : The application %s is in %s state and will be activated !",
	   l_app_name, l_active_state);
      log_debug_message
	  ("Send Active State Notification : The new state for %s will be fetched after entering a stable state!",
	   l_app_name);
      return;
    }

    /* test if application is in a transitional state to deactivation */
    if (strcmp(l_active_state, "deactivating") == 0) {
      /* the task is in a transitional state to active */
      log_debug_message
	  ("Send Active State Notification : The application %s is in %s state and will be deactivated !",
	   l_app_name, l_active_state);
      log_debug_message
	  ("Send Active State Notification : The new state for %s will be fetched after entering a stable state!",
	   l_app_name);
       return;
    }

    /* test if application is in a transitional state to reload */
    if (strcmp(l_active_state, "reloading") == 0) {
      /* the task is in a transitional state to active */
      log_debug_message
	  ("Send Active State Notification : The application %s is in %s state and will be reloaded !",
	   l_app_name, l_active_state);
      log_debug_message
	  ("Send Active State Notification : The new state for %s will be fetched after entering a stable state!",
	   l_app_name);
	return;
    }
  }
 
  return;

free_res:
  /* resources free */
  if(l_active_state) free(l_active_state);
  if(l_sub_state) free(l_sub_state);
  if(l_app_name) free(l_app_name);
  if(l_service_name) free(l_service_name);
  if(l_state_msg) free(l_state_msg);
  /* free the error */
  dbus_error_free(&l_err);
  /* free unit object path string */
  if (NULL != l_path)
          free(l_path);
  return;
}

