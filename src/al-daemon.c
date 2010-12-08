/* 
* al-daemon.c, contains the implementation of the Dbus handler functions
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

/* Application Launcher Daemon */
#include <ctype.h>
#include <dbus/dbus.h>
#include <dirent.h>
#include <errno.h>
#include <glib.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/user.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <unistd.h>
#include "al-daemon.h"

/* Function to extract PID value using the name of an application */
pid_t AppPidFromName(char *p_app_name)
{
  /* define the directory to scan */
  DIR *l_dir;
  /* pointer to the next directory entry */
  struct dirent *l_next;
  /* buffer size to save the name */
  int l_buff_size = DIM_MAX;
  /* to store the PID */
  pid_t l_pid;
  /* open the directory to scan */
  l_dir = opendir("/proc");
  /* error handler */
  if (!l_dir) {
    log_message("Cannot open dir for %s\n!", p_app_name);
    exit(1);
  }
  /* while there are more entries */
  while ((l_next = readdir(l_dir)) != NULL) {
    /* current file in the directory */
    FILE *l_status;
    /* file handling variables */
    char l_filename[l_buff_size];
    /* aux buffer */
    char l_buffer[l_buff_size];
    /* application name with full path */
    char l_name[l_buff_size];
    /* extracted application name */
    char l_aname[l_buff_size];
    /* binary name start position */
    char *l_start_pos;
    /* binary name end position */
    char *l_last_pos;
    /* binary length name */
    int l_len = 0;
    /* current index */
    char *l_idx;
    /* test if we are out of the proc directory */
    if (strcmp(l_next->d_name, "..") == 0)
      continue;
    /* test if the current dir name is formed from digits */
    if (!isdigit(*l_next->d_name))
      continue;
    /* extract names from PID dirs */
    sprintf(l_filename, "/proc/%s/cmdline", l_next->d_name);
    if (!(l_status = fopen(l_filename, "r"))) {
      continue;
    }
    /* buffer to store all app names */
    if (fgets(l_buffer, l_buff_size - 1, l_status) == NULL) {
      fclose(l_status);
      continue;
    }
    fclose(l_status);
    /* fill the buffer from /proc */
    sscanf(l_buffer, "%s", l_name);
    /* extract the name from the absolute path  */
    /* compute the start position */
    l_start_pos = strrchr(l_name, '/');
    if (l_start_pos)
      /* compute the current index */
      l_idx = (l_start_pos + 1);
    else
      l_idx = l_name;
    /* compute the end position */
    l_last_pos = strchr(l_name, ' ');
    if (!l_last_pos)
      /* compute length */
      l_len = strlen(l_idx);
    else if (l_start_pos)
      l_len = l_last_pos - l_start_pos;
    else
      l_len = l_last_pos - l_idx;
    /* create the name to test */
    strncpy(l_aname, l_idx, l_len);
    l_aname[l_len] = '\0';
    /* check if the name parameter is the name is identical to the extracted value */
    if (strcmp(l_aname, p_app_name) == 0) {
      l_pid = strtol(l_next->d_name, NULL, 0);
    }
  }
  return l_pid;
}


