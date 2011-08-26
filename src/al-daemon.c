/* 
* al-daemon.c, contains the implementation of the main application launcher loop
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
#include "lum.h"
#include "notifier.h"
#include "dbus_interface.h"
#include "utils.h"

/* CLI commands */
unsigned char g_stop = 0;
unsigned char g_start = 0;

/* Function responsible with the command line interface output */
void AlPrintCLI()
{
  fprintf(stdout,
	  "Syntax: \n"
	  "   al-daemon --start|-S options\n"
	  "   al-daemon --stop|-K\n"
	  "   al-daemon --version|-V\n"
	  "   al-daemon --help|-H\n"
	  "\n"
	  "Options: \n"
	  "  --verbose|-v prints the internal daemon log messages\n");
}

/* Function responsible with command line options parsing */
void AlParseCLIOptions(int argc, char *const *argv)
{
  /* accepted commnad line options */
  static struct option l_long_opts[] = {
    {"start", 0, NULL, 'S'},
    {"stop", 0, NULL, 'K'},
    {"help", 0, NULL, 'H'},
    {"version", 0, NULL, 'V'},
    {"verbose", 0, NULL, 'v'},
    {NULL, 0, NULL, 0}
  };

  int l_op;
  /* option parsing */
  while (1) {
    l_op = getopt_long(argc, argv, "HKSVv", l_long_opts, (int *) 0);

    if (l_op == -1)
      break;
    switch (l_op) {
    case 'H':			/* printf the help */
      AlPrintCLI();
      return;
    case 'K':			/* kill the daemon */
      g_stop = 1;
      break;
    case 'S':			/* start the daemon */
      g_start = 1;
      break;
    case 'V':			/* print version */
      log_message("\nAL Daemon version %s\n", AL_VERSION);
      exit(0);
    case 'v' :
      break;
    default:
      AlPrintCLI();
      return;
    }
  }
  if (argc < 2)
    AlPrintCLI();
}

