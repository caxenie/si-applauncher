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

/* Connection to the system bus */
DBusGConnection *g_conn = NULL;
/* locals to store AL Daemon interface utils */
ALDbus *g_al_dbus = NULL;
DBusGProxy *g_al_proxy = NULL;
/* used for property fetch */
DBusGProxy *d_dbus_proxy = NULL;
/* proxy to interface with systemd */
DBusGProxy *sysd_proxy = NULL;
/* connection to the system bus for notification thread */
DBusConnection * l_conn = NULL;

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


/* Signal handler for the daemon */
void AlSignalHandler(int p_sig)

{
    switch(p_sig) {
	    case SIGTERM:
		log_debug_message("Application launcher received TERM signal ...\n", 0);
		log_debug_message("Application launcher daemon exiting ....\n", 0);
		log_debug_message("Removing lock file %s \n", AL_PID_FILE);
		remove(AL_PID_FILE);
		log_message("Daemon exited !\n", 0);
                exit(EXIT_SUCCESS);
		break;
	    case SIGKILL:
		log_debug_message("Application launcher received KILL signal ...\n", 0);
		break;
 	    default:
		log_debug_message("Daemon received unhandled signal %s\n!", strsignal(p_sig));
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
    log_message("Daemon process was started !\n", 0);
    /* initialise the last user mode */
    if(!(l_ret=InitializeLastUserMode())){
      log_error_message("Last user mode initialization failed !\n", 0);
    }
    else { log_message("Last user mode initialized. Listening for method calls ....\n", 0);
    }
    /* initialize SRM Daemon */
	if(!initialize_al_dbus()){
		log_error_message("Failed to initialize AL Daemon!\n Stopping daemon ...", 0);
		terminate_al_dbus();
		return 1;

	}
	/* start the signal dispatching thread */
	al_dbus_signal_dispatcher();
	/* main loop */
	GMainLoop *l_loop = NULL;
	if(!(l_loop = g_main_loop_new(NULL, FALSE))){
		log_error_message("Error creating main loop !\n", 0);
		exit(1);
	}

	/* run the main loop */
	g_main_loop_run(l_loop);

  }
  log_message("Daemon exited !\n", 0);
  /* close logging mechanism */
  closelog ();

  /* free res */
  terminate_al_dbus();

  return 0;
}