/* Find application name from PID */
void AppNameFromPid(int p_pid, char *p_app_name)
{
  /* file descriptor */
  FILE *l_fp;
  /* working buffer */
  char l_buf[DIM_MAX];
  /* absolute path from where the info is extracted */
  char l_path[DIM_MAX];
  /* current line */
  char l_line[DIM_MAX];
  /* extracted application name */
  char l_aline[DIM_MAX];
/* binary name start position */
  char *l_start_pos;
  /* binary name end position */
  char *l_last_pos;
  /* binary length name */
  int l_len = 0;
  /* current index */
  char *l_idx;
  /* convert pid intro string */
  sprintf(l_buf, "%d", p_pid);
  /* acces the commandline */
  sprintf(l_path, "/proc/%s/cmdline", l_buf);
  /* open the cmdline to extract the name of app */
  l_fp = fopen(l_path, "r");
  /* extract the name pf the app */
  fgets(l_line, sizeof(char) * DIM_MAX, l_fp);
  /* save the name in non-local var to avoid stack clear */
  /* extract the name from the absolute path  */
  /* compute the start position */
  l_start_pos = strrchr(l_line, '/');
  if (l_start_pos)
    /* compute the current index */
    l_idx = (l_start_pos + 1);
  else
    l_idx = l_line;
  /* compute the end position */
  l_last_pos = strchr(l_line, ' ');
  if (!l_last_pos)
    /* compute length */
    l_len = strlen(l_idx);
  else if (l_start_pos)
    l_len = l_last_pos - l_start_pos;
  else
    l_len = l_last_pos - l_idx;
  /* create the name to test */
  strncpy(l_aline, l_idx, l_len);
  l_aline[l_len] = '\0';
  strcpy(p_app_name, l_aline);
  /* free the file descriptor */
  fclose(l_fp);
}

/* Function responsible to parse the .timer unit and extract the triggering key */
GKeyFile *ParseUnitFile(char *p_file)
{

  unsigned long l_groups_length;
  char **l_groups;
  GKeyFile *l_out_new_key_file = g_key_file_new();
  GError *l_err = NULL;

  if (!g_key_file_load_from_file
      (l_out_new_key_file, p_file, G_KEY_FILE_NONE, &l_err)) {
    log_message
	("AL Daemon : Cannot load from timer unit key file! (%d: %s)\n",
	 l_err->code, l_err->message);
    return NULL;
    g_error_free(l_err);
  }

  l_groups = g_key_file_get_groups(l_out_new_key_file, &l_groups_length);
  if (l_groups == NULL) {
    log_message
	("AL Daemon : Could not retrieve groups from %s timer unit file!\n",
	 p_file);
    return NULL;
  }
  unsigned long l_i;
  for (l_i = 0; l_i < l_groups_length; l_i++) {
    unsigned long l_keys_length;
    char **l_keys;

    l_keys =
	g_key_file_get_keys(l_out_new_key_file, l_groups[l_i],
			    &l_keys_length, &l_err);

    if (l_keys == NULL) {
      log_message
	  ("AL Daemon : Error in retrieving keys in timer unit file! (%d: %s)",
	   l_err->code, l_err->message);
      g_error_free(l_err);
    } else {
      unsigned long l_j;
      for (l_j = 0; l_j < l_keys_length; l_j++) {
	char *l_str_value;
	l_str_value =
	    g_key_file_get_string(l_out_new_key_file, l_groups[l_i],
				  l_keys[l_j], &l_err);
	if (l_str_value == NULL) {
	  log_message
	      ("AL Daemon : Error retrieving key's value in timer unit file. (%d, %s)\n",
	       l_err->code, l_err->message);
	  g_error_free(l_err);
	}

      }
    }
  }

  return l_out_new_key_file;
}

/* Function responsible to parse the .timer unit and setup the triggering key value */
void SetupUnitFileKey(char *p_file, char *p_key, char *p_val)
{

  GKeyFile *l_key_file = ParseUnitFile(p_file);
  unsigned long l_file_length;

  if (p_file == NULL) {
    exit(1);
  }

  g_key_file_set_string(l_key_file, "Timer", p_key, p_val);

  GError *l_err = NULL;
  char *l_new_file_data =
      g_key_file_to_data(l_key_file, &l_file_length, &l_err);

  g_key_file_free(l_key_file);

  if (l_new_file_data == NULL) {
    log_message
	("AL Daemon : Could not get new file data for timer unit! (%d: %s)",
	 l_err->code, l_err->message);
    g_error_free(l_err);
    exit(1);
  }
  if (!g_file_set_contents(p_file, l_new_file_data, l_file_length, &l_err)) {
    log_message
	("AL Daemon : Could not save new file for timer unit! (%d: %s)",
	 l_err->code, l_err->message);
    g_error_free(l_err);
    exit(1);
  }
}