/* Server that exposes a method call and waits for it to be called; the main daemon loop */
void AlListenToMethodCall()
{
  /* define message and reply */
  DBusMessage *l_msg, *l_reply, *l_msg_notif, *l_reply_notif, *l_msg_state, *l_reply_state, *l_sig_state_notif;
  /* define connection for default method calls and notification signals */
  DBusConnection *l_conn;
  /* error definition */
  DBusError l_err;
  /* return code */
  int l_ret, l_r;
  /* auxiliary storage for name handling */
  char *l_app = malloc(DIM_MAX*sizeof(l_app));
  /* application arguments */
  char *l_app_args;
  char *l_app_name;
  /* variables for state extraction */
  char *l_app_status;
  char l_state_info[DIM_MAX];
  char *l_active_state = malloc(DIM_MAX * sizeof(l_active_state));
  char *l_sub_state = malloc(DIM_MAX * sizeof(l_sub_state));
  /* delimiters for service / application name extraction */
  char l_delim_serv[] = " ";
  /* application PID */
  int l_pid;
  /* ownership values to pass to RunAs or StopAs user dependent methods */
  int l_uid_val, l_gid_val; 
  /* timing value for deferred tasks */
  char *l_time;
  /* user name to execute task as */
  char *l_user;
  /* handlers for deferred execution unit and timing extraction */
  char *l_app_copy = malloc(DIM_MAX*sizeof(l_app_copy)); 
  char *l_app_deferred = malloc(DIM_MAX*sizeof(l_app_deferred));
  /* setup fg/bg application state */
  bool l_fg_state;
  /* application object path in state property setup */
  char *l_path;
  /* iterator used for variant DBus type that stores the state property value */
  DBusMessageIter l_iter, l_variant; 
  /* load unit message for systemd */
  DBusMessage *l_load_msg, *l_load_reply;
  log_message
      ("AL Daemon Method Call Listener : Listening for method calls!%s",
       "\n");

  /* initialise the error */
  dbus_error_init(&l_err);

  /* connect to the bus and check for errors */
  l_conn = dbus_bus_get(DBUS_BUS_SYSTEM, &l_err);
  if (dbus_error_is_set(&l_err)) {
    log_error_message
	("AL Daemon Method Call Listener : Connection Error (%s)\n",
	 l_err.message);
    dbus_error_free(&l_err);
  }
  if (NULL == l_conn) {
    log_error_message("AL Daemon Method Call Listener : Connection Null!%s",
		"\n");
    return;
  }
  /* request our name on the bus and check for errors */
  l_ret =
      dbus_bus_request_name(l_conn, AL_SERVER_NAME,
			    DBUS_NAME_FLAG_REPLACE_EXISTING, &l_err);
  if (dbus_error_is_set(&l_err)) {
    log_error_message("AL Daemon Method Call Listener : Name Error (%s)\n",
		l_err.message);
    dbus_error_free(&l_err);
  }
  if (DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER != l_ret) {
    log_error_message
	("AL Daemon Method Call Listener : Not Primary Owner (%d)! \n",
	 l_ret);
    log_error_message
	("AL Daemon : Daemon will be stopped !%s","\n");
    system("killall -9 al-daemon");
  }

  /* add matcher for notification on property change signals */
  dbus_bus_add_match(l_conn,
		     "type='signal',"
		     "sender='org.freedesktop.systemd1',"
		     "interface='org.freedesktop.DBus.Properties',"
		     "member='PropertiesChanged'", &l_err);
  if (dbus_error_is_set(&l_err)) {
    log_error_message
	("AL Daemon Method Call Listener : Failed to add signal matcher: %s\n",
	 l_err.message);
    if (l_msg)
      dbus_message_unref(l_msg);
    dbus_error_free(&l_err);
  }
  /* add matcher for method calls */
  dbus_bus_add_match(l_conn,
		     "type='method_call',"
		     "interface='org.GENIVI.AppL'", &l_err);

  if (dbus_error_is_set(&l_err)) {
    log_error_message
	("AL Daemon Method Call Listener : Failed to add method call matcher: %s\n",
	 l_err.message);
    if (l_msg)
      dbus_message_unref(l_msg);
    dbus_error_free(&l_err);
  }

  /* subscribe to systemd to receive state change notifications */
  if (!
      (l_msg_notif =
       dbus_message_new_method_call("org.freedesktop.systemd1",
				    "/org/freedesktop/systemd1",
				    "org.freedesktop.systemd1.Manager",
				    "Subscribe"))) {
    log_error_message
	("AL Daemon Method Call Listener : Could not allocate message when subscribing to systemd! \n %s \n",
	 l_err.message);
    if (l_msg_notif)
      dbus_message_unref(l_msg_notif);
    if (l_reply_notif)
      dbus_message_unref(l_reply_notif);
    dbus_error_free(&l_err);
  }

  if (!
      (l_reply_notif =
       dbus_connection_send_with_reply_and_block(l_conn, l_msg_notif,
						 -1, &l_err))) {
    log_error_message
	("AL Daemon Method Call Listener : Failed to issue method call: %s \n",
	 l_err.message);
    if (l_msg_notif)
      dbus_message_unref(l_msg_notif);
    if (l_reply_notif)
      dbus_message_unref(l_reply_notif);
    dbus_error_free(&l_err);
  }

  /* loop, testing for new messages */
  while (TRUE) {
    /* non blocking read of the next available message */
    dbus_connection_read_write(l_conn, 0);
    l_msg = dbus_connection_pop_message(l_conn);

    /* loop again if we haven't got a message */
    if (NULL == l_msg) {
      sleep(1);
      continue;
    }

    if (dbus_message_is_method_call(l_msg,
                                    "org.freedesktop.DBus.Introspectable",
                                    "Introspect")) {
        ReplyToIntrospect(l_msg, l_conn);
    }

    /* check if this is a method call for the right interface amd method */
    if (dbus_message_is_method_call(l_msg, AL_METHOD_INTERFACE, "Run")) {
      AlReplyToMethodCall(l_msg, l_conn);
      /* extract application name and arguments */
      dbus_message_get_args(l_msg, &l_err,
                            DBUS_TYPE_STRING, &l_app,
			    DBUS_TYPE_BOOLEAN, &l_fg_state,
			    DBUS_TYPE_INVALID);
      log_debug_message
	("AL Daemon Method Call Listener Run: Arguments were extracted for %s\n",
	 l_app);
      if ((strstr(l_app, "reboot") !=NULL) || (strstr(l_app, "poweroff") !=NULL)){
	      strcpy(l_app_copy, l_app);
	      l_app_deferred = strtok(l_app_copy, " ");
	      l_time = strtok(NULL, " ");
              strcpy(l_app, l_app_copy);
      }
      log_debug_message("AL Daemon Method Call Listener : Run app: %s\n", l_app);
      /* if reboot / shutdown unit add deferred functionality in timer file */
      if (strstr(l_app, "reboot") != NULL) {
	SetupUnitFileKey("/lib/systemd/system/reboot.timer",
			 "OnActiveSec", l_time, "reboot");
      }
      if (strstr(l_app, "poweroff") != NULL) {
	SetupUnitFileKey("/lib/systemd/system/poweroff.timer",
			 "OnActiveSec", l_time, "poweroff");
      }

      /* check for application service file existence */
      if (!AppExistsInSystem(l_app)) {
	log_error_message
	    ("AL Daemon Method Call Listener : Cannot run %s !\n Application %s is not found in the system !\n",
	     l_app, l_app);
	continue;
      } else {
	if ((l_r = (int) AppPidFromName(l_app)) != 0) {
           log_error_message
	      ("AL Daemon Method Call Listener : Cannot run %s !\n", l_app);
	  /* check the application current state before starting it */
	  /* extract the application state for testing existence */
          /* make a copy of the name to avoid additional postfix when calling run */
	  /* concatenate the appropiate string according the unit type i.e. service/target */
          if (AppExistsInSystem(l_app)==1){
           strcpy(l_app_copy, l_app);
	   if (AlGetAppState
	      (l_conn, strcat(l_app_copy, ".service"), l_state_info) == 0) {

	    /* copy the state */
	    l_app_status = strdup(l_state_info);

	    /* active state extraction from global state info */
	    l_active_state = strtok(l_app_status, l_delim_serv);
	    l_active_state = strtok(NULL, l_delim_serv);
	    l_active_state = strtok(NULL, l_delim_serv);
	    l_sub_state = strtok(NULL, l_delim_serv);
	    }
          }
	  if (AppExistsInSystem(l_app)==2){
           strcpy(l_app_copy, l_app);
	   if (AlGetAppState
	      (l_conn, strcat(l_app_copy, ".target"), l_state_info) == 0) {

	    /* copy the state */
	    l_app_status = strdup(l_state_info);

	    /* active state extraction from global state info */
	    l_active_state = strtok(l_app_status, l_delim_serv);
	    l_active_state = strtok(NULL, l_delim_serv);
	    l_active_state = strtok(NULL, l_delim_serv);
	    l_sub_state = strtok(NULL, l_delim_serv);
	    }
          }
           
	  if (strcmp(l_active_state, "active") == 0) {
	    if ((strcmp(l_sub_state, "exited") != 0)
		|| (strcmp(l_sub_state, "dead") != 0)
		|| (strcmp(l_sub_state, "failed") != 0)) {
	      log_error_message
		  ("AL Daemon Method Call Listener : Cannot run %s !\n Application/application group %s is already running in the system !\n",
		   l_app, l_app);
	      continue;
	    }
	  }
	}
      }
      /* ensure proper load state for the unit before starting it */
      if (!(l_load_msg =
       dbus_message_new_method_call("org.freedesktop.systemd1",
				    "/org/freedesktop/systemd1",
				    "org.freedesktop.systemd1.Manager",
				    "LoadUnit"))) {
	    log_error_message
		("AL Daemon Method Call Listener : Could not allocate message when loading unit for Run ! \n %s \n",
		 l_err.message);
	    if (l_load_msg)
	      dbus_message_unref(l_load_msg);
	    if (l_load_reply)
	      dbus_message_unref(l_load_reply);
	    dbus_error_free(&l_err);
	    continue;
	  }
   /* temp to store full service name */
   char *l_full_srv = malloc(DIM_MAX*sizeof(l_full_srv));;
   strcpy(l_full_srv, l_app);
   strcat(l_full_srv, ".service");
   /* append the name of the application to load */
    if (!dbus_message_append_args(l_load_msg,
				 DBUS_TYPE_STRING, &l_full_srv,
                                 DBUS_TYPE_INVALID)) {
                    log_error_message
			("AL Daemon Method Call Listener : Failed to append app name for Run when loading : %s \n",
			 l_err.message);
		    if (l_load_msg)
		      dbus_message_unref(l_load_msg);
		    if (l_load_reply)
		      dbus_message_unref(l_load_reply);
		    dbus_error_free(&l_err);
		    continue;
                }
  
    if (!(l_load_reply =
       dbus_connection_send_with_reply_and_block(l_conn, l_load_msg,
						 -1, &l_err))) {
	    log_error_message
		("AL Daemon Method Call Listener : Failed to issue LoadUnit method call for Run: %s \n",
		 l_err.message);
	    if (l_load_msg)
	      dbus_message_unref(l_load_msg);
	    if (l_load_reply)
	      dbus_message_unref(l_load_reply);
	    dbus_error_free(&l_err);
	    continue;
	    }
      /* setup foreground property in systemd and wait for reply */
      if(SetupApplicationStartupState(l_conn, l_app , l_fg_state)!=0){
		log_error_message("AL Daemon Method Call Listener : Cannot setup fg/bg state for %s , application will run in former state or default state \n" ,l_app);
      }
      /* run the application */
      Run(l_pid, l_fg_state, 0, l_app);
    }

    /* check if this is a method call for the right interface amd method */
    if (dbus_message_is_method_call(l_msg, AL_METHOD_INTERFACE, "RunAs")) {
      AlReplyToMethodCall(l_msg, l_conn);
            
      /* extract application name and arguments */
      dbus_message_get_args(l_msg, &l_err, 
			    DBUS_TYPE_STRING, &l_app,
 			    DBUS_TYPE_INT32, &l_uid_val,
 			    DBUS_TYPE_INT32, &l_gid_val,
			    DBUS_TYPE_BOOLEAN, &l_fg_state,
			    DBUS_TYPE_INVALID);
      /* test if deferred task */
      if ((strstr(l_app, "reboot") !=NULL) || (strstr(l_app, "poweroff") !=NULL)){
	      strcpy(l_app_copy, l_app);
	      l_app_deferred = strtok(l_app_copy, " ");
	      l_time = strtok(NULL, " ");
              strcpy(l_app, l_app_copy);
      }
      /* if reboot / shutdown unit add deferred functionality in timer file */
      log_debug_message("AL Daemon Method Call Listener : RunAs app: %s\n",
		  l_app);
      if (strstr(l_app, "reboot") != NULL ) {
	SetupUnitFileKey("/lib/systemd/system/reboot.timer",
			 "OnActiveSec", l_time, "reboot");
      }
      if (strstr(l_app, "poweroff") != NULL) {
	SetupUnitFileKey("/lib/systemd/system/poweroff.timer",
			 "OnActiveSec", l_time, "poweroff");
      }
      /* check for application service file existence */
      if (!AppExistsInSystem(l_app)) {
	log_error_message
	    ("AL Daemon Method Call Listener : Cannot runas %s !\n Application %s is not found in the system !\n",
	     l_app, l_app);
	continue;
      } else {
	if ((l_r = (int) AppPidFromName(l_app)) != 0) {
	  log_error_message
	      ("AL Daemon Method Call Listener : Cannot runas %s !\n",
	       l_app);

	  /* check the application current state before starting it */
	  /* extract the application state for testing existence */
          /* make a copy of the name to avoid additional suffix when calling runas */
          strcpy(l_app_copy, l_app);
	  if (AlGetAppState
	      (l_conn, strcat(l_app_copy, ".service"), l_state_info) == 0) {

	    /* copy the state */
	    l_app_status = strdup(l_state_info);

	    /* active state extraction from global state info */
	    l_active_state = strtok(l_app_status, l_delim_serv);
	    l_active_state = strtok(NULL, l_delim_serv);
	    l_active_state = strtok(NULL, l_delim_serv);
	    l_sub_state = strtok(NULL, l_delim_serv);

	  }
	  if (strcmp(l_active_state, "active") == 0) {
	    if ((strcmp(l_sub_state, "exited") != 0)
		|| (strcmp(l_sub_state, "dead") != 0)
		|| (strcmp(l_sub_state, "failed") != 0)) {
	      log_error_message
		  ("AL Daemon Method Call Listener : Cannot runas %s !\n Application %s is already running in the system !\n",
		   l_app, l_app);
	      continue;
	    }
	  }
	}
      }
      /* ensure proper load state for the unit before starting it */
    if (!(l_load_msg =
       dbus_message_new_method_call("org.freedesktop.systemd1",
				    "/org/freedesktop/systemd1",
				    "org.freedesktop.systemd1.Manager",
				    "LoadUnit"))) {
	    log_error_message
		("AL Daemon Method Call Listener : Could not allocate message when loading unit for RunAs ! \n %s \n",
		 l_err.message);
	    if (l_load_msg)
	      dbus_message_unref(l_load_msg);
	    if (l_load_reply)
	      dbus_message_unref(l_load_reply);
	    dbus_error_free(&l_err);
	    continue;
    }
    /* temp to store full service name */
    char *l_full_srv = malloc(DIM_MAX*sizeof(l_full_srv));;
    strcpy(l_full_srv, l_app);
    strcat(l_full_srv, ".service");
    /* append the name of the application to load */
    if (!dbus_message_append_args(l_load_msg,
				 DBUS_TYPE_STRING, &l_full_srv,
                                 DBUS_TYPE_INVALID)) {
                    log_error_message
			("AL Daemon Method Call Listener : Failed to append app name for RunAs when loading : %s \n",
			 l_err.message);
		    if (l_load_msg)
		      dbus_message_unref(l_load_msg);
		    if (l_load_reply)
		      dbus_message_unref(l_load_reply);
		    dbus_error_free(&l_err);
		    continue;
     }
  
    if (!(l_load_reply =
       dbus_connection_send_with_reply_and_block(l_conn, l_load_msg,
						 -1, &l_err))) {
	    log_error_message
		("AL Daemon Method Call Listener : Failed to issue LoadUnit method call for RunAs: %s \n",
		 l_err.message);
	    if (l_load_msg)
	      dbus_message_unref(l_load_msg);
	    if (l_load_reply)
	      dbus_message_unref(l_load_reply);
	    dbus_error_free(&l_err);
	    continue;
      }

      /* runas the application with euid and egid parameters */
      RunAs(l_uid_val, l_gid_val, l_pid, l_fg_state, 0, l_app);
      /* setup foreground property in systemd and wait for reply 
       * the setup is done after starting the application due to 
       * the fact that RunAs issues a daemon-reload that would overwrite
       * the state setup. the time penalty is minimal 
       */
      if(SetupApplicationStartupState(l_conn, l_app, l_fg_state)!=0){
		log_error_message("AL Daemon Method Call Listener : Cannot setup fg/bg state for %s , application will runas %d in former state or default state \n" ,l_app, l_uid_val);
		continue;
      }
    }

    /* check if this is a method call for the right interface amd method */
    if (dbus_message_is_method_call(l_msg, AL_METHOD_INTERFACE, "Stop")) {
      AlReplyToMethodCall(l_msg, l_conn);
      /* extract application name */
      dbus_message_get_args(l_msg, &l_err, DBUS_TYPE_UINT32,
			    &l_pid, DBUS_TYPE_INVALID);
      /* extract application name from pid */
      l_r = (int) AppNameFromPid(l_pid, l_app);
      log_debug_message
	  ("AL Daemon Method Call Listener : Stopping application with pid %d\n",
	   l_pid);
      if (l_r!=1) {
	/* test for application service file existence */
	if (!AppExistsInSystem(l_app)) {
	  log_error_message
	      ("AL Daemon Method Call Listener : Cannot stop %s !\n Application %s is not found in the system !\n",
	       l_app, l_app);
	  continue;
	}
	log_error_message
	      ("AL Daemon Method Call Listener : Cannot stop %s !\n", l_app);
	/* check the application current state before stopping it */
	/* extract the application state for testing existence */
  	/* make a copy of the name to avoid additional postfix when calling stop */
        /* test if the unit is a service and react according to type */
	if(AppExistsInSystem(l_app)==1){
	  strcpy(l_app_copy, l_app);
          if (AlGetAppState(l_conn, strcat(l_app_copy, ".service"), l_state_info)
	    == 0) {

	  /* copy the state */
	  l_app_status = strdup(l_state_info);

	  /* active state extraction from global state info */
	  l_active_state = strtok(l_app_status, l_delim_serv);
	  l_active_state = strtok(NULL, l_delim_serv);
	  l_active_state = strtok(NULL, l_delim_serv);
	  l_sub_state = strtok(NULL, l_delim_serv);
          }
	}
	/* test if the unit is a target and react according to type */
	if(AppExistsInSystem(l_app)==2){
	  strcpy(l_app_copy, l_app);
          if (AlGetAppState(l_conn, strcat(l_app_copy, ".target"), l_state_info)
	    == 0) {

	  /* copy the state */
	  l_app_status = strdup(l_state_info);

	  /* active state extraction from global state info */
	  l_active_state = strtok(l_app_status, l_delim_serv);
	  l_active_state = strtok(NULL, l_delim_serv);
	  l_active_state = strtok(NULL, l_delim_serv);
	  l_sub_state = strtok(NULL, l_delim_serv);
          }
	}
        /* state testing */
	if ((strcmp(l_active_state, "active") != 0)
	    && (strcmp(l_active_state, "reloading") != 0)
	    && (strcmp(l_active_state, "activating") != 0)
	    && (strcmp(l_active_state, "deactivating") != 0)) {

	  log_error_message
	      ("AL Daemon Method Call Listener : Cannot stop %s !\n Application %s is already stopped !\n",
	       l_app, l_app);
	  continue;
	}
      }
       /* test if we have a service that will be stopped */
       if((AppExistsInSystem(l_app))==1){
	/* maintains the command line to pass to systemd */
        char l_cmd[DIM_MAX];
        /* if the name of the service corresponds to the name of the process to start call Stop */
        if(AppPidFromName(l_app)!=0){
		Stop(l_pid);
	}
        /* if the name of the service differs from the name of the process 
           to start (multiple ExecStart clauses service )*/
  	sprintf(l_cmd, "systemctl stop %s.service", l_app);
	l_ret = system(l_cmd);
  	if (l_ret != 0) {
       	log_error_message
	      ("AL Daemon Method Call Listener : Cannot stop %s !\n Application %s is already stopped !\n",
	       l_app, l_app);	
	}
      }
      /* test if we have a target and stop all the applications started by it */
      if(AppExistsInSystem(l_app)==2){
	char l_cmd[DIM_MAX];
	sprintf(l_cmd, "systemctl stop %s.target", l_app);
	l_ret = system(l_cmd);
  	if (l_ret != 0) {
       	log_error_message
	      ("AL Daemon Method Call Listener : Cannot stop %s !\n Application group %s is already stopped !\n",
	       l_app, l_app);	
	}
      }
    }

     /* check if this is a method call for the right interface amd method */
    if (dbus_message_is_method_call(l_msg, AL_METHOD_INTERFACE, "StopAs")) {
      AlReplyToMethodCall(l_msg, l_conn);
      /* extract application name */
      dbus_message_get_args(l_msg, &l_err, 
                            DBUS_TYPE_UINT32, &l_pid,
 			    DBUS_TYPE_INT32, &l_uid_val,
 			    DBUS_TYPE_INT32, &l_gid_val,
			    DBUS_TYPE_INVALID);
      /* extract application name from pid */
      l_r = (int) AppNameFromPid(l_pid, l_app);
      log_debug_message
	  ("AL Daemon Method Call Listener : Stopping application with pid %d using stopas !\n",
	   l_pid);
      if (l_r!=1) {
	/* test for application service file existence */
	if (!AppExistsInSystem(l_app)) {
	  log_error_message
	      ("AL Daemon Method Call Listener : Cannot stopas %s !\n Application %s is not found in the system !\n",
	       l_app, l_app);
	  continue;
	}
	log_error_message
	      ("AL Daemon Method Call Listener : Cannot stopas %s !\n", l_app);
	/* check the application current state before stopping it */
	/* extract the application state for testing existence */
        /* make a copy of the name to avoid additional postfix when calling stopas */
        strcpy(l_app_copy, l_app);
	if (AlGetAppState(l_conn, strcat(l_app_copy, ".service"), l_state_info)
	    == 0) {

	  /* copy the state */
	  l_app_status = strdup(l_state_info);

	  /* active state extraction from global state info */
	  l_active_state = strtok(l_app_status, l_delim_serv);
	  l_active_state = strtok(NULL, l_delim_serv);
	  l_active_state = strtok(NULL, l_delim_serv);
	  l_sub_state = strtok(NULL, l_delim_serv);

	}
        /* state testing */
	if ((strcmp(l_active_state, "active") != 0)
	    && (strcmp(l_active_state, "reloading") != 0)
	    && (strcmp(l_active_state, "activating") != 0)
	    && (strcmp(l_active_state, "deactivating") != 0)) {

	  log_error_message
	      ("AL Daemon Method Call Listener : Cannot stopas %s !\n Application %s is already stopped !\n",
	       l_app, l_app);
	  continue;
	}
      }
      /* stopas the application */
      StopAs(l_pid, l_uid_val, l_gid_val);
    }

    /* check if this is a method call for the right interface amd method */
    if (dbus_message_is_method_call(l_msg, AL_METHOD_INTERFACE, "Resume")) {
      AlReplyToMethodCall(l_msg, l_conn);
      /* extract the application name */
      dbus_message_get_args(l_msg, &l_err, DBUS_TYPE_UINT32,
			    &l_pid, DBUS_TYPE_INVALID);
      /* extract application name from pid */
      l_r = (int) AppNameFromPid(l_pid, l_app);
      log_debug_message
	  ("AL Daemon Method Call Listener : Resuming application %s\n",
	   l_app);
      if (l_r!=1) {
	/* test for application service file existence */
	if (!AppExistsInSystem(l_app)) {
	  log_error_message
	      ("AL Daemon Method Call Listener : Cannot resume %s !\n Application %s is not found in the system !\n",
	       l_app, l_app);
	  continue;
	}
        log_error_message
	      ("AL Daemon Method Call Listener : Cannot resume %s !\n", l_app);
	/* check the application current state before starting it */
	/* extract the application state for testing existence */
        /* make a copy of the name to avoid additional postfix when calling resume */
        strcpy(l_app_copy, l_app);
	if (AlGetAppState(l_conn, strcat(l_app_copy, ".service"), l_state_info)
	    == 0) {

	  /* copy the state */
	  l_app_status = strdup(l_state_info);

	  /* active state extraction from global state info */
	  l_active_state = strtok(l_app_status, l_delim_serv);
	  l_active_state = strtok(NULL, l_delim_serv);
	  l_active_state = strtok(NULL, l_delim_serv);
	  l_sub_state = strtok(NULL, l_delim_serv);

	}

	if (strcmp(l_active_state, "active") == 0) {
	  if ((strcmp(l_sub_state, "exited") != 0)
	      || (strcmp(l_sub_state, "dead") != 0)
	      || (strcmp(l_sub_state, "failed") != 0)) {
	    log_error_message
		("AL Daemon Method Call Listener : Cannot run %s !\n Application %s is already running in the system !\n",
		 l_app, l_app);
	    continue;
	  }
	}
      }
      /* resume the application */
      Resume(l_pid);
    }

    /* check if this is a method call for the right interface amd method */
    if (dbus_message_is_method_call(l_msg, AL_METHOD_INTERFACE, "Suspend")) {
      AlReplyToMethodCall(l_msg, l_conn);
      /* extract the application name */
      dbus_message_get_args(l_msg, &l_err, DBUS_TYPE_UINT32,
			    &l_pid, DBUS_TYPE_INVALID);
      /* extract application name from pid */
      l_r = (int) AppNameFromPid(l_pid, l_app);
      log_debug_message
	  ("AL Daemon Method Call Listener : Suspending application %s\n",
	   l_app);
      if (l_r!=1) {
	/* test for application service file existence */
	if (!AppExistsInSystem(l_app)) {
	  log_error_message
	      ("AL Daemon Method Call Listener : Cannot suspend %s !\n Application %s is not found in the system !\n",
	       l_app, l_app);
	  continue;
	}
	log_error_message
	      ("AL Daemon Method Call Listener : Cannot suspend %s !\n ", l_app);
	/* check the application current state before starting it */
	/* extract the application state for testing existence */
        /* make a copy of the name to avoid additional suffix when calling suspend */
        strcpy(l_app_copy, l_app);
	if (AlGetAppState(l_conn, strcat(l_app_copy, ".service"), l_state_info)
	    == 0) {

	  /* copy the state */
	  l_app_status = strdup(l_state_info);

	  /* active state extraction from global state info */
	  l_active_state = strtok(l_app_status, l_delim_serv);
	  l_active_state = strtok(NULL, l_delim_serv);
	  l_active_state = strtok(NULL, l_delim_serv);
	  l_sub_state = strtok(NULL, l_delim_serv);

	}

	if ((strcmp(l_active_state, "active") != 0)
	    && (strcmp(l_active_state, "reloading") != 0)
	    && (strcmp(l_active_state, "activating") != 0)
	    && (strcmp(l_active_state, "deactivating") != 0)) {

	  log_error_message
	      ("AL Daemon Method Call Listener : Cannot suspend %s !\n Application %s is already suspended !\n",
	       l_app, l_app);
	  continue;
	}
      }
      /* suspend the application */
      Suspend(l_pid);
    }

   /* check if this is a method call for the right interface amd method */
    if (dbus_message_is_method_call(l_msg, AL_METHOD_INTERFACE, "Restart")) {
      AlReplyToMethodCall(l_msg, l_conn);
      /* extract application name and arguments */
      dbus_message_get_args(l_msg, &l_err, 
			    DBUS_TYPE_STRING, &l_app,
			    DBUS_TYPE_INVALID);
      log_debug_message("AL Daemon Method Call Listener : Restart app: %s\n", l_app);
      /* check for application service file existence */
      if (!AppExistsInSystem(l_app)) {
	log_error_message
	    ("AL Daemon Method Call Listener : Cannot restart %s !\n Application %s is not found in the system !\n",
	     l_app, l_app);
	continue;
      } 
      Restart(l_app); 
    }
    
    if (dbus_message_is_method_call(l_msg, AL_METHOD_INTERFACE, "ChangeTaskState")) {
      AlReplyToMethodCall(l_msg, l_conn);
      /* extract application name and arguments */
      dbus_message_get_args(l_msg, &l_err,
                            DBUS_TYPE_INT32, &l_pid,
			    DBUS_TYPE_BOOLEAN, &l_fg_state,
			    DBUS_TYPE_INVALID);

    /* test if the application is stopped */
    if(!l_pid){
	log_error_message("AL Daemon Method Call Listener : Cannot change state because application doesn't exist or is stopped !%s", "\n");
	continue;
    }
    /* extract application name from pid */
      l_r = (int) AppNameFromPid(l_pid, l_app);
      if (l_r!=1) {
	/* test for application service file existence */
	if (!AppExistsInSystem(l_app)) {
	  log_error_message
	      ("AL Daemon Method Call Listener : Cannot change state for %s !\n Application %s is not found in the system !\n",
	       l_app, l_app);
	  continue;
	}
     }
      
      if(AppPidFromName(l_app)!=0){
	log_debug_message("AL Daemon Method Call Listener ChangeTaskState : Setting the state property for %s !\n ", l_app);
      
  /* form the proper unit name to fetch object path */
  strcat(l_app,".service");
  /* get unit object path */ 
  if (NULL == (l_path = GetUnitObjectPath(l_conn, l_app)))
  {
          log_error_message
                  ("AL Daemon ChangeTaskState : Unable to extract object path for %s", l_app);
          continue;
  }
  log_debug_message
          ("AL Daemon ChangeTaskState  : Extracted object path for %s\n",
           l_app);

  /* send state (fg/bg) property setup method call to systemd */
  if (!(l_msg_state =
       dbus_message_new_method_call("org.freedesktop.systemd1",  
				    l_path, 
				    "org.freedesktop.DBus.Properties", 
				    "Set"))) { 

    log_error_message
	("AL Daemon Method Call Listener : Could not allocate message when setting state property to systemd! \n %s \n",
	 l_err.message);
    if (l_msg_state)
      dbus_message_unref(l_msg_state);
    if (l_reply_state)
      dbus_message_unref(l_reply_state);
    dbus_error_free(&l_err);
    continue;
  }
  log_debug_message
	("AL Daemon ChangeTaskState  : Called property set method call for %s\n",
	 l_app);
   /* initialize the iterator for arguments append */
   dbus_message_iter_init_append(l_msg_state, &l_iter);
  log_debug_message
	("AL Daemon ChangeTaskState  : Initialized iterator for property value setup method call for %s\n", l_app);
   /* append interface for property setting */
   char *l_iface = "org.freedesktop.systemd1.Service";
   if(!dbus_message_iter_append_basic(&l_iter, 
				      DBUS_TYPE_STRING, 
				      &l_iface)){
	log_error_message
	("AL Daemon ChangeTaskState : Could not append interface to message for %s \n",
	 l_app);
	continue;
   }
   log_debug_message
	("AL Daemon ChangeTaskState  : Appended interface name for property set for %s\n",
	 l_app);
   /* append property name */
   char *l_prop = "Foreground";
   if(!dbus_message_iter_append_basic(&l_iter, 
				      DBUS_TYPE_STRING, 
				      &l_prop)){ 
	log_error_message
	("AL Daemon ChangeTaskState : Could not append property to message for %s \n",
	 l_app);
	continue;
   }
   log_debug_message
	("AL Daemon ChangeTaskState  : Appended property name to setup for %s\n",
	 l_app);
   /* append the variant that stores the value for the foreground state property */
    if(!dbus_message_iter_open_container(&l_iter, 
				     DBUS_TYPE_VARIANT, 
				     DBUS_TYPE_BOOLEAN_AS_STRING,
				     &l_variant)){
	log_error_message("AL Daemon ChangeTaskState : Not enough memory to open container %s" ,"\n");
	continue;
    }
    log_debug_message
	("AL Daemon ChangeTaskState  : Opened container for property value setup for %s\n",
	 l_app);
    dbus_bool_t l_state = (dbus_bool_t)l_fg_state;
    if(!dbus_message_iter_append_basic (&l_variant, 
				        DBUS_TYPE_BOOLEAN,  
                                        &l_state)){
    	log_error_message
	("AL Daemon ChangeTaskState : Could not append property value to message for %s \n",
	 l_app);
     	continue;
    }
    log_debug_message
	("AL Daemon ChangeTaskState  : Set the state (fg/bg) value in variant for %s\n",
	 l_app);
    if(!dbus_message_iter_close_container (&l_iter, 
					   &l_variant)){
	log_error_message("AL Daemon ChangeTaskState : Not enough memory to close container %s" ,"\n");
	continue;
    }
    log_debug_message
	("AL Daemon ChangeTaskState  : Closed container for property value setup for %s\n",
	 l_app);     
   /* wait for the reply from systemd after setting the state (fg/bg) property */
   if (!(l_reply_state =
       dbus_connection_send_with_reply_and_block(l_conn, l_msg_state,
						 -1, &l_err))) {
    	log_error_message
	("AL Daemon Method Call Listener : Didn't received a reply for state property method call: %s \n",l_err.message);
    if (l_msg_state)
      dbus_message_unref(l_msg_state);
    if (l_reply_state)
      dbus_message_unref(l_reply_state);
    dbus_error_free(&l_err);
    continue;
   }
   
   log_debug_message("AL Daemon Method Call Listener ChangeTaskState : Reply after setting state for %s was received!\n ", l_app);
	
   log_debug_message("AL Daemon Method Call Listener ChangeTaskState : Broadcast state change notification for %s !\n ", l_app);
     
     /* send notification anouncing that an application changed state (fg/bg) 
      * used if needed  
      */	
#if 0     
     AlChangeTaskStateNotifier(l_conn, l_app, (l_fg_state==TRUE)?"TRUE":"FALSE");
#endif

   log_debug_message("AL Daemon Method Call Listener ChangeTaskState : State change notification was sent for %s !\n ", l_app);			
    ChangeTaskState(l_pid, l_fg_state);       
    if (NULL != l_path)
            free(l_path);
    }
 }

    /* consider only property change notification signals */
    if (dbus_message_is_signal
	(l_msg, "org.freedesktop.DBus.Properties", "PropertiesChanged")) {
      /* handling variables for object path, interface and property */
      const char *l_path, *l_interface, *l_property = "Id";
      /* initialize reply iterators */
      DBusMessageIter l_iter, l_sub_iter;

      log_debug_message
	  ("AL Daemon Signal Listener : An application changed state !%s",
	   "\n");
      /* get object path for message */
      l_path = dbus_message_get_path(l_msg);
      /* extract the interface name from message */
      if (!dbus_message_get_args(l_msg, &l_err,
				 DBUS_TYPE_STRING, &l_interface,
				 DBUS_TYPE_INVALID)) {
	log_error_message
	    ("AL Daemon Method Call Listener : Failed to parse message: %s",
	     l_err.message);
	if (l_msg)
	  dbus_message_unref(l_msg);

	dbus_error_free(&l_err);
	continue;
      }
      /* filter only the unit and service specific interfaces */
      if ((strcmp(l_interface, "org.freedesktop.systemd1.Unit") != 0)
	  && (strcmp(l_interface, "org.freedesktop.systemd1.Job") != 0)) { // previously org.freedesktop.systemd1.Service
	if (l_msg)
	  /* free the message */
	  dbus_message_unref(l_msg);
	dbus_error_free(&l_err);
	continue;
      }

      /* new method call to get unit properties */
      if (!
	  (l_msg =
	   dbus_message_new_method_call("org.freedesktop.systemd1", l_path,
					"org.freedesktop.DBus.Properties",
					"Get"))) {
	log_error_message
	    ("AL Daemon Method Call Listener : Could not allocate message for %s !\n",
	     l_path);
	if (l_msg)
	  dbus_message_unref(l_msg);
	dbus_error_free(&l_err);
	continue;
      }
      /* append arguments for the message */
      if (!dbus_message_append_args(l_msg,
				    DBUS_TYPE_STRING, &l_interface,
				    DBUS_TYPE_STRING, &l_property,
				    DBUS_TYPE_INVALID)) {
	log_error_message
	    ("AL Daemon Method Call Listener : Could not append arguments to message for %s !\n",
	     l_path);
	if (l_msg)
	  dbus_message_unref(l_msg);
	dbus_error_free(&l_err);
	continue;
      }
      /* send the message and wait for a reply */
      if (!
	  (l_reply =
	   dbus_connection_send_with_reply_and_block(l_conn,
						     l_msg, -1, &l_err))) {
	log_error_message
	    ("AL Daemon Signal Listener : No reply received. Unable to issue method call: %s",
	     l_err.message);
	if (l_msg)
	  dbus_message_unref(l_msg);
	if (l_reply)
	  dbus_message_unref(l_reply);
	dbus_error_free(&l_err);
	continue;
      }
      /* initialize the reply iterator and check argument type */
      if (!dbus_message_iter_init(l_reply, &l_iter) ||
	  dbus_message_iter_get_arg_type(&l_iter) != DBUS_TYPE_VARIANT) {
	log_error_message
	    ("AL Daemon Signal Call Listener : Failed to parse reply for %s !\n",
	     l_path);
	if (l_msg)
	  dbus_message_unref(l_msg);
	if (l_reply)
	  dbus_message_unref(l_reply);
	dbus_error_free(&l_err);
	continue;
      }
      /* recurse sub iterator */
      dbus_message_iter_recurse(&l_iter, &l_sub_iter);
      /* consider only unit specific signals */
      if (strcmp(l_interface, "org.freedesktop.systemd1.Unit") == 0) {
	const char *l_id;
	/* verify the argument type */
	if (dbus_message_iter_get_arg_type(&l_sub_iter) !=
	    DBUS_TYPE_STRING) {
	  log_error_message
	      ("AL Daemon Method Call Listener : Failed to parse reply for %s !",
	       l_path);
	  if (l_msg)
	    dbus_message_unref(l_msg);
	  if (l_reply)
	    dbus_message_unref(l_reply);
	  dbus_error_free(&l_err);
	  continue;
	}
	/* extract the application id from the signal */
	dbus_message_iter_get_basic(&l_sub_iter, &l_id);
	log_debug_message("AL Daemon Method Call Listener : Unit %s changed.\n",
		    l_id);
        /* notify clients about tasks global state changes */
	AlAppStateNotifier(l_conn, (char *) l_id);
	/* notify about task started/stopped */
	AlSendAppSignal(l_conn, (char *) l_id);
      }
    }
  }
  /* free the messages */
  dbus_message_unref(l_msg);
  dbus_message_unref(l_msg_notif);
  dbus_message_unref(l_msg_state);
}

