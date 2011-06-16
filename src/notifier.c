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
  const char *l_path = NULL;
  /* store the current global state attributes */
  char *l_as_state, *l_ls_state, *l_ss_state;
  DBusMessageIter l_iter, l_sub_iter;
  /* image path and pid */
  char *l_imagePath;
  int l_pid;
  /* new method call to get unit information */
  if (!(l_msg = dbus_message_new_method_call("org.freedesktop.systemd1",
					     "/org/freedesktop/systemd1",
					     "org.freedesktop.systemd1.Manager",
					     "GetUnit"))) {
    log_message
	("AL Daemon State Extractor : Could not allocate message for %s\n",
	 p_app_name);
    l_ret = -ENOMEM;
    goto free_res;
  }
  /* append application name as argument to method call */
  if (!dbus_message_append_args(l_msg,
				DBUS_TYPE_STRING, &p_app_name,
				DBUS_TYPE_INVALID)) {
    log_message
	("AL Daemon State Extractor : Could not append arguments to message for %s\n",
	 p_app_name);
    l_ret = -ENOMEM;
    goto free_res;
  }
  /* send the message on the bus and wait for a reply */
  if (!(l_reply =
	dbus_connection_send_with_reply_and_block(p_bus, l_msg, -1,
						  &l_error))) {
    log_message
	("AL Daemon Active State Extractor : Unknown information for %s \n",
	 p_app_name);
    log_message("AL Daemon Active State Extractor : Error [%s: %s]\n",
		l_error.name, l_error.message);
    goto free_res;
  }
  /* extract arguments from the reply; the object path is useful for property fetch */
  if (!dbus_message_get_args(l_reply, &l_error,
			     DBUS_TYPE_OBJECT_PATH, &l_path,
			     DBUS_TYPE_INVALID)) {
    log_message
	("AL Daemon State Extractor : Failed to parse reply for %s\n",
	 p_app_name);
    l_ret = -EIO;
    goto free_res;
  }
  /* unrereference the message */
  dbus_message_unref(l_msg);
  /* issue a new method call to extract properties for the unit */
  if (!(l_msg = dbus_message_new_method_call("org.freedesktop.systemd1",
					     l_path,
					     "org.freedesktop.DBus.Properties",
					     "Get"))) {
    log_message
	("AL Daemon Active State Extractor : Could not allocate message for %s \n",
	 p_app_name);
    l_ret = -ENOMEM;
    goto free_res;
  }
  /* append the arguments; the active state will be needed */
  if (!dbus_message_append_args(l_msg,
				DBUS_TYPE_STRING, &interface,
				DBUS_TYPE_STRING, &l_as_property,
				DBUS_TYPE_INVALID)) {
    log_message
	("AL Daemon Active State Extractor : Could not append arguments to message for %s \n",
	 p_app_name);
    l_ret = -ENOMEM;
    goto free_res;
  }
  /* unreference the reply */
  dbus_message_unref(l_reply);
  /* send the message over the systemd bus and wait for a reply */
  if (!(l_reply =
	dbus_connection_send_with_reply_and_block(p_bus, l_msg, -1,
						  &l_error))) {
    log_message
	("AL Daemon Active State Extractor : Failed to issue method call for %s\n",
	 p_app_name);
    l_ret = -EIO;
    goto free_res;
  }
  /* parse reply and extract arguments */
  if (!dbus_message_iter_init(l_reply, &l_iter) ||
      dbus_message_iter_get_arg_type(&l_iter) != DBUS_TYPE_VARIANT) {
    log_message
	("AL Daemon Active State Extractor : Failed to parse reply for %s\n",
	 p_app_name);
    l_ret = -EIO;
    goto free_res;
  }
  /* sub iterator for reply  */
  dbus_message_iter_recurse(&l_iter, &l_sub_iter);
  /* argument type check */
  if (dbus_message_iter_get_arg_type(&l_sub_iter) != DBUS_TYPE_STRING) {
    log_message
	("AL Daemon Active State Extractor : Failed to get arg type for %s when fetching active state\n",
	 p_app_name);
    l_ret = -EIO;
    goto free_res;
  }
  /* extract the argument as a basic type */
  dbus_message_iter_get_basic(&l_sub_iter, &l_as_state);
  /* unreference the message */
  dbus_message_unref(l_msg);
  /* new method call to fetch the information about the unit */
  if (!(l_msg = dbus_message_new_method_call("org.freedesktop.systemd1",
					     "/org/freedesktop/systemd1",
					     "org.freedesktop.systemd1.Manager",
					     "GetUnit"))) {
    log_message
	("AL Daemon Load State Extractor : Could not allocate message for %s\n",
	 p_app_name);
    l_ret = -ENOMEM;
    goto free_res;
  }
  /* the arguments for the method call is the application name */
  if (!dbus_message_append_args(l_msg,
				DBUS_TYPE_STRING, &p_app_name,
				DBUS_TYPE_INVALID)) {
    log_message
	("AL Daemon Load State Extractor : Could not append arguments to message for %s\n",
	 p_app_name);
    l_ret = -ENOMEM;
    goto free_res;
  }
  /* send the message and wait for a reply */
  if (!(l_reply =
	dbus_connection_send_with_reply_and_block(p_bus, l_msg, -1,
						  &l_error))) {
    log_message
	("AL Daemon Load State Extractor : Unknown information for %s \n",
	 p_app_name);
    log_message("AL Daemon Load State Extractor : Error [%s: %s]\n",
		l_error.name, l_error.message);
    goto free_res;
  }
  /* extract the object path for the specific application */
  if (!dbus_message_get_args(l_reply, &l_error,
			     DBUS_TYPE_OBJECT_PATH, &l_path,
			     DBUS_TYPE_INVALID)) {
    log_message
	("AL Daemon Load State Extractor : Failed to parse reply for %s\n",
	 p_app_name);
    l_ret = -EIO;
    goto free_res;
  }
  /* new method call to fetch the application's properties */
  if (!(l_msg = dbus_message_new_method_call("org.freedesktop.systemd1",
					     l_path,
					     "org.freedesktop.DBus.Properties",
					     "Get"))) {
    log_message
	("AL Daemon Load State Extractor : Could not allocate message for %s \n",
	 p_app_name);
    l_ret = -ENOMEM;
    goto free_res;
  }
  /* append arguments to request load state information */
  if (!dbus_message_append_args(l_msg,
				DBUS_TYPE_STRING, &interface,
				DBUS_TYPE_STRING, &l_ls_property,
				DBUS_TYPE_INVALID)) {
    log_message
	("AL Daemon Load State Extractor : Could not append arguments to message for %s \n",
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
    log_message
	("AL Daemon Load State Extractor : Failed to issue method call for %s\n",
	 p_app_name);
    l_ret = -EIO;
    goto free_res;
  }
  /* initialize iterator and verify argument type */
  if (!dbus_message_iter_init(l_reply, &l_iter) ||
      dbus_message_iter_get_arg_type(&l_iter) != DBUS_TYPE_VARIANT) {
    log_message
	("AL Daemon Load State Extractor : Failed to parse reply for %s\n",
	 p_app_name);
    l_ret = -EIO;
    goto free_res;
  }
  /* recursive iterator for arguments */
  dbus_message_iter_recurse(&l_iter, &l_sub_iter);
  /* extracted argument type verification */
  if (dbus_message_iter_get_arg_type(&l_sub_iter) != DBUS_TYPE_STRING) {
    log_message
	("AL Daemon Load State Extractor : Failed to get arg type for %s when fetching load state\n",
	 p_app_name);
    l_ret = -EIO;
    goto free_res;
  }
  /* get the argument and store it */
  dbus_message_iter_get_basic(&l_sub_iter, &l_ls_state);
  /* unreference message */
  dbus_message_unref(l_msg);
  /* method call to fetch application information */
  if (!(l_msg = dbus_message_new_method_call("org.freedesktop.systemd1",
					     "/org/freedesktop/systemd1",
					     "org.freedesktop.systemd1.Manager",
					     "GetUnit"))) {
    log_message
	("AL Daemon Sub State Extractor : Could not allocate message for %s\n",
	 p_app_name);
    l_ret = -ENOMEM;
    goto free_res;
  }
  /* append the application name as argument */
  if (!dbus_message_append_args(l_msg,
				DBUS_TYPE_STRING, &p_app_name,
				DBUS_TYPE_INVALID)) {
    log_message
	("AL Daemon Sub State Extractor : Could not append arguments to message for %s\n",
	 p_app_name);
    l_ret = -ENOMEM;
    goto free_res;
  }
  /* sends the message on the bus and wait for a reply */
  if (!(l_reply =
	dbus_connection_send_with_reply_and_block(p_bus, l_msg, -1,
						  &l_error))) {
    log_message
	("AL Daemon Sub State Extractor : Unknown information for %s \n",
	 p_app_name);
    log_message("AL Daemon Sub State Extractor : Error [%s: %s]\n",
		l_error.name, l_error.message);
    goto free_res;
  }
  /* extract object path for application */
  if (!dbus_message_get_args(l_reply, &l_error,
			     DBUS_TYPE_OBJECT_PATH, &l_path,
			     DBUS_TYPE_INVALID)) {
    log_message
	("AL Daemon Sub State Extractor : Failed to parse reply for %s\n",
	 p_app_name);
    l_ret = -EIO;
    goto free_res;
  }
  /* issues a new method call to fetch application's properties */
  if (!(l_msg = dbus_message_new_method_call("org.freedesktop.systemd1",
					     l_path,
					     "org.freedesktop.DBus.Properties",
					     "Get"))) {
    log_message
	("AL Daemon Sub State Extractor : Could not allocate message for %s \n",
	 p_app_name);
    l_ret = -ENOMEM;
    goto free_res;
  }
  /* append arguments */
  if (!dbus_message_append_args(l_msg,
				DBUS_TYPE_STRING, &interface,
				DBUS_TYPE_STRING, &l_ss_property,
				DBUS_TYPE_INVALID)) {
    log_message
	("AL Daemon Sub State Extractor : Could not append arguments to message for %s \n",
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
    log_message
	("AL Daemon Sub State Extractor : Failed to issue method call for %s\n",
	 p_app_name);
    l_ret = -EIO;
    goto free_res;
  }
  /* create iterator for reply parsing and verify argument type */
  if (!dbus_message_iter_init(l_reply, &l_iter) ||
      dbus_message_iter_get_arg_type(&l_iter) != DBUS_TYPE_VARIANT) {
    log_message
	("AL Daemon Sub State Extractor : Failed to parse reply for %s\n",
	 p_app_name);
    l_ret = -EIO;
    goto free_res;
  }
  /* recurse sub-iterator for reply parsing */
  dbus_message_iter_recurse(&l_iter, &l_sub_iter);
  /* check extracted argument type validity */
  if (dbus_message_iter_get_arg_type(&l_sub_iter) != DBUS_TYPE_STRING) {
    log_message
	("AL Daemon Sub State Extractor : Failed to get arg type for %s when fetching sub state\n",
	 p_app_name);
    l_ret = -EIO;
    goto free_res;
  }
  /* extract the sub state information for the application */
  dbus_message_iter_get_basic(&l_sub_iter, &l_ss_state);
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

  log_message
      ("AL Daemon State Extractor : State information for %s is given by next string [ %s ] \n",
       p_app_name, l_state);
  /* copy into return variable */
  strcpy(p_state_info, l_state);

  log_message
      ("AL Daemon State Extractor : State information for %s was extracted ! Returning to notifier function !\n",
       p_app_name);
  l_ret = 0;
  /* free allocated resources */
  dbus_message_unref(l_msg);
  dbus_message_unref(l_reply);
  l_msg = l_reply = NULL;

free_res:
  /* free the messages */
  if (l_msg)
    dbus_message_unref(l_msg);

  if (l_reply)
    dbus_message_unref(l_reply);
  /* free the error */
  dbus_error_free(&l_error);

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

  log_message
      ("AL Daemon Send Notification : Sending signal with value %s\n",
       p_app_name);

  /* extract the information to broadcast */
  log_message
      ("AL Daemon Send Notification : Getting application state for %s \n",
       p_app_name);


  /* extract the application state  */
  if (AlGetAppState(p_conn, p_app_name, l_state_info) == 0) {

    log_message
	("AL Daemon Send Notification : Received application state for %s \n",
	 p_app_name);

    /* copy the state */
    l_app_status = strdup(l_state_info);

    log_message
	(" AL Daemon Send Notification : Global State information for %s -> [ %s ] \n",
	 p_app_name, l_app_status);

    /* create a signal for global state notification & check for errors */
    l_msg = dbus_message_new_signal(SRM_OBJECT_PATH,
				    AL_SIGNAL_INTERFACE,
				    "GlobalStateNotification");

    /* check for message state */
    if (NULL == l_msg) {
      log_message("AL Daemon Send Notification for %s : Message Null\n",
		  p_app_name);
      return;
    }
    log_message
	("AL Daemon Send Notification : Appending state arguments for %s\n",
	 p_app_name);

    /* append arguments onto the end of message (signal) */
    dbus_message_iter_init_append(l_msg, &l_args);
    log_message
	("AL Daemon Send Notification : Initialized iterator for appending message arguments at end for %s\n",
	 p_app_name);
    /* append status information encoded in a string */
    if (!dbus_message_iter_append_basic
	(&l_args, DBUS_TYPE_STRING, &l_app_status)) {

      log_message
	  ("AL Daemon Method Caller : Could not append args for status extraction message for %s!\n",
	   p_app_name);
    }
    log_message
	("AL Daemon Send Notification : Sending the state message for %s\n",
	 p_app_name);

    /* send the message and flush the connection */
    if (!dbus_connection_send(p_conn, l_msg, &l_serial)) {
      log_message
	  ("AL Daemon Send Notification for %s: Connection Out Of Memory!\n",
	   p_app_name);
    }

    log_message("AL Daemon Send Notification for %s: Signal Sent\n",
		p_app_name);
  }
  /* free the message */
  dbus_message_unref(l_msg);
}

/* 
 * Function responsible to issue a notification after a task chnages state (fg/bg) 
 */
void AlChangeTaskStateNotifier(DBusConnection *p_conn, char *p_app_name, char *p_app_state)
{

  /* message to be sent */
  DBusMessage *l_msg;
  /* message arguments */
  DBusMessageIter l_args;
  /* return code */
  int l_ret;
  /* reply information */
  dbus_uint32_t l_serial = 0;

  log_message
      ("AL Daemon Send ChangeTaskStateComplete Notification : Sending signal for application %s with state %s \n", p_app_name, p_app_state);

    /* create a signal for fg/bg state notification & check for errors */
    l_msg = dbus_message_new_signal(SRM_OBJECT_PATH,
				    AL_SIGNAL_INTERFACE,
				    "ChangeTaskStateComplete");

    /* check errors on message to be sent */
    if (NULL == l_msg) {
      log_message("AL Daemon Send ChangeTaskStateComplete Notification for %s : Message Null\n",
		  p_app_name);
      return;
    }
    log_message
	("AL Daemon Send ChangeTaskStateComplete Notification : Appending state arguments for %s\n",
	 p_app_name);

    /* append arguments to the message (signal) */
    dbus_message_iter_init_append(l_msg, &l_args);
    log_message
	("AL Daemon Send ChangeTaskStateComplete Notification : Initialized iterator for appending message arguments at end for %s\n",
	 p_app_name);
    /* append status (fg/bg) information encoded in a string */
    if (!dbus_message_iter_append_basic
	(&l_args, DBUS_TYPE_STRING, &p_app_name)) {
      log_message
	  ("AL Daemon Send ChangeTaskStateComplete Notification : Could not append app name for %s!\n", p_app_name);
    }
    if (!dbus_message_iter_append_basic
	(&l_args, DBUS_TYPE_STRING, &p_app_state)) {

      log_message
	  ("AL Daemon Send ChangeTaskStateComplete Notification : Could not append app state for %s!\n", p_app_name);
    }
    log_message
	("AL Daemon Send ChangeTaskStateComplete Notification : Sending the state message for %s\n",
	 p_app_name);

    /* send the message and flush the connection */
    if (!dbus_connection_send(p_conn, l_msg, &l_serial)) {
      log_message
	  ("AL Daemon Send ChangeTaskStateComplete Notification for %s: Connection Out Of Memory!\n",
	   p_app_name);
    }

    log_message("AL Daemon Send ChangeTaskStateComplete Notification for %s: Signal Sent\n",
		p_app_name);

  /* free the message */
  dbus_message_unref(l_msg);
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
  const char *l_path = NULL;
  /* message iterators for pid extraction */
  DBusMessageIter l_iter, l_sub_iter;

  /* interface and method to acces application pid */
  const char *l_interface =
      "org.freedesktop.systemd1.Service", *l_pid_property = "ExecMainPID";

  log_message
      ("AL Daemon Send Active State Notification : Sending signal for  %s\n",
       p_app_name);

  /* initialise the error value */
  dbus_error_init(&l_err);

  /* extract the information to broadcast */
  log_message
      ("AL Daemon Send Active State Notification : Getting application state for %s \n",
       p_app_name);

  /* extract the PID for the application that will be stopped because it won't be available in /proc anymore */

  if (!(l_msg = dbus_message_new_method_call("org.freedesktop.systemd1",
					     "/org/freedesktop/systemd1",
					     "org.freedesktop.systemd1.Manager",
					     "GetUnit"))) {
    log_message
	("AL Daemon Send Active State Notification : Could not allocate message for %s\n",
	 p_app_name);
    l_ret = -ENOMEM;
    goto free_res;
  }
  /* append application name as argument to method call */
  if (!dbus_message_append_args(l_msg,
				DBUS_TYPE_STRING, &p_app_name,
				DBUS_TYPE_INVALID)) {
    log_message
	("AL Daemon Send Active State Notification : Could not append arguments to message for %s\n",
	 p_app_name);
    l_ret = -ENOMEM;
    goto free_res;
  }
  /* send the message on the bus and wait for a reply */
  if (!(l_reply =
	dbus_connection_send_with_reply_and_block(p_conn, l_msg, -1,
						  &l_err))) {
    log_message
	("AL Daemon Send Active State Notification : Unknown information for %s \n",
	 p_app_name);
    log_message
	("AL Daemon Send Active State Notification : Error [%s: %s]\n",
	 l_err.name, l_err.message);
    goto free_res;
  }
  /* extract arguments from the reply; the object path is useful for property fetch */
  if (!dbus_message_get_args(l_reply, &l_err,
			     DBUS_TYPE_OBJECT_PATH, &l_path,
			     DBUS_TYPE_INVALID)) {
    log_message
	("AL Daemon Send Active State Notification : Failed to parse reply for %s\n",
	 p_app_name);
    l_ret = -EIO;
    goto free_res;
  }
  /* unrereference the message */
  dbus_message_unref(l_msg);
  /* issue a new method call to extract properties for the unit */
  if (!(l_msg = dbus_message_new_method_call("org.freedesktop.systemd1",
					     l_path,
					     "org.freedesktop.DBus.Properties",
					     "Get"))) {
    log_message
	("AL Daemon Send Active State Notification : Could not allocate message for %s \n",
	 p_app_name);
    l_ret = -ENOMEM;
    goto free_res;
  }
  /* append the arguments; the main pid will be needed */
  if (!dbus_message_append_args(l_msg,
				DBUS_TYPE_STRING, &l_interface,
				DBUS_TYPE_STRING, &l_pid_property,
				DBUS_TYPE_INVALID)) {
    log_message
	("AL Daemon Send Active State Notification : Could not append arguments to message for %s \n",
	 p_app_name);
    l_ret = -ENOMEM;
    goto free_res;
  }
  /* unreference the reply */
  dbus_message_unref(l_reply);
  /* send the message over the systemd bus and wait for a reply */
  if (!(l_reply =
	dbus_connection_send_with_reply_and_block(p_conn, l_msg, -1,
						  &l_err))) {
    log_message
	("AL Daemon Send Active State Notification : Failed to issue method call for %s\n",
	 p_app_name);
    l_ret = -EIO;
    goto free_res;
  }
  /* parse reply and extract arguments */
  if (!dbus_message_iter_init(l_reply, &l_iter) ||
      dbus_message_iter_get_arg_type(&l_iter) != DBUS_TYPE_VARIANT) {
    log_message
	("AL Daemon Send Active State Notification : Failed to parse reply for %s\n",
	 p_app_name);
    l_ret = -EIO;
    goto free_res;
  }
  /* sub iterator for reply  */
  dbus_message_iter_recurse(&l_iter, &l_sub_iter);
  /* argument type check */
  if (dbus_message_iter_get_arg_type(&l_sub_iter) != DBUS_TYPE_UINT32) {
    log_message
	("AL Daemon Send Active State Notification : Failed to get arg type for %s when fetching pid!\n",
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

    log_message
	("AL Daemon Send Active State Notification : Received application state for %s \n",
	 p_app_name);

    /* copy the state */
    l_app_status = strdup(l_state_info);

    log_message
	(" AL Daemon Send Active State Notification : Global State information for %s -> [ %s ] \n",
	 p_app_name, l_app_status);

    log_message
	(" AL Daemon Send Active State Notification : Application state is %s \n",
	 l_app_status);

    /* service name extraction from global state info */
    l_service_name = strtok(l_app_status, l_delim_serv);
    /* active state extraaction from global state info */
    l_active_state = strtok(NULL, l_delim_serv);
    l_active_state = strtok(NULL, l_delim_serv);
    l_app_name = strtok(l_service_name, l_delim_app);
    /* form the application status message to send */
    strcpy(l_state_msg, l_app_name);
    strcat(l_state_msg, " ");
    sprintf(l_pid_string, "%d", l_pid);
    strcat(l_state_msg, l_pid_string);
    strcat(l_state_msg, " ");

    /* test if application was started and signal this event */
    if (strcmp(l_active_state, "active") == 0) {

      /* append to dbus signal the application status */
      strcat(l_state_msg, AL_SIGNAME_TASK_STARTED);
      TaskStarted(l_app_name, l_pid);
      /* create a signal & check for errors */
      l_msg = dbus_message_new_signal(SRM_OBJECT_PATH,
				      AL_SIGNAL_INTERFACE,
				      AL_SIGNAME_TASK_STARTED);
    }

    /* test if application was stopped and became inactive and signal this event */
    if (strcmp(l_active_state, "inactive") == 0) {
      /* append to dbus signal the application status */
      strcat(l_state_msg, AL_SIGNAME_TASK_STOPPED);
      TaskStopped(l_app_name, l_pid);
      /* create a signal & check for errors */
      l_msg = dbus_message_new_signal(SRM_OBJECT_PATH,
				      AL_SIGNAL_INTERFACE,
				      AL_SIGNAME_TASK_STOPPED);
    }

    /* test if application failed and stopped and signal this event */
    if (strcmp(l_active_state, "failed") == 0) {
      /* append to dbus signal the application status */
      strcat(l_state_msg, AL_SIGNAME_TASK_STOPPED);
      TaskStopped(l_app_name, l_pid);
      /* create a signal & check for errors */
      l_msg = dbus_message_new_signal(SRM_OBJECT_PATH,
				      AL_SIGNAL_INTERFACE,
				      AL_SIGNAME_TASK_STOPPED);
    }

    /* test if application is in a transitional state to activation */
    if (strcmp(l_active_state, "activating") == 0) {
      /* the task is in a transitional state to active */
      log_message
	  ("AL Daemon Send Active State Notification : The application %s is in %s state and will be activated !",
	   l_app_name, l_active_state);
      log_message
	  ("AL Daemon Send Active State Notification : The new state for %s will be fetched after entering a stable state!",
	   l_app_name);
      return;
    }

    /* test if application is in a transitional state to deactivation */
    if (strcmp(l_active_state, "deactivating") == 0) {
      /* the task is in a transitional state to active */
      log_message
	  ("AL Daemon Send Active State Notification : The application %s is in %s state and will be deactivated !",
	   l_app_name, l_active_state);
      log_message
	  ("AL Daemon Send Active State Notification : The new state for %s will be fetched after entering a stable state!",
	   l_app_name);
      return;
    }

    /* test if application is in a transitional state to reload */
    if (strcmp(l_active_state, "reloading") == 0) {
      /* the task is in a transitional state to active */
      log_message
	  ("AL Daemon Send Active State Notification : The application %s is in %s state and will be reloaded !",
	   l_app_name, l_active_state);
      log_message
	  ("AL Daemon Send Active State Notification : The new state for %s will be fetched after entering a stable state!",
	   l_app_name);
      return;
    }

    /* check for message state */
    if (NULL == l_msg) {
      log_message
	  ("AL Daemon Send Active State Notification : for %s : Message Null\n",
	   p_app_name);

      return;
    }
    log_message
	("AL Daemon Send Active State Notification: Appending state arguments for %s\n",
	 p_app_name);

    /* append arguments onto the end of message (signal) */
    dbus_message_iter_init_append(l_msg, &l_args);
    log_message
	("AL Daemon Send Active State Notification : Initialized iterator for appending message arguments at end for %s\n",
	 p_app_name);

    /* append status information encoded in a string */
    if (!dbus_message_iter_append_basic
	(&l_args, DBUS_TYPE_STRING, &l_state_msg)) {

      log_message
	  ("AL Daemon Send Active State Notification : Could not append image path for %s!\n",
	   p_app_name);
    }

    /* send the message and flush the connection */
    if (!dbus_connection_send(p_conn, l_msg, &l_serial)) {
      log_message
	  ("AL Daemon Send Active State Notification : for %s: Connection Out Of Memory!\n",
	   p_app_name);
    }

    log_message
	("AL Daemon Send Active State Notification for %s: Signal Sent\n",
	 p_app_name);
  }

free_res:
  /* free the messages */
  if (l_msg)
    dbus_message_unref(l_msg);

  if (l_reply)
    dbus_message_unref(l_reply);
  /* free the error */
  dbus_error_free(&l_err);

}