/* High level interface for the AL Daemon */
void Run(bool p_isFg, int p_parentPID, char *p_commandLine)
{
  // TODO 
  // isFg and parentPID usage when starting applications

  /* extract application name and command line parameters
     parsing the commandLine argument */

  // delimiters for the command line argument
  const char l_delim[] = "  ";
  // current token in the command line
  char *l_token;
  // app name and options
  char *l_name;
  // copy of the original string to avoid modification
  char *l_buff;

  // copy the string
  l_buff = strdup(p_commandLine);
  // extract first token : app name
  l_name = strtok(l_buff, l_delim);
  // extract next tokens : app options
  // init options string
  l_token = strtok(NULL, l_delim);

  if (strcmp(p_commandLine, "reboot") == 0) {
    AlSystemMethodCall("Run", "reboot.timer", l_token);
  }
  if (strcmp(p_commandLine, "poweroff") == 0) {
    AlSystemMethodCall("Run", "poweroff.timer", l_token);
  }
  if (strcmp(p_commandLine, "shutdown") == 0) {
    AlSystemMethodCall("Run", "shutdown.timer", l_token);
  }
  AlSystemMethodCall("Run", l_name, " ");
  
  log_message("%s started with run !\n", l_name);
}

void RunAs(int p_egid, int p_euid, bool p_isFg, int p_parentPID,
	   char *p_commandLine)
{
  // TODO
  // egid, euid, isFg and parentPID usage when starting applications 
  /* extract application name and command line parameters
     parsing the commandLine argument */

  // delimiters for the command line argument
  const char l_delim[] = " -";
  // current token in the command line
  char *l_token;
  // app name and options
  char *l_name;
  char l_options[DIM_MAX];
  // copy of the original string to avoid modification
  char *l_buff;
  // copy the string
  l_buff = strdup(p_commandLine);
  // extract first token : app name
  l_name = strtok(l_buff, l_delim);
  // extract next tokens : app options
  l_token = strtok(NULL, l_delim);

  if (strcmp(p_commandLine, "reboot") == 0) {
    AlSystemMethodCall("Run", "reboot.timer", l_token);
  }
  if (strcmp(p_commandLine, "poweroff") == 0) {
    AlSystemMethodCall("Run", "poweroff.timer", l_token);
  }
  if (strcmp(p_commandLine, "shutdown") == 0) {
    AlSystemMethodCall("Run", "shutdown.timer", l_token);
  }
  AlSystemMethodCall("RunAs", l_name, " ");

  log_message("%s started with RunAs !\n", l_name);
  // TODO add signaling mechanisms AlSendAppSignal(AL_SIGNAME_TASK_STARTED);
  // change out newPID
}

void Suspend(int p_pid)
{

  char l_app_name[DIM_MAX];
  char *l_commandLine;
  AppNameFromPid(p_pid, l_app_name);
  l_commandLine = l_app_name;
  AlSystemMethodCall("Suspend", strcat(l_commandLine, ".service"), " ");


  // TODO add signaling mechanisms  AlSendAppSignal(AL_SIGNAME_TASK_STOPPED);
}

void Resume(int p_pid)
{

 
  char l_app_name[DIM_MAX];
  char *l_commandLine;
  AppNameFromPid(p_pid, l_app_name);
  l_commandLine = l_app_name;
  AlSystemMethodCall("Resume", strcat(l_commandLine, ".service"), " ");
  //TODO add signaling mechanisms  AlSendAppSignal(AL_SIGNAME_TASK_STARTED);
}

void Stop(int p_egid, int p_euid, int p_pid)
{

  char l_app_name[DIM_MAX];
  char *l_commandLine;
  AppNameFromPid(p_pid, l_app_name);
  printf("stopping %s\n", l_app_name);
  l_commandLine = l_app_name;
  AlSystemMethodCall("Stop", strcat(l_commandLine, ".service"), " ");
  //TODO add signaling mechanisms   AlSendAppSignal(AL_SIGNAME_TASK_STOPPED);
}

