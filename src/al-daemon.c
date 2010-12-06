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
    printf("Cannot open dir for %s\n!", p_app_name);
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


/* High level interface for the AL Daemon */
void Run(bool p_isFg, int p_parentPID, char *p_commandLine)
{
  // TODO 
  // isFg and parentPID usage when starting applications
  int l_ret;
  log_message("%s started with run !\n", p_commandLine);
  char l_cmd[DIM_MAX];
  sprintf(l_cmd, "systemctl start %s.service", p_commandLine);
  l_ret = system(l_cmd);
  if (l_ret == -1) {
    log_message
	("AL Daemon : Application cannot be started with run! Err: %s\n",
	 strerror(errno));
  }
}

void RunAs(int p_egid, int p_euid, bool p_isFg, int p_parentPID, char *p_commandLine)
{
  // TODO
  // egid, euid, isFg and parentPID usage when starting applications 
  int l_ret;
  log_message("%s started with runas !\n", p_commandLine);
  char l_cmd[DIM_MAX];
  sprintf(l_cmd, "systemctl start %s.service", p_commandLine);
  l_ret = system(l_cmd);

  if (l_ret == -1) {
    log_message
	("AL Daemon : Application cannot be started with runas! Err: %s\n",
	 strerror(errno));
  }
  // TODO add signaling mechanisms AlSendAppSignal(AL_SIGNAME_TASK_STARTED);
  // change out newPID
}

void Suspend(int p_pid)
{
  int l_ret;
  char l_app_name[DIM_MAX];
  char *l_commandLine;
  AppNameFromPid(p_pid, l_app_name);
  l_commandLine = l_app_name;
  char l_cmd[DIM_MAX];
  log_message("%s canceled with suspend !\n", l_commandLine);
  sprintf(l_cmd, "systemctl cancel %s.service", l_commandLine);
  l_ret = system(l_cmd);

  if (l_ret == -1) {
    log_message
	("AL Daemon : Application cannot be suspended! Err:%s\n",
	 strerror(errno));
    // TODO add signaling mechanisms  AlSendAppSignal(AL_SIGNAME_TASK_STOPPED);
  }
}

void Resume(int p_pid)
{

  int l_ret;
  char l_app_name[DIM_MAX];
  char *l_commandLine;
  AppNameFromPid(p_pid, l_app_name);
  l_commandLine = l_app_name;
  char l_cmd[DIM_MAX];
  log_message("%s restarted with resume !\n", l_commandLine);
  sprintf(l_cmd, "systemctl restart %s.service", l_commandLine);
  l_ret = system(l_cmd);

  if (l_ret == -1) {
    log_message("AL Daemon : Application cannot be resumed! Err:%s\n",
		strerror(errno));
  }
  //TODO add signaling mechanisms  AlSendAppSignal(AL_SIGNAME_TASK_STARTED);
}

void Stop(int p_egid, int p_euid, int p_pid)
{
  int l_ret;
  char l_app_name[DIM_MAX];
  AppNameFromPid(p_pid, l_app_name);
  printf("stopping %s\n", l_app_name);
  char l_cmd[DIM_MAX];
  log_message("%s stopped with stop !\n", l_app_name);
  sprintf(l_cmd, "systemctl stop %s.service", l_app_name);
  l_ret = system(l_cmd);

  if (l_ret == -1) {
    log_message("AL Daemon : Application cannot be stopped! Err:%s\n",
		strerror(errno));
  }
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
    log_message("AL Daemon Send Signal : Name Error (%s)\n", l_err.message);
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
  if (!dbus_message_iter_append_basic(&l_args, DBUS_TYPE_STRING, &p_sigvalue)) {
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
void AlReplyToMethodCall(DBusMessage *p_msg, DBusConnection *p_conn)
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
	("AL Daemon Method Call Listener : Not Primary Owner (%d)\n", l_ret);
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
      Run(true, 0, l_app);
    }

    if (dbus_message_is_method_call(l_msg, AL_METHOD_INTERFACE, "RunAs")) {
      AlReplyToMethodCall(l_msg, l_conn);
      dbus_message_get_args(l_msg, &l_err, DBUS_TYPE_STRING, &l_app,
			    DBUS_TYPE_STRING, &l_app_args,
			    DBUS_TYPE_INVALID);
      log_message("RunAs app: %s\n", l_app);
      RunAs(0, 0, true, 0, l_app);
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