/* Signal handler for the daemon */
void AlSignalHandler(int p_sig)

{
    switch(p_sig) {
	    case SIGTERM:
		log_debug_message("AL Daemon : Application launcher received TERM signal ...\n", 0);
		log_debug_message("AL Daemon : Application launcher daemon exiting ....\n", 0);
		log_debug_message("AL Daemon : Removing lock file %s \n", AL_PID_FILE);
		remove(AL_PID_FILE);
		log_message("AL Daemon : Daemon exited !\n", 0);
                exit(EXIT_SUCCESS);
		break;
	    case SIGKILL:
		log_debug_message("AL Daemon : Application launcher received KILL signal ...\n", 0);
		break;
 	    default:
		log_debug_message("AL Daemon : Daemon received unhandled signal %s\n!", strsignal(p_sig));
		break;
        }
}

/* Application Launcher Daemon entrypoint */
int main(int argc, char **argv)
{
  /* return code */
  int l_ret;
  /* logging mechanism */
  int log =  LOG_MASK (LOG_ERR) | LOG_MASK (LOG_INFO);
#ifdef DEBUG
  log = log | LOG_MASK(LOG_DEBUG);
#endif

  openlog ("AL-DAEMON", 0x0, LOG_USER);
  setlogmask(log);

  /* handle signals */
  signal(SIGTERM, AlSignalHandler);
  signal(SIGKILL, AlSignalHandler);
  
  /* parse cli options */
  AlParseCLIOptions(argc, argv);

  if (g_stop) {
    AlDaemonShutdown();
    return 0;
  }

  if (g_start) {
    /* daemonize the application launcher */
    AlDaemonize();
    log_message("AL Daemon : Daemon process was started !\n", 0);
    /* initialise the last user mode */
    if(!(l_ret=InitializeLastUserMode())){
      log_error_message("AL Daemon : Last user mode initialization failed !\n", 0);
    }
    else { log_message("AL Daemon : Last user mode initialized. Listening for method calls ....\n", 0);
    }
    /* start main daemon loop */
    AlListenToMethodCall();
  }
  log_message("AL Daemon : Daemon exited !\n", 0);
  /* close logging mechanism */
  closelog ();

  return 0;
}