void TaskStarted()
{

  log_message("Task received %s signal!\n", AL_SIGNAME_TASK_STARTED);

  // out : char* ImagePath , int pid
}

void TaskStopped()
{

  log_message("Task received %s signal!\n", AL_SIGNAME_TASK_STOPPED);
  // out : char* ImagePath, int pid
}

/* Call a method on a system object */
void AlSystemMethodCall(char *p_param, char *p_app, char *p_app_args)
{
  DBusMessage *l_msg;
  DBusMessageIter l_args;
  DBusConnection *l_conn;
  DBusError l_err;
  DBusPendingCall *l_pending;
  int l_ret;
  bool l_stat = 0;
  dbus_uint32_t l_level = 0;


  fprintf(stderr,
	  "AL System Method Caller : Calling remote method %s for application %s\n",
	  p_param, p_app);

  // initialise the errors
  dbus_error_init(&l_err);

  // connect to the system bus and check for errors
  l_conn = dbus_bus_get(DBUS_BUS_SYSTEM, &l_err);
  if (dbus_error_is_set(&l_err)) {

    fprintf(stderr,
	    "AL System Method Caller : Connection Error (%s)\n",
	    l_err.message);

    dbus_error_free(&l_err);
  }
  if (NULL == l_conn) {
    exit(1);
  }
  // request our name on the bus
  l_ret =
      dbus_bus_request_name(l_conn, AL_DAEMON_CALLER_NAME,
			    DBUS_NAME_FLAG_REPLACE_EXISTING, &l_err);
  if (dbus_error_is_set(&l_err)) {

    fprintf(stderr, "AL System Method Caller : Name Error (%s)\n",
	    l_err.message);

    dbus_error_free(&l_err);
  }
  if (DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER != l_ret) {
    exit(1);
  }
  // filter and create a new method call message and check for errors
  if (strcmp(p_param, "Run") == 0) {

    l_msg = dbus_message_new_method_call(AL_DAEMON_SERVER_NAME,	// target for the method call
					 AL_DAEMON_SYSTEM_OBJECT_PATH,	// object to call on
					 AL_DAEMON_SYSTEM_INTERFACE,	// interface to call on
					 "StartUnit");	// method name
  } else if (strcmp(p_param, "RunAs") == 0) {

    l_msg = dbus_message_new_method_call(AL_DAEMON_SERVER_NAME,	// target for the method call
					 AL_DAEMON_SYSTEM_OBJECT_PATH,	// object to call on
					 AL_DAEMON_SYSTEM_INTERFACE,	// interface to call on
					 "StartUnit");	// method name
  } else if (strcmp(p_param, "Stop") == 0) {

    l_msg = dbus_message_new_method_call(AL_DAEMON_SERVER_NAME,	// target for the method call
					 AL_DAEMON_SYSTEM_OBJECT_PATH,	// object to call on
					 AL_DAEMON_SYSTEM_INTERFACE,	// interface to call on
					 "StopUnit");	// method name
  } else if (strcmp(p_param, "Suspend") == 0) {

    l_msg = dbus_message_new_method_call(AL_DAEMON_SERVER_NAME,	// target for the method call
					 AL_DAEMON_SYSTEM_OBJECT_PATH,	// object to call on
					 AL_DAEMON_SYSTEM_INTERFACE,	// interface to call on
					 "CreateSnapshot");	// method name
  } else if (strcmp(p_param, "Resume") == 0) {

    l_msg = dbus_message_new_method_call(AL_DAEMON_SERVER_NAME,	// target for the method call
					 AL_DAEMON_SYSTEM_OBJECT_PATH,	// object to call on
					 AL_DAEMON_SYSTEM_INTERFACE,	// interface to call on
					 "StartUnit");	// method name
  }
  if (NULL == l_msg) {

    fprintf(stderr, "AL System Method Caller : Message Null\n");

    exit(1);
  }
  // append necessary arguments (application name and arguments)
  dbus_message_append_args(l_msg, DBUS_TYPE_STRING, &p_app,
			   DBUS_TYPE_STRING, &p_app_args,
			   DBUS_TYPE_INVALID);

  // append arguments
  dbus_message_iter_init_append(l_msg, &l_args);
  if (!dbus_message_iter_append_basic(&l_args, DBUS_TYPE_STRING, &p_param)) {

    fprintf(stderr, "AL System Method Caller : Args Out Of Memory!\n");

    exit(1);
  }

// send message and get a handle for a reply
  if (!dbus_connection_send_with_reply(l_conn, l_msg, &l_pending, -1)) {	// -1 is default timeout
    
    fprintf(stderr,
	    "AL System Method Caller : Connection Out Of Memory!\n");
    
    exit(1);
  }
  if (NULL == l_pending) {
    
    fprintf(stderr, "AL System Method Caller : Pending Call Null\n");
    
    exit(1);
  }
  // flush the connection
  dbus_connection_flush(l_conn);
    
  fprintf(stderr, "AL System Method Caller : Request Sent\n");
    
  // free message
  dbus_message_unref(l_msg);

  // block until we recieve a reply
  dbus_pending_call_block(l_pending);

  // get the reply message
  l_msg = dbus_pending_call_steal_reply(l_pending);
  if (NULL == l_msg) {
    
    fprintf(stderr, "AL System Method Caller : Reply Null\n");
    
    exit(1);
  }

  // free the pending message handle
  dbus_pending_call_unref(l_pending);

  // read the parameters
  if (!dbus_message_iter_init(l_msg, &l_args)) {

    fprintf(stderr,
	    "AL System Method Caller : Message has no arguments!\n");

  } else if (DBUS_TYPE_BOOLEAN != dbus_message_iter_get_arg_type(&l_args)) {

    fprintf(stderr,
	    "AL System Method Caller : Argument is not boolean!\n");

  } else {
    dbus_message_iter_get_basic(&l_args, &l_stat);
  }

  if (!dbus_message_iter_next(&l_args)) {

    fprintf(stderr,
	    "AL System Method Caller : Message has too few arguments!\n");

  } else if (DBUS_TYPE_UINT32 != dbus_message_iter_get_arg_type(&l_args)) {

    fprintf(stderr, "AL System Method Caller : Argument is not int!\n");

  } else {
    dbus_message_iter_get_basic(&l_args, &l_level);
  }

  fprintf(stderr, "AL System Method Caller : Got Reply: %d, %d\n",
	  l_stat, l_level);

   // free reply
  dbus_message_unref(l_msg);
}

/* Connect to the DBUS bus and send a broadcast signal */
void AlSendAppSignal(char *p_sigvalue)
{
  /* message to be sent */
  DBusMessage *l_msg;
  /* message arguments */
  DBusMessageIter l_args;
  /* connection to the bus */
  DBusConnection *l_conn;
  /* error */
  DBusError l_err;
  int l_ret;
  dbus_uint32_t l_serial = 0;

  log_message("AL Daemon Send Signal : Sending signal with value %s\n",
	      p_sigvalue);

  // initialise the error value
  dbus_error_init(&l_err);

  // connect to the DBUS system bus, and check for errors
  l_conn = dbus_bus_get(DBUS_BUS_SYSTEM, &l_err);
  if (dbus_error_is_set(&l_err)) {
    log_message("AL Daemon Send Signal : Connection Error (%s)\n",
		l_err.message);
    dbus_error_free(&l_err);
  }
  if (NULL == l_conn) {
    exit(1);
  }
  // register our name on the bus, and check for errors
  l_ret =
      dbus_bus_request_name(l_conn, AL_SIG_LISTENER,
			    DBUS_NAME_FLAG_REPLACE_EXISTING, &l_err);
  if (dbus_error_is_set(&l_err)) {
    log_message("AL Daemon Send Signal : Name Error (%s)\n",
		l_err.message);
    dbus_error_free(&l_err);
  }
  if (DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER != l_ret) {
    exit(1);
  }
  // create a signal & check for errors 
  if (strcmp(p_sigvalue, AL_SIGNAME_TASK_STARTED) == 0) {
    l_msg = dbus_message_new_signal(SRM_OBJECT_PATH,	// object name of the signal
				    AL_SIGNAL_INTERFACE,	// interface name of the signal
				    AL_SIGNAME_TASK_STARTED);	// name of the signal
  } else if (strcmp(p_sigvalue, AL_SIGNAME_TASK_STOPPED) == 0) {
    l_msg = dbus_message_new_signal(SRM_OBJECT_PATH,	// object name of the signal
				    AL_SIGNAL_INTERFACE,	// interface name of the signal
				    AL_SIGNAME_TASK_STOPPED);	// name of the signal
  }
  /* check for message state */
  if (NULL == l_msg) {
    log_message("AL Daemon Send Signal %s : Message Null\n", p_sigvalue);
    exit(1);
  }
  // append arguments onto signal
  dbus_message_iter_init_append(l_msg, &l_args);
  if (!dbus_message_iter_append_basic
      (&l_args, DBUS_TYPE_STRING, &p_sigvalue)) {
    log_message("AL Daemon Send Signal %s: Args Out Of Memory!\n",
		p_sigvalue);
    exit(1);
  }
  // send the message and flush the connection
  if (!dbus_connection_send(l_conn, l_msg, &l_serial)) {
    log_message
	("AL Daemon Send Signal %s: Connection Out Of Memory!\n",
	 p_sigvalue);
    exit(1);
  }
  // flush the connection
  dbus_connection_flush(l_conn);

  log_message("AL Daemon Send Signal %s: Signal Sent\n", p_sigvalue);

  // free the message
  dbus_message_unref(l_msg);
}

/* Receive the method calls and reply */
void AlReplyToMethodCall(DBusMessage * p_msg, DBusConnection * p_conn)
{
  /* reply message */
  DBusMessage *l_reply;
  /* reply arguments */
  DBusMessageIter l_args;
  bool l_stat = true;
  dbus_uint32_t l_level = 21614;
  dbus_uint32_t l_serial = 0;
  char *l_param = "";


  // read the arguments
  if (!dbus_message_iter_init(p_msg, &l_args)) {
    log_message
	("AL Daemon Reply to Method Call : Message has no arguments!%s",
	 "\n");
  } else if (DBUS_TYPE_STRING != dbus_message_iter_get_arg_type(&l_args)) {
    log_message
	("AL Daemon Reply to Method Call : Argument is not string!%s",
	 "\n");
  } else {
    dbus_message_iter_get_basic(&l_args, &l_param);
    log_message
	("AL Daemon Reply to Method Call : Method called with %s\n",
	 l_param);
  }

  // create a reply from the message
  l_reply = dbus_message_new_method_return(p_msg);

  // add the arguments to the reply
  dbus_message_iter_init_append(l_reply, &l_args);
  if (!dbus_message_iter_append_basic(&l_args, DBUS_TYPE_BOOLEAN, &l_stat)) {
    log_message
	("AL Daemon Reply to Method Call : Arg Bool Out Of Memory!%s",
	 "\n");
    exit(1);
  }
  if (!dbus_message_iter_append_basic(&l_args, DBUS_TYPE_UINT32, &l_level)) {
    log_message
	("AL Daemon Reply to Method Call : Arg Int Out Of Memory!%s",
	 "\n");
    exit(1);
  }
  // send the reply && flush the connection
  if (!dbus_connection_send(p_conn, l_reply, &l_serial)) {
    log_message
	("AL Daemon Reply to Method Call : Connection Out Of Memory!%s",
	 "\n");
    exit(1);
  }
  dbus_connection_flush(p_conn);

  // free the reply
  dbus_message_unref(l_reply);
}

/* Server that exposes a method call and waits for it to be called */
void AlListenToMethodCall()
{
  DBusMessage *l_msg;
  DBusConnection *l_conn;
  DBusError l_err;
  int l_ret;
  char *l_app;
  char *l_app_args;
  char *l_cmdLine;

  log_message
      ("AL Daemon Method Call Listener : Listening for method calls!%s",
       "\n");

  // initialise the error
  dbus_error_init(&l_err);

  // connect to the bus and check for errors
  l_conn = dbus_bus_get(DBUS_BUS_SYSTEM, &l_err);
  if (dbus_error_is_set(&l_err)) {
    log_message
	("AL Daemon Method Call Listener : Connection Error (%s)\n",
	 l_err.message);
    dbus_error_free(&l_err);
  }
  if (NULL == l_conn) {
    log_message("AL Daemon Method Call Listener : Connection Null!%s",
		"\n");
    exit(1);
  }
  // request our name on the bus and check for errors
  l_ret =
      dbus_bus_request_name(l_conn, AL_SERVER_NAME,
			    DBUS_NAME_FLAG_REPLACE_EXISTING, &l_err);
  if (dbus_error_is_set(&l_err)) {
    log_message("AL Daemon Method Call Listener : Name Error (%s)\n",
		l_err.message);
    dbus_error_free(&l_err);
  }
  if (DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER != l_ret) {
    log_message
	("AL Daemon Method Call Listener : Not Primary Owner (%d)\n",
	 l_ret);
    exit(1);
  }
  // loop, testing for new messages
  while (true) {
    // non blocking read of the next available message
    dbus_connection_read_write(l_conn, 0);
    l_msg = dbus_connection_pop_message(l_conn);

    // loop again if we haven't got a message
    if (NULL == l_msg) {
      sleep(1);
      continue;
    }
    // check this is a method call for the right interface & method

    if (dbus_message_is_method_call(l_msg, AL_METHOD_INTERFACE, "Run")) {
      AlReplyToMethodCall(l_msg, l_conn);
      dbus_message_get_args(l_msg, &l_err, DBUS_TYPE_STRING, &l_app,
			    DBUS_TYPE_STRING, &l_app_args,
			    DBUS_TYPE_INVALID);
      log_message("Run app: %s\n", l_app);
      if (strcmp(l_app, "reboot") == 0) {
	SetupUnitFileKey("/lib/systemd/system/reboot.timer",
			 "OnActiveSec", l_app_args);
      }
      if (strcmp(l_app, "shutdown") == 0) {
	SetupUnitFileKey("/lib/systemd/system/shutdown.timer",
			 "OnActiveSec", l_app_args);
      }
      if (strcmp(l_app, "poweroff") == 0) {
	SetupUnitFileKey("/lib/systemd/system/poweroff.timer",
			 "OnActiveSec", l_app_args);
      }
      // build the command line
      strcpy(l_cmdLine, l_app);
      strcat(l_cmdLine, " ");
      strcat(l_cmdLine, l_app_args);
      Run(true, 0, l_cmdLine);
    }

    if (dbus_message_is_method_call(l_msg, AL_METHOD_INTERFACE, "RunAs")) {
      AlReplyToMethodCall(l_msg, l_conn);
      dbus_message_get_args(l_msg, &l_err, DBUS_TYPE_STRING, &l_app,
			    DBUS_TYPE_STRING, &l_app_args,
			    DBUS_TYPE_INVALID);
      log_message("RunAs app: %s\n", l_app);
      if (strcmp(l_app, "reboot") == 0) {
	SetupUnitFileKey("/lib/systemd/system/reboot.timer",
			 "OnActiveSec", l_app_args);
      }
      if (strcmp(l_app, "shutdown") == 0) {
	SetupUnitFileKey("/lib/systemd/system/shutdown.timer",
			 "OnActiveSec", l_app_args);
      }
      if (strcmp(l_app, "poweroff") == 0) {
	SetupUnitFileKey("/lib/systemd/system/poweroff.timer",
			 "OnActiveSec", l_app_args);
      }
      // build the command line
      strcpy(l_cmdLine, l_app);
      strcat(l_cmdLine, " ");
      strcat(l_cmdLine, l_app_args);
      RunAs(0, 0, true, 0, l_cmdLine);
    }

    if (dbus_message_is_method_call(l_msg, AL_METHOD_INTERFACE, "Stop")) {
      AlReplyToMethodCall(l_msg, l_conn);
      dbus_message_get_args(l_msg, &l_err, DBUS_TYPE_STRING, &l_app,
			    DBUS_TYPE_INVALID);
      log_message("Stop app: %s\n", l_app);
      Stop(0, 0, (int) AppPidFromName(l_app));
    }

    if (dbus_message_is_method_call(l_msg, AL_METHOD_INTERFACE, "Resume")) {
      AlReplyToMethodCall(l_msg, l_conn);
      dbus_message_get_args(l_msg, &l_err, DBUS_TYPE_STRING, &l_app,
			    DBUS_TYPE_INVALID);
      log_message("Resume app: %s\n", l_app);
      Resume((int) AppPidFromName(l_app));
    }

    if (dbus_message_is_method_call(l_msg, AL_METHOD_INTERFACE, "Suspend")) {
      AlReplyToMethodCall(l_msg, l_conn);
      dbus_message_get_args(l_msg, &l_err, DBUS_TYPE_STRING, &l_app,
			    DBUS_TYPE_INVALID);
      log_message("Suspend app: %s\n", l_app);
      Suspend((int) AppPidFromName(l_app));
    }
    // free the message
    dbus_message_unref(l_msg);
  }
}

int main(int argc, char **argv)
{
  /* setup a PID and a SID for our AL Interface daemon */
  pid_t l_al_pid, l_al_sid;

  /* fork off the parent process */
  l_al_pid = fork();
  if (l_al_pid < 0) {
    log_message("AL Daemon : Cannot fork off parent process!%s", "\n");
    exit(EXIT_FAILURE);
  }

  /* if the PID is ok then exit the parent process */
  if (l_al_pid > 0) {
    exit(EXIT_SUCCESS);
  }

  /* change the file mode mask to have full access to the 
     files generated by the daemon */
  umask(0);

  /* create a new SID for the child process to avoid becoming an orphan */
  l_al_sid = setsid();
  if (l_al_sid < 0) {
    log_message("AL Daemon : Cannot set SID for the process!%s", "\n");
    exit(EXIT_FAILURE);
  }

  /* change the current working directory */
  if ((chdir("/")) < 0) {
    log_message("AL Daemon : Cannot change directory for process!%s",
		"\n");
    exit(EXIT_FAILURE);
  }

  /* close unneeded file descriptors to maintain security */
  //close(STDIN_FILENO);

  /* specific initialization code */
  // TODO add specific init calls here if needed

  /* main daemon loop */
  while (1) {

    if (2 > argc) {
      fprintf(stdout, "Syntax: al-daemon --start [--verbose|-v]\n"
	      "\tal-daemon --stop\n");
      return 1;
    }
    if (0 == strcmp(argv[1], "--start")) {
      if (argc == 3) {
	if ((0 == strcmp(argv[2], "--verbose"))
	    || (0 == strcmp(argv[2], "-v"))) {
	  g_verbose = 1;
	}
      }
      AlListenToMethodCall();
    } else if (0 == strcmp(argv[1], "--stop")) {
      system("killall -9 al-daemon");
    } else {
      fprintf(stdout, "Syntax: al-daemon --start [-verbose|-v]\n"
	      "\tal-daemon --stop\n");

      return 1;
    }
  }
  return 0;
}
