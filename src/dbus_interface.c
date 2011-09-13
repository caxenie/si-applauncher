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
#include <gio/gio.h>

#include "dbus_interface.h"
#include "utils.h"
#include "lum.h"
#include "notifier.h"
#include "al-daemon.h"
#include "al_dbus-glue.h"
#include "task_info_custom_marshaller.c"
#include "task_state_change_custom_marshaller.c"

/* define the AL Daemon GLib Object */
G_DEFINE_TYPE(ALDbus, al_dbus, G_TYPE_OBJECT);

extern DBusGProxy *g_al_proxy;
extern DBusGProxy *g_dbus_proxy;
extern DBusGConnection *g_conn;
extern ALDbus *g_al_dbus;
extern DBusGProxy *sysd_proxy;
extern DBusConnection * l_conn;

/**
 * Class initialization function.
 */
static void al_dbus_class_init(ALDbusClass * class
			   /**< [in] the class object to initialize. */
    )
{
	/* define the signal for resource usage */

	class->ALSignals[AL_SIG_TASK_STARTED] =
	    g_signal_new("task_started",
			 G_OBJECT_CLASS_TYPE(class),
			 (G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
			 0,
			 NULL,
			 NULL,
			 al_dbus_VOID__INT_STRING,
			 G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_STRING);
	class->ALSignals[AL_SIG_TASK_STOPPED] =
	    g_signal_new("task_stopped",
			 G_OBJECT_CLASS_TYPE(class),
			 (G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
			 0,
			 NULL,
			 NULL,
			 al_dbus_VOID__INT_STRING,
			 G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_STRING);
	class->ALSignals[AL_SIG_CHANGE_STATE_COMPLETE] =
	    g_signal_new("change_task_state_complete",
			 G_OBJECT_CLASS_TYPE(class),
			 (G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
			 0,
			 NULL,
			 NULL,
			 al_dbus_VOID__STRING_STRING,
			 G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_STRING);
	class->ALSignals[AL_SIG_GLOBAL_NOTIFICATION] =
	    g_signal_new("global_state_notification",
			 G_OBJECT_CLASS_TYPE(class),
			 (G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
			 0,
			 NULL,
			 NULL,
			 g_cclosure_marshal_VOID__STRING,
			 G_TYPE_NONE, 1, G_TYPE_STRING);
}

/*
 * Passthrough functions to allow funcitons in other
 * files to destroy a AL Dbus object.
 */

static void al_dbus_finalize(ALDbus * server);

void al_dbus_destroy(ALDbus * server)
{
	al_dbus_finalize(server);
}

/**
 * Instance initialization function.
 */
static void al_dbus_init(ALDbus * server
		       /**< [in] the instance to initialize. */
    )
{
	GError *error = NULL;
	DBusGProxy *proxy = NULL;
	guint request_ret;

	/* Get a proxy to DBus */
	proxy = dbus_g_proxy_new_for_name(g_conn,
					  DBUS_SERVICE_DBUS,
					  DBUS_PATH_DBUS, DBUS_INTERFACE_DBUS);

	/* Register a service name */
	if (!org_freedesktop_DBus_request_name(proxy, AL_METHOD_INTERFACE,
					       0, &request_ret, &error)) {
		log_error_message("Unable to register AL Daemon service: %s\n",
			   error->message);
		exit(-9);
		g_error_free(error);
	} else if (request_ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
		log_error_message("Unable to register AL Daemon service.  Is another"
			   " instance already running?", 0);
		exit(-10);
	} else {
		/* We got the service name, so register the object. */
		dbus_g_object_type_install_info(al_dbus_get_type(),
						&dbus_glib_al_dbus_object_info);

		dbus_g_connection_register_g_object(g_conn,
						    SRM_OBJECT_PATH,
						    G_OBJECT(server));
	}

	g_object_unref(proxy);
}

/*
 * Class destructor function.
 */
static void al_dbus_finalize(ALDbus * server
		       /**< [in] the instance to finalize. */
    )
{
	guint request_ret;
	GError *error = NULL;
	DBusGProxy *proxy = NULL;

	/* Get a proxy to DBus. */
	proxy = dbus_g_proxy_new_for_name(g_conn,
					  DBUS_SERVICE_DBUS,
					  DBUS_PATH_DBUS, DBUS_INTERFACE_DBUS);

	dbus_connection_unregister_object_path(dbus_g_connection_get_connection
					       (g_conn), SRM_OBJECT_PATH);

    /** Release the connection name so that we can own request it again.
     * later if we need.
     */
	if (!org_freedesktop_DBus_release_name(proxy, AL_METHOD_INTERFACE,
					       &request_ret, &error)) {
		log_error_message("Unable to release AL Daemon service: %s\n",
			   error->message);
		g_error_free(error);
	} else if (request_ret != DBUS_RELEASE_NAME_REPLY_RELEASED) {
		log_error_message("Unable to release AL Daemon service.  Is another"
			   " instance already running?", 0);
	}

	g_type_free_instance((GTypeInstance *) server);

	g_object_unref(proxy);

 free_res:

	if (error)
		g_error_free(error);
	if (proxy)
		g_object_unref(proxy);

	return;
}

/* Function responsible to initialize the AL Daemon DBus interface */

gboolean initialize_al_dbus()
{

	gboolean success = TRUE;
	GError *pGError = NULL;
	guint request_ret;
	DBusGProxy *proxy;
	int l_err;

	g_type_init();
	dbus_g_thread_init();

	if (success == TRUE) {
		if (pGError != NULL) {
			g_error_free(pGError);
			pGError = NULL;
		}

		g_conn = dbus_g_bus_get(DBUS_BUS_SYSTEM, &pGError);

		if (!g_conn) {
			log_error_message("Init : Failed to open connection to bus: %s\n",
				   pGError->message);
			g_error_free(pGError);
			success = FALSE;
		}
	}

	if (success) {
		/* creating the al dbus object */
		g_al_dbus = (ALDbus *) g_object_new(al_dbus_get_type(), NULL);

	}

	/* get a proxy to systemd */
	if (!(sysd_proxy = dbus_g_proxy_new_for_name(g_conn,
						     SYSTEMD_SERVICE_NAME,
						     SYSTEMD_PATH,
						     SYSTEMD_INTERFACE))) {
		log_error_message("Failed to get proxy to Systemd !", 0);
		goto free_res;
	}

	if (success == TRUE)
		log_debug_message("The AL Daemon was initialized ...\n", 0);

	return success;

 free_res:
	if (sysd_proxy) {
		g_object_unref(sysd_proxy);
		sysd_proxy = NULL;
	}

	success = FALSE;
	return success;
}

/* Function responsible to cleanup the resources associated with the AL Daemon DBus interface 
 */

gboolean terminate_al_dbus()
{
	log_debug_message("Shutting down the AL Daemon ...\n", 0);

	if (g_al_proxy) {
		g_object_unref(g_al_proxy);
		g_al_proxy = NULL;
	}
	if (g_al_dbus) {
		g_object_unref(g_al_dbus);
		g_al_dbus = NULL;
	}
	if (g_conn) {
		dbus_g_connection_unref(g_conn);
		g_conn = NULL;
	}
  
        if(l_conn){
		dbus_connection_unref(l_conn);
		l_conn = NULL;
	}
	log_debug_message("The AL Daemon was terminated ...\n", 0);
	return TRUE;
}

/* API method calls */

gboolean al_dbus_run(ALDbus * server,
		     gchar * command_line,
		     gint parent_pid,
		     gboolean foreground, DBusGMethodInvocation * context)
{

	gboolean success = TRUE;
	/* return code */
	int l_r;
	/* new pid of the app */
	int l_new_pid;
	/* additional parameter processing and handling */
	char *command_line_copy = malloc(sizeof(command_line));
	/* used when extracting the deferred execution time */
	char *command_line_deferred = malloc(sizeof(command_line));
	/* time until deferred triggering */
	char *l_time = NULL;
	/* app state info container */
	char l_state_info[DIM_MAX];
	/* application status string copy */
	char *l_state_info_copy = NULL;
	/* active state and sub state containers */
	char *l_active_state = NULL;
	char *l_sub_state = NULL;
	/* standard delimiter to use in service handling */
	char l_delim_serv[] = " ";
	/* error handler for dbus calls */
	GError *l_err = NULL;
	/* handler for DBusConnection from DBusGConnection */
	DBusConnection *l_conn =
	    (DBusConnection *) dbus_g_connection_get_connection(g_conn);
	log_debug_message
	    ("Method Call Listener Run: Arguments were extracted for %s\n",
	     command_line);
	/* check command line name for deferred binaries */
	if ((strstr(command_line, "reboot") != NULL)
	    || (strstr(command_line, "poweroff") != NULL)) {
		/* make a copy of the string because will be altered */
		strcpy(command_line_copy, command_line);
		/* throw away the reboot/poweroff command name */
		command_line_deferred = strtok(command_line_copy, " ");
		/* extract the timing */
		l_time = strtok(NULL, " ");
		/* restore the command string for future use */
		strcpy(command_line, command_line_copy);
	/* if reboot / shutdown unit add deferred functionality in timer file */
	if (strstr(command_line, "reboot") != NULL) {
		SetupUnitFileKey("/lib/systemd/system/reboot.timer",
				 "OnActiveSec", l_time, "reboot");
	}
	if (strstr(command_line, "poweroff") != NULL) {
		SetupUnitFileKey("/lib/systemd/system/poweroff.timer",
				 "OnActiveSec", l_time, "poweroff");
		}
        }
	/* check for application service file existence */
	if (AppExistsInSystem(command_line)==0) {
		log_error_message
		    ("Method Call Listener : Cannot run %s !\n Application %s is not found in the system !\n",
		     command_line, command_line);
			/* resources free */
		if(command_line_copy){
			free(command_line_copy);
			command_line_copy = NULL;
		}
		l_new_pid = 0;
		dbus_g_method_return(context, l_new_pid);
		goto free_res;
	} else {
		if ((l_r = (int)AppPidFromName(command_line)) != 0) {
			log_error_message
			    ("Method Call Listener : Cannot run %s !\n",
			     command_line);
			/* check the application current state before starting it */
			/* extract the application state for testing existence */
			/* make a copy of the name to avoid additional postfix when calling run */
			/* concatenate the appropiate string according the unit type i.e. service/target */
			if (AppExistsInSystem(command_line) == 1) {
				log_debug_message("Unit %s is service\n", command_line);
				strcpy(command_line_copy, command_line);
				if (AlGetAppState
				    (l_conn,
				     strcat(command_line_copy, ".service"),
				     l_state_info) == 0) {
					log_debug_message("Fetched state for service %s \n", command_line);
					/* copy the state */
					l_state_info_copy =
					    strdup(l_state_info);
					/* allocate state handlers */
					l_active_state =
					    malloc(sizeof(l_state_info_copy));
					l_sub_state =
					    malloc(sizeof(l_state_info_copy));

					/* active state extraction from global state info */
					l_active_state =
					    strtok(l_state_info_copy,
						   l_delim_serv);
					l_active_state =
					    strtok(NULL, l_delim_serv);
					l_active_state =
					    strtok(NULL, l_delim_serv);
					l_sub_state =
					    strtok(NULL, l_delim_serv);
				}
				else {
					log_error_message("Failed to fetch app state for service %s\n", command_line);
						/* resources free */
					if(command_line_copy){
						free(command_line_copy);
						command_line_copy = NULL;
					}
					l_new_pid = (int)AppPidFromName(command_line);
					dbus_g_method_return(context, l_new_pid);
					goto free_res;
				}
			}else if (AppExistsInSystem(command_line) == 2) {
				log_debug_message("Unit %s is target\n", command_line);
				strcpy(command_line_copy, command_line);
				if (AlGetAppState
				    (l_conn,
				     strcat(command_line_copy, ".target"),
				     l_state_info) == 0) {
					log_debug_message("Fetched state for target %s \n", command_line);
					/* copy the state */
					l_state_info_copy =
					    strdup(l_state_info);
					l_active_state =
					    malloc(sizeof(l_state_info_copy));
					l_sub_state =
					    malloc(sizeof(l_state_info_copy));

					/* active state extraction from global state info */
					l_active_state =
					    strtok(l_state_info_copy,
						   l_delim_serv);
					l_active_state =
					    strtok(NULL, l_delim_serv);
					l_active_state =
					    strtok(NULL, l_delim_serv);
					l_sub_state =
					    strtok(NULL, l_delim_serv);
				}
			 	else {
					log_error_message("Failed to fetch app state for target %s\n", command_line);
						/* resources free */
					if(command_line_copy){
						free(command_line_copy);
						command_line_copy = NULL;
					}
					l_new_pid = (int)AppPidFromName(command_line);
					dbus_g_method_return(context, l_new_pid);
					goto free_res;
				}
			} else {
				log_error_message("Invalid unit state for %s\n", command_line);
						/* resources free */
					if(command_line_copy){
						free(command_line_copy);
						command_line_copy = NULL;
					}
					l_new_pid = (int)AppPidFromName(command_line);
					dbus_g_method_return(context, l_new_pid);
					goto free_res;
			}
			log_debug_message("Test complete active state for service %s \n", command_line);
			if (strcmp(l_active_state, "active") == 0) {
				if ((strcmp(l_sub_state, "exited") != 0)
				    || (strcmp(l_sub_state, "dead") != 0)
				    || (strcmp(l_sub_state, "failed") != 0)) {
					log_error_message
					    ("Method Call Listener : Cannot run %s !\n Application/application group %s is already running in the system !\n", command_line, command_line);
				}
		}
		log_debug_message("Freeing the resources if the app %s is already running ... \n", command_line);
		l_new_pid = (int)AppPidFromName(command_line);
		dbus_g_method_return(context, l_new_pid);
		goto free_res;
	       }
        }
	log_debug_message("Preparing to start application %s \n", command_line);
	/* ensure proper load state for the unit before starting it */
	char *l_service_path = NULL;
	/* temp to store full service name */
	char *l_full_srv = malloc(sizeof(l_state_info));
	strcpy(l_full_srv, command_line);
	if ((strstr(command_line, "reboot") != NULL)
	    || (strstr(command_line, "poweroff") != NULL)) {
				strcat(l_full_srv, ".timer");
	} else strcat(l_full_srv, ".service");
	/* load unit info */
	if (!dbus_g_proxy_call(sysd_proxy,
			       "LoadUnit",
			       &l_err,
			       G_TYPE_STRING,
			       l_full_srv,
			       G_TYPE_INVALID,
			       DBUS_TYPE_G_OBJECT_PATH,
			       &l_service_path, G_TYPE_INVALID)) {
		log_error_message("Method call failed: %s\n", l_err->message);
		l_new_pid = 0;
		dbus_g_method_return(context, l_new_pid);
		goto free_res;
	}
	/* free full service name string */
	if(l_full_srv)
		free(l_full_srv);
	/* setup foreground property in systemd and wait for reply */
	if (SetupApplicationStartupState(l_conn, command_line, foreground) != 0) {
		log_error_message
		    ("Method Call Listener : Cannot setup fg/bg state for %s , application will run in former state or default state \n",
		     command_line);
	}
	/* call the Run command */
	Run(command_line, parent_pid, foreground);
	l_new_pid = (int)AppPidFromName(command_line);
	log_debug_message("Called Run  : [ %s | %s ]\n", command_line,
		    (foreground == true) ? "true" : "false");
	dbus_g_method_return(context, l_new_pid);

free_res:
	if (l_err)
		g_error_free(l_err);
	/* free aux string */
	if(command_line_deferred){
		free(command_line_deferred);
		command_line_deferred = NULL;
	  }
	return success;

}

gboolean al_dbus_run_as(ALDbus * server,
			gchar * command_line,
			gint parent_pid,
			gboolean foreground,
			gint app_uid,
			gint app_gid, DBusGMethodInvocation * context)
{

	gboolean success = TRUE;
	/* return code */
	int l_r;
	/* new app pid */
	int l_new_pid;
	/* additional parameter processing and handling */
	char *command_line_copy = malloc(sizeof(command_line));
	/* used when extracting the deferred execution time */
	char *command_line_deferred = malloc(sizeof(command_line));
	/* time until deferred triggering */
	char *l_time = NULL;
	/* app state info container */
	char l_state_info[DIM_MAX];
	/* application status string copy */
	char *l_state_info_copy = NULL;
	/* active state and sub state containers */
	char *l_active_state = NULL;
	char *l_sub_state = NULL;
	/* standard delimiter to use in service handling */
	char l_delim_serv[] = " ";
	/* error handler for dbus calls */
	GError *l_err = NULL;
	/* handler for DBusConnection from DBusGConnection */
	DBusConnection *l_conn =
	    (DBusConnection *) dbus_g_connection_get_connection(g_conn);
	log_debug_message
	    ("Method Call Listener RunAs: Arguments were extracted for %s\n",
	     command_line);
	/* check command line name for deferred binaries */
	if ((strstr(command_line, "reboot") != NULL)
	    || (strstr(command_line, "poweroff") != NULL)) {
		/* make a copy of the string because will be altered */
		strcpy(command_line_copy, command_line);
		/* throw away the reboot/poweroff command name */
		command_line_deferred = strtok(command_line_copy, " ");
		/* extract the timing */
		l_time = strtok(NULL, " ");
		/* restore the command string for future use */
		strcpy(command_line, command_line_copy);
	/* if reboot / shutdown unit add deferred functionality in timer file */
	if (strstr(command_line, "reboot") != NULL) {
		SetupUnitFileKey("/lib/systemd/system/reboot.timer",
				 "OnActiveSec", l_time, "reboot");
	}
	if (strstr(command_line, "poweroff") != NULL) {
		SetupUnitFileKey("/lib/systemd/system/poweroff.timer",
				 "OnActiveSec", l_time, "poweroff");
		}
	}
	/* check for application service file existence */
	if (AppExistsInSystem(command_line)==0) {
		log_error_message
		    ("Method Call Listener : Cannot runas %s !\n Application %s is not found in the system !\n",
		     command_line, command_line);
			/* resources free */
		if(command_line_copy){
			free(command_line_copy);
			command_line_copy = NULL;
		}
		l_new_pid = 0;
		dbus_g_method_return(context, l_new_pid);
		goto free_res;
	} else {
		if ((l_r = (int)AppPidFromName(command_line)) != 0) {
			log_error_message
			    ("Method Call Listener : Cannot runas %s !\n",
			     command_line);
			/* check the application current state before starting it */
			/* extract the application state for testing existence */
			/* make a copy of the name to avoid additional postfix when calling run */
			/* concatenate the appropiate string according the unit type i.e. service/target */
			if (AppExistsInSystem(command_line) == 1) {
				log_debug_message("Unit %s is service\n", command_line);
				strcpy(command_line_copy, command_line);
				if (AlGetAppState
				    (l_conn,
				     strcat(command_line_copy, ".service"),
				     l_state_info) == 0) {
					log_debug_message("Fetched state for service %s \n", command_line);
					/* copy the state */
					l_state_info_copy =
					    strdup(l_state_info);
					/* allocate state handlers */
					l_active_state =
					    malloc(sizeof(l_state_info_copy));
					l_sub_state =
					    malloc(sizeof(l_state_info_copy));

					/* active state extraction from global state info */
					l_active_state =
					    strtok(l_state_info_copy,
						   l_delim_serv);
					l_active_state =
					    strtok(NULL, l_delim_serv);
					l_active_state =
					    strtok(NULL, l_delim_serv);
					l_sub_state =
					    strtok(NULL, l_delim_serv);
				}
				else {
					log_error_message("Failed to fetch app state for service %s\n", command_line);
						/* resources free */
					if(command_line_copy){
						free(command_line_copy);
						command_line_copy = NULL;
					}
					l_new_pid = (int)AppPidFromName(command_line);
					dbus_g_method_return(context, l_new_pid);
					goto free_res;
				}
			}else if (AppExistsInSystem(command_line) == 2) {
				log_debug_message("Unit %s is target\n", command_line);
				strcpy(command_line_copy, command_line);
				if (AlGetAppState
				    (l_conn,
				     strcat(command_line_copy, ".target"),
				     l_state_info) == 0) {
					log_debug_message("Fetched state for target %s \n", command_line);
					/* copy the state */
					l_state_info_copy =
					    strdup(l_state_info);
					l_active_state =
					    malloc(sizeof(l_state_info_copy));
					l_sub_state =
					    malloc(sizeof(l_state_info_copy));

					/* active state extraction from global state info */
					l_active_state =
					    strtok(l_state_info_copy,
						   l_delim_serv);
					l_active_state =
					    strtok(NULL, l_delim_serv);
					l_active_state =
					    strtok(NULL, l_delim_serv);
					l_sub_state =
					    strtok(NULL, l_delim_serv);
				}
			 	else {
					log_error_message("Failed to fetch app state for target %s\n", command_line);
						/* resources free */
					if(command_line_copy){
						free(command_line_copy);
						command_line_copy = NULL;
					}
					l_new_pid = (int)AppPidFromName(command_line);
					dbus_g_method_return(context, l_new_pid);
					goto free_res;
				}
			} else {
				log_error_message("Invalid unit state for %s\n", command_line);
						/* resources free */
					if(command_line_copy){
						free(command_line_copy);
						command_line_copy = NULL;
					}
					l_new_pid = (int)AppPidFromName(command_line);
					dbus_g_method_return(context, l_new_pid);
					goto free_res;
			}
			log_debug_message("Test complete active state for service %s \n", command_line);
			if (strcmp(l_active_state, "active") == 0) {
				if ((strcmp(l_sub_state, "exited") != 0)
				    || (strcmp(l_sub_state, "dead") != 0)
				    || (strcmp(l_sub_state, "failed") != 0)) {
					log_error_message
					    ("Method Call Listener : Cannot run %s !\n Application/application group %s is already running in the system !\n", command_line, command_line);
				}
		}
		log_debug_message("Freeing the resources if the app %s is already running ... \n", command_line);
		l_new_pid = (int)AppPidFromName(command_line);
		dbus_g_method_return(context, l_new_pid);
		goto free_res;
	       }
        }
	log_debug_message("Preparing to start application %s \n", command_line);
	/* ensure proper load state for the unit before starting it */
	char *l_service_path = NULL;
	/* temp to store full service name */
	char *l_full_srv = malloc(sizeof(l_state_info));
	strcpy(l_full_srv, command_line);
	if ((strstr(command_line, "reboot") != NULL)
	    || (strstr(command_line, "poweroff") != NULL)) {
				strcat(l_full_srv, ".timer");
	} else strcat(l_full_srv, ".service");
	/* load unit info */
	if (!dbus_g_proxy_call(sysd_proxy,
			       "LoadUnit",
			       &l_err,
			       G_TYPE_STRING,
			       l_full_srv,
			       G_TYPE_INVALID,
			       DBUS_TYPE_G_OBJECT_PATH,
			       &l_service_path, G_TYPE_INVALID)) {
		g_print("[%s] Method call failed: %s\n", __func__,
			l_err->message);
		l_new_pid = 0;
		dbus_g_method_return(context, l_new_pid);
		goto free_res;
	}
	/* free the full service name string */
	if(l_full_srv)
		free(l_full_srv);
	log_debug_message("Called RunAs : [ %s | %s | %d | %d ]\n", command_line,
		    (foreground == TRUE) ? "true" : "false", app_uid, app_gid);
	RunAs(command_line, parent_pid, foreground, app_uid, app_gid);
	/* setup the internal state information for the application */
	if (SetupApplicationStartupState(l_conn, command_line, foreground) != 0) {
		log_error_message
		    ("Method Call Listener : Cannot setup fg/bg state for %s , application will runas %d in former state or default state \n",
		     command_line, app_uid);
		l_new_pid = (int)AppPidFromName(command_line);
		dbus_g_method_return(context, l_new_pid);
		goto free_res;
	}
	l_new_pid = (int)AppPidFromName(command_line);
	al_dbus_task_started(g_al_dbus, l_new_pid, command_line);
	dbus_g_method_return(context, l_new_pid);

free_res:
	if (l_err)
		g_error_free(l_err);
	/* free aux string */
	if(command_line_deferred){
		free(command_line_deferred);
		command_line_deferred = NULL;
	  }
	return success;
}

gboolean al_dbus_stop(ALDbus * server,
		      gint app_pid, DBusGMethodInvocation * context)
{

	/* callback return */
	gboolean success = TRUE;
	/* return code */
	int l_r, l_ret;
	/* application name */
	char *l_app = malloc(DIM_MAX * sizeof(l_app));
	char *l_app_copy = malloc(DIM_MAX * sizeof(l_app));
	/* variables for state extraction */
	char *l_app_status;
	/* app state info container */
	char l_state_info[DIM_MAX];
	/* application status string copy */
	char *l_state_info_copy = NULL;
	/* active state and sub state containers */
	char *l_active_state = NULL;
	char *l_sub_state = NULL;
	/* standard delimiter to use in service handling */
	char l_delim_serv[] = " ";
	/* handler for DBusConnection from DBusGConnection */
	DBusConnection *l_conn =
	    (DBusConnection *) dbus_g_connection_get_connection(g_conn);
	/* extract application name from pid */
	l_r = (int)AppNameFromPid(app_pid, l_app);
	log_debug_message
	    ("Method Call Listener : Stopping application with pid %d\n",
	     app_pid);
	if (l_r != 1) {
		/* test for application service file existence */
		if (!AppExistsInSystem(l_app)) {
			log_error_message
			    ("Method Call Listener : Cannot stop %s !\n Application %s is not found in the system !\n",
			     l_app, l_app);
			dbus_g_method_return(context);
			goto free_res;
		}
	   log_error_message
		    ("Method Call Listener : Cannot stop %s !\n",
		     l_app);
	   dbus_g_method_return(context);
	   goto free_res;
	}
		/* check the application current state before stopping it */
		/* extract the application state for testing existence */
		/* make a copy of the name to avoid additional postfix when calling stop */
		/* test if the unit is a service and react according to type */
		if (AppExistsInSystem(l_app) == 1) {
			strcpy(l_app_copy, l_app);
			if (AlGetAppState
			    (l_conn, strcat(l_app_copy, ".service"),
			     l_state_info)
			    == 0) {
				/* copy the state */
				l_app_status = strdup(l_state_info);
				/* active state extraction from global state info */
				l_active_state =
				    strtok(l_app_status, l_delim_serv);
				l_active_state = strtok(NULL, l_delim_serv);
				l_active_state = strtok(NULL, l_delim_serv);
				l_sub_state = strtok(NULL, l_delim_serv);
			}
		} else if (AppExistsInSystem(l_app) == 2) {
			strcpy(l_app_copy, l_app);
			if (AlGetAppState
			    (l_conn, strcat(l_app_copy, ".target"),
			     l_state_info)
			    == 0) {
				/* copy the state */
				l_app_status = strdup(l_state_info);
				/* active state extraction from global state info */
				l_active_state =
				    strtok(l_app_status, l_delim_serv);
				l_active_state = strtok(NULL, l_delim_serv);
				l_active_state = strtok(NULL, l_delim_serv);
				l_sub_state = strtok(NULL, l_delim_serv);
			}
		} else {
			log_error_message("Cannot determine unit type and cannot extract state\n", 0);
			dbus_g_method_return(context);
		        goto free_res;
		}
		/* state testing */
		if ((strcmp(l_active_state, "active") != 0)
		    && (strcmp(l_active_state, "reloading") != 0)
		    && (strcmp(l_active_state, "activating") != 0)
		    && (strcmp(l_active_state, "deactivating") != 0)) {
			log_error_message
			    ("AL Daemon Method Call Listener : Cannot stop %s !\n Application %s is already stopped !\n",
			     l_app, l_app);
			dbus_g_method_return(context);
			goto free_res;
		}
	/* test if we have a service that will be stopped */
	if ((AppExistsInSystem(l_app)) == 1) {
		/* maintains the command line to pass to systemd */
		char l_cmd[DIM_MAX];
		/* if the name of the service corresponds to the name of the process to start call Stop */
		if (AppPidFromName(l_app) != 0) {
			Stop(app_pid);
			dbus_g_method_return(context);
			return success;
		}
		/* if the name of the service differs from the name of the process 
		   to start (multiple ExecStart clauses service ) */
		sprintf(l_cmd, "systemctl stop %s.service", l_app);
		l_ret = system(l_cmd);
		if (l_ret != 0) {
			log_error_message
			    ("Method Call Listener : Cannot stop %s !\n Application %s is already stopped !\n",
			     l_app, l_app);
			dbus_g_method_return(context);
			goto free_res;
		}
		dbus_g_method_return(context);
		return success;
	}
	/* test if we have a target and stop all the applications started by it */
	if (AppExistsInSystem(l_app) == 2) {
		char l_cmd[DIM_MAX];
		sprintf(l_cmd, "systemctl stop %s.target", l_app);
		l_ret = system(l_cmd);
		if (l_ret != 0) {
			log_error_message
			    ("Method Call Listener : Cannot stop %s !\n Application group %s is already stopped !\n",
			     l_app, l_app);
			dbus_g_method_return(context);
			goto free_res;
		}
		dbus_g_method_return(context);
		return success;
	}

free_res:
	if(l_app)
		free(l_app);
	if(l_app_copy)
		free(l_app_copy);

	return success;

}

gboolean al_dbus_resume(ALDbus * server,
			gint app_pid, DBusGMethodInvocation * context)
{

	gboolean success = TRUE;
	/* return code */
	int l_r;
	/* application name */
	char *l_app = malloc(DIM_MAX * sizeof(l_app));
	char *l_app_copy = malloc(DIM_MAX * sizeof(l_app));
	/* app state info container */
	char l_state_info[DIM_MAX];
	/* variables for state extraction */
	char *l_app_status;
	/* application status string copy */
	char *l_state_info_copy = NULL;
	/* active state and sub state containers */
	char *l_active_state = NULL;
	char *l_sub_state = NULL;
	/* standard delimiter to use in service handling */
	char l_delim_serv[] = " ";
	/* handler for DBusConnection from DBusGConnection */
	DBusConnection *l_conn =
	    (DBusConnection *) dbus_g_connection_get_connection(g_conn);
	/* extract application name from pid */
	l_r = (int)AppNameFromPid(app_pid, l_app);
	log_debug_message
	    ("Method Call Listener : Resuming application %s\n",
	     l_app);
	if (l_r != 1) {
		/* test for application service file existence */
		if (!AppExistsInSystem(l_app)) {
			log_error_message
			    ("Method Call Listener : Cannot resume %s !\n Application %s is not found in the system !\n",
			     l_app, l_app);
			goto free_res;
		}
		log_error_message
		    ("Method Call Listener : Cannot resume %s !\n",
		     l_app);
		/* check the application current state before starting it */
		/* extract the application state for testing existence */
		/* make a copy of the name to avoid additional postfix when calling resume */
		strcpy(l_app_copy, l_app);
		if (AlGetAppState
		    (l_conn, strcat(l_app_copy, ".service"), l_state_info)
		    == 0) {

			/* copy the state */
			l_app_status = strdup(l_state_info);

			/* active state extraction from global state info */
			l_active_state = strtok(l_app_status, l_delim_serv);
			l_active_state = strtok(NULL, l_delim_serv);
			l_active_state = strtok(NULL, l_delim_serv);
			l_sub_state = strtok(NULL, l_delim_serv);

		} else {
			log_error_message("Cannot extract unit information\n", 0);
			goto free_res;
		}
		if (strcmp(l_active_state, "active") == 0) {
			if ((strcmp(l_sub_state, "exited") != 0)
			    || (strcmp(l_sub_state, "dead") != 0)
			    || (strcmp(l_sub_state, "failed") != 0)) {
				log_error_message
				    ("Method Call Listener : Cannot run %s !\n Application %s is already running in the system !\n",
				     l_app, l_app);
				goto free_res;
			}
		}
	}
	Resume(app_pid);
	log_debug_message("Called Resume : [%d] \n", app_pid);

free_res:
	if(l_app)
		free(l_app);
	if(l_app_copy)
		free(l_app_copy);

	dbus_g_method_return(context);

	return success;

}

gboolean al_dbus_suspend(ALDbus * server,
			 gint app_pid, DBusGMethodInvocation * context)
{

	gboolean success = TRUE;
	/* return code */
	int l_r;
	/* application name */
	char *l_app = malloc(DIM_MAX * sizeof(l_app));
	char *l_app_copy = malloc(DIM_MAX * sizeof(l_app));
	/* variables for state extraction */
	char *l_app_status;
	/* app state info container */
	char l_state_info[DIM_MAX];
	/* application status string copy */
	char *l_state_info_copy = NULL;
	/* active state and sub state containers */
	char *l_active_state = NULL;
	char *l_sub_state = NULL;
	/* standard delimiter to use in service handling */
	char l_delim_serv[] = " ";
	/* handler for DBusConnection from DBusGConnection */
	DBusConnection *l_conn =
	    (DBusConnection *) dbus_g_connection_get_connection(g_conn);
	/* extract application name from pid */
	l_r = (int)AppNameFromPid(app_pid, l_app);
	log_debug_message
	    ("Method Call Listener : Suspending application %s\n",
	     l_app);
	if (l_r != 1) {
		/* test for application service file existence */
		if (!AppExistsInSystem(l_app)) {
			log_error_message
			    ("Method Call Listener : Cannot suspend %s !\n Application %s is not found in the system !\n",
			     l_app, l_app);
			goto free_res;
		}
		log_error_message
		    ("Method Call Listener : Cannot suspend %s !\n ",
		     l_app);
		/* check the application current state before starting it */
		/* extract the application state for testing existence */
		/* make a copy of the name to avoid additional suffix when calling suspend */
		strcpy(l_app_copy, l_app);
		if (AlGetAppState
		    (l_conn, strcat(l_app_copy, ".service"), l_state_info)
		    == 0) {

			/* copy the state */
			l_app_status = strdup(l_state_info);

			/* active state extraction from global state info */
			l_active_state = strtok(l_app_status, l_delim_serv);
			l_active_state = strtok(NULL, l_delim_serv);
			l_active_state = strtok(NULL, l_delim_serv);
			l_sub_state = strtok(NULL, l_delim_serv);

		} else {
			log_error_message("Cannot extract unit information\n", 0);
			goto free_res;
		}

		if ((strcmp(l_active_state, "active") != 0)
		    && (strcmp(l_active_state, "reloading") != 0)
		    && (strcmp(l_active_state, "activating") != 0)
		    && (strcmp(l_active_state, "deactivating") != 0)) {

			log_error_message
			    ("Method Call Listener : Cannot suspend %s !\n Application %s is already suspended !\n",
			     l_app, l_app);
			goto free_res;
		}
	}
	Suspend(app_pid);
	log_debug_message("Called Suspend : [%d] \n", app_pid);

free_res:
	if(l_app)
		free(l_app);
	if(l_app_copy)
		free(l_app_copy);

	dbus_g_method_return(context);

	return success;
}

gboolean al_dbus_stop_as(ALDbus * server,
			 gint app_pid,
			 gint app_uid,
			 gint app_gid, DBusGMethodInvocation * context)
{

	gboolean success = TRUE;
	/* return code */
	int l_r;
	/* application name */
	char *l_app = malloc(DIM_MAX*sizeof(l_app));
	char *l_app_copy = malloc(DIM_MAX*sizeof(l_app));;
	/* application status */
	char *l_app_status = malloc(DIM_MAX*sizeof(l_app_status));
	/* app state info container */
	char l_state_info[DIM_MAX];
	/* application status string copy */
	char *l_state_info_copy = NULL;
	/* active state and sub state containers */
	char *l_active_state = NULL;
	char *l_sub_state = NULL;
	/* standard delimiter to use in service handling */
	char l_delim_serv[] = " ";
	/* handler for DBusConnection from DBusGConnection */
	DBusConnection *l_conn =
	    (DBusConnection *) dbus_g_connection_get_connection(g_conn);
	/* extract application name from pid */
	l_r = (int)AppNameFromPid(app_pid, l_app);
	log_debug_message
	    ("Method Call Listener : Stopping application with pid %d using stopas !\n",
	     app_pid);
	if (l_r != 1) {
		/* test for application service file existence */
		if (!AppExistsInSystem(l_app)) {
			log_error_message
			    ("Method Call Listener : Cannot stop %s using stopas ! Application is not found in the system !\n",
			     l_app);
			goto free_res;
		}
		log_error_message
		    ("Method Call Listener : Cannot stopas %s !\n",
		     l_app);
		/* check the application current state before stopping it */
		/* extract the application state for testing existence */
		/* make a copy of the name to avoid additional postfix when calling stopas */
		strcpy(l_app_copy, l_app);
		if (AlGetAppState
		    (l_conn, strcat(l_app_copy, ".service"), l_state_info)
		    == 0) {

			/* copy the state */
			l_app_status = strdup(l_state_info);

			/* active state extraction from global state info */
			l_active_state = strtok(l_app_status, l_delim_serv);
			l_active_state = strtok(NULL, l_delim_serv);
			l_active_state = strtok(NULL, l_delim_serv);
			l_sub_state = strtok(NULL, l_delim_serv);

		} else {
			log_error_message("Cannot extract unit information\n", 0);
			goto free_res;
		}
		/* state testing */
		if ((strcmp(l_active_state, "active") != 0)
		    && (strcmp(l_active_state, "reloading") != 0)
		    && (strcmp(l_active_state, "activating") != 0)
		    && (strcmp(l_active_state, "deactivating") != 0)) {

			log_error_message
			    ("Method Call Listener : Cannot stopas %s !\n Application %s is already stopped !\n",
			     l_app, l_app);
			goto free_res;
		}
	}
	/* stopas the application */
	StopAs(app_pid, app_uid, app_gid);
	log_debug_message("Called Stopas : [%d | %d | %d ] \n", app_pid, app_uid,
		    app_gid);

free_res:
	if(l_app)
		free(l_app);
	if(l_app_copy)
		free(l_app_copy);
	if(l_app_status)
		free(l_app_status);

	dbus_g_method_return(context);

	return success;
}

gboolean al_dbus_restart(ALDbus * server,
			 gchar * app_name, DBusGMethodInvocation * context)
{

	gboolean success = TRUE;
	log_debug_message("Method Call Listener : Restart app: %s\n",
			  app_name);
	/* check for application service file existence */
	if (!AppExistsInSystem(app_name)) {
		log_error_message
		    ("Method Call Listener : Cannot restart %s !\n Application %s is not found in the system !\n",
		     app_name, app_name);
		goto free_res;
	}
	Restart(app_name);
	log_debug_message("Called Restart : [%s] \n", app_name);

free_res:
	dbus_g_method_return(context);

	return success;
}

gboolean al_dbus_change_task_state(ALDbus * server,
				   gint app_pid,
				   gboolean foreground,
				   DBusGMethodInvocation * context)
{

	/* callback return code */
	gboolean success = TRUE;
	/* application path */
	char *l_path;
	/* application name */
	char *l_app_name = malloc(DIM_MAX*sizeof(l_app_name));
	/* return code */
	int l_ret;
	/* error for method calls */
	GError * l_err = NULL;
	/* interface to get the properties */
	const gchar * l_interface;
	/* proxy to set the foreground property */
	DBusGProxy *l_prop_proxy = NULL;
	/* variant to contain the state to set */
	GValue l_set_value = {0,};
	g_value_init(&l_set_value, G_TYPE_BOOLEAN);
	g_value_set_boolean(&l_set_value, foreground);
	/* get app name */
	l_ret = AppNameFromPid(app_pid, l_app_name);
	if (l_ret!=1) {
	/* test for application service file existence */
	if (!AppExistsInSystem(l_app_name)) {
	  log_error_message
	      ("Change Task State : Cannot change state for %s !\n Application %s is not found in the system !\n",
	       l_app_name, l_app_name);
	  goto free_res;
	   }
        }
	if (NULL == (l_path = GetUnitObjectPath((DBusConnection*)dbus_g_connection_get_connection(g_conn), strcat(l_app_name, ".service"))))
	  {
          log_error_message
                  ("Change Task State : Unable to extract object path for %s", l_app_name);
          goto free_res;
  	}
	/* get the interface for the current service */
	l_interface = GetInterfaceFromPath(l_path);
	/* get a proxy to set the property */
	if (!(l_prop_proxy = dbus_g_proxy_new_for_name(g_conn,
						       "org.freedesktop.systemd1",
		   				        l_path,
						        "org.freedesktop.DBus.Properties"))){
		log_error_message("Failed to get proxy to D-Bus !", 0);
		goto free_res;
	}
	/* set the foreground value */
	if (!dbus_g_proxy_call (l_prop_proxy, "Set", &l_err,
                                G_TYPE_STRING, l_interface,
				G_TYPE_STRING, "Foreground",
				G_TYPE_VALUE, &l_set_value,
				G_TYPE_INVALID, G_TYPE_INVALID)){
		log_error_message("Change Task State : Failed to change task foreground task state : %s \n", l_err->message);
		g_value_unset(&l_set_value);
		goto free_res;
	}	
	log_debug_message("Called ChangeTaskState : [%d | %s] \n", app_pid,
		    (foreground == TRUE) ? "true" : "false");
        ChangeTaskState(app_pid, foreground);
	/* emit task changed state complete */
	al_dbus_change_task_state_complete(g_al_dbus, l_app_name, (foreground == TRUE) ? "true" : "false");
	
	dbus_g_method_return(context);

	/* resources free */
	if(l_path)
		free(l_path);
	if(l_app_name)
		free(l_app_name);

	return success;

free_res:
	if(l_path)
		free(l_path);
	if(l_app_name)
		free(l_app_name);
	if(l_prop_proxy){
		g_object_unref(l_prop_proxy);
		l_prop_proxy = NULL;
	}
	
	dbus_g_method_return(context);

	return success;
}

/* API signals */

gboolean al_dbus_global_state_notification(ALDbus * server, gchar * app_status)
{

	gboolean success = TRUE;
	ALDbusClass *klass = (ALDbusClass*)G_OBJECT_GET_CLASS(server);
	g_signal_emit(server,
		      klass->ALSignals[AL_SIG_GLOBAL_NOTIFICATION],
		      0,
		      app_status);
	return success;
}

gboolean al_dbus_task_started(ALDbus * server, gint app_pid, gchar * image_path)
{

	gboolean success = TRUE;
	ALDbusClass *klass = (ALDbusClass*)G_OBJECT_GET_CLASS(server);
	g_signal_emit(server,
		      klass->ALSignals[AL_SIG_TASK_STARTED],
		      0,
		      app_pid,
		      image_path);
	return success;
}

gboolean al_dbus_task_stopped(ALDbus * server, gint app_pid, gchar * image_path)
{

	gboolean success = TRUE;
	ALDbusClass *klass = (ALDbusClass*)G_OBJECT_GET_CLASS(server);
	g_signal_emit(server,
		      klass->ALSignals[AL_SIG_TASK_STOPPED],
		      0,
		      app_pid,
		      image_path);
	return success;
}

gboolean al_dbus_change_task_state_complete(ALDbus * server,
					    gchar * app_name, gchar * app_state)
{

	gboolean success = TRUE;
	ALDbusClass *klass = (ALDbusClass*)G_OBJECT_GET_CLASS(server);
	g_signal_emit(server,
		      klass->ALSignals[AL_SIG_CHANGE_STATE_COMPLETE],
		      0,
		      app_name,
		      app_state);
	return success;
}

/* Filter function for system bus signals to be dispatched by the daemon */

static DBusHandlerResult al_dbus_signal_filter(DBusConnection *connection, DBusMessage *message, void *data) {
	/* error handler */
        DBusError error;
	/* messages */
        DBusMessage *m = NULL, *reply = NULL;
	/* error initialization */
        dbus_error_init(&error);
	/* check input message type */
        if (dbus_message_is_signal(message, DBUS_INTERFACE_LOCAL, "Disconnected")) {
                log_error_message("Error! D-Bus connection terminated.\n",0);
                dbus_connection_close(connection);
        } else if (dbus_message_is_signal(message, "org.freedesktop.DBus.Properties", "PropertiesChanged")) {
		/* extract useful info if a service property changed */
                const char *path, *interface, *property = "Id";
                DBusMessageIter iter, sub;
		/* extract the service path */
                path = dbus_message_get_path(message);
		/* get service interface */
                if (!dbus_message_get_args(message, &error,
                                          DBUS_TYPE_STRING, &interface,
                                          DBUS_TYPE_INVALID)) {
                        log_error_message("Signal Dispatcher Thread : Failed to parse message when PropertiesChanged signal received !\n", 0);
                        goto finish;
                }
  		/* check for unit / job run state changes */
                if ((strcmp(interface, "org.freedesktop.systemd1.Job")!=0) &&
                    (strcmp(interface, "org.freedesktop.systemd1.Unit")!=0))
                        goto finish;
		/* get information about the changing property */
                if (!(m = dbus_message_new_method_call(
                              "org.freedesktop.systemd1",
                              path,
                              "org.freedesktop.DBus.Properties",
                              "Get"))) {
                        log_error_message("Signal Dispatcher Thread : Could not allocate message when Getting properties from systemd!\n", 0);
                        goto oom;
                }

                if (!dbus_message_append_args(m,
                                              DBUS_TYPE_STRING, &interface,
                                              DBUS_TYPE_STRING, &property,
                                              DBUS_TYPE_INVALID)) {
                        log_error_message("Signal Dispatcher Thread : Could not append arguments to message when Getting properties from systemd!\n", 0);
                        goto finish;
                }

                if (!(reply = dbus_connection_send_with_reply_and_block(connection, m, -1, &error))) {
                        log_error_message("Signal Dispatcher Thread : Failed to parse reply when Getting properties from systemd!", 0);
                        goto finish;
                }
		/* extract information from the reply */
                if (!dbus_message_iter_init(reply, &iter) ||
                    dbus_message_iter_get_arg_type(&iter) != DBUS_TYPE_VARIANT)  {
                        log_error_message("Signal Dispatcher Thread : Failed to extract reply content when Getting properties from systemd!\n", 0);
                        goto finish;
                }

                dbus_message_iter_recurse(&iter, &sub);
		/* if a unit chnaged run state (started/stopped) */ 
                if (strcmp(interface, "org.freedesktop.systemd1.Unit")==0) {
                        const char *id;

                        if (dbus_message_iter_get_arg_type(&sub) != DBUS_TYPE_STRING)  {
                                log_error_message("Signal Dispatcher Thread : Failed to parse reply when getting unit type! \n",0);
                                goto finish;
                        }
			/* get the name of the unit */
                        dbus_message_iter_get_basic(&sub, &id);
                        log_debug_message("Unit %s changed run state !\n", id);
		        /* send global state notification signal */
			AlAppStateNotifier(l_conn, id);
			/* send task started/stopped signal */
			AlSendAppSignal(l_conn, id);
                }
        }
	/* if erroneous exit deallocate resources */
finish:
        if (m)
                dbus_message_unref(m);

        if (reply)
                dbus_message_unref(reply);

        dbus_error_free(&error);
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	/* if out of memory : dealocate resources and return proper value */
oom:
        if (m)
                dbus_message_unref(m);

        if (reply)
                dbus_message_unref(reply);

        dbus_error_free(&error);
        return DBUS_HANDLER_RESULT_NEED_MEMORY;
}

/* Function that monitors signals on the bus and applies filter */

int al_dbus_monitor_signals(DBusConnection *bus) {
	/* handler messages */
        DBusMessage *m = NULL, *reply = NULL;
	/* error handler */
        DBusError error;
        int r;

	/* error initialization */
        dbus_error_init(&error);

	/* add matcher for property change signals */
                dbus_bus_add_match(bus,
                                   "type='signal',"
                                   "sender='org.freedesktop.systemd1',"
                                   "interface='org.freedesktop.DBus.Properties',"
                                   "member='PropertiesChanged'",
                                   &error);

                if (dbus_error_is_set(&error)) {
                        log_error_message("Signal Dispatcher Thread : Failed to parse message when adding matcher on Properties interface\n", 0);
                        r = -EIO;
                        goto finish;
                }
	/* add the filter for the specific signals */
        if (!dbus_connection_add_filter(bus, al_dbus_signal_filter, NULL, NULL)) {
                log_error_message("Signal Dispatcher Thread : Failed to add filter for systemd property changes signals!\n", 0);
                r = -ENOMEM;
                goto finish;
        }	
	/* subscribe to systemd */
        if (!(m = dbus_message_new_method_call(
                              "org.freedesktop.systemd1",
                              "/org/freedesktop/systemd1",
                              "org.freedesktop.systemd1.Manager",
                              "Subscribe"))) {
                log_error_message("Signal Dispatcher Thread : Could not allocate message when subscribing to systemd !\n", 0);
                r = -ENOMEM;
                goto finish;
        }

        if (!(reply = dbus_connection_send_with_reply_and_block(bus, m, -1, &error))) {
                log_error_message("Signal Dispatcher Thread : Failed to parse reply after subscribing to systemd ! \n", 0);
                r = -EIO;
                goto finish;
        }
	/* message processing */
        while (dbus_connection_read_write_dispatch(bus, -1))
                ;

        r = 0;

finish:
        /* resources free */
        if (m)
                dbus_message_unref(m);
        if (reply)
                dbus_message_unref(reply);
        dbus_error_free(&error);
        return r;
}

/* Function that connect to the system bus and listens to PropertiesChanged signal to enable daemon notification support */
void *al_dbus_catch_signals(void *param){
    
    /* error handler*/
    DBusError l_err;
    /* define message and reply */
    DBusMessage *l_msg =  NULL, *l_reply = NULL, *l_msg_notif = NULL, *l_reply_notif = NULL;
    /* initialise the error value */
    dbus_error_init(&l_err);
    /* generic return code */
    int l_ret;
    /* application name */
    char *l_app;
    /* thread attributes setup */
    l_ret = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    if (l_ret != 0) {
        log_error_message("Signal Dispatcher Thread : Thread pthread_setcancelstate failed\n", 0);
        exit(EXIT_FAILURE);
    }
    l_ret = pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
    if (l_ret != 0) {
        log_error_message("Signal Dispatcher Thread : Thread pthread_setcanceltype failed\n", 0);
        exit(EXIT_FAILURE);
    }
    /* connect to the system bus and check for errors */
    l_conn = dbus_bus_get_private(DBUS_BUS_SYSTEM, &l_err);
    if (dbus_error_is_set(&l_err)) {
        log_error_message("Signal Dispatcher Thread : Cannot connect to dbus! %s\n", l_err.message);
        dbus_error_free(&l_err);
    }
   /* loop and listen for signals */
   for(;l_conn!=NULL;){
      if(al_dbus_monitor_signals(l_conn)!=0){
	log_error_message("Signal Dispatcher Thread : An error occured ... restart signal listener", 0);
	continue;
     }
   }
  /* resources free */
  if(l_msg)
	dbus_message_unref(l_msg);
  if(l_reply)
	dbus_message_unref(l_reply);
  if(l_msg_notif)
	dbus_message_unref(l_msg_notif);
  if(l_reply_notif)
	dbus_message_unref(l_reply_notif);
  /* close and unref private connection */
  if (l_conn != NULL)
        dbus_connection_close(l_conn);
        dbus_connection_unref(l_conn);
  pthread_exit(NULL);
}

/* Function responsible to cancel the signal dispatcher thread */
void cancel_signal_dispatcher(){
	log_debug_message("Cancelling signal dispatcher thread\n", 0);
	pthread_cancel(signal_dispatch_thread_id);
}

/* Function responsible to dispatch and emit signals according to context */
void al_dbus_signal_dispatcher()
{
	/* return code */
	int l_ret;
	if(l_ret = pthread_create(&signal_dispatch_thread_id,
	    		          NULL,
			          al_dbus_catch_signals,
			          (void*)NULL)){
		log_error_message("Signal Dispatcher Thread : Cannot create signal dispatcher thread : %d\n", l_ret);
		exit(EXIT_FAILURE);
	}
}

/* High level interface for the AL Daemon */
void Run(char *p_commandLine, int p_parentPID, bool p_isFg)
{
	/* store the return code */
	int l_ret;
	/* application PID */
	int l_pid;
	/* state string */
	char *l_flag = malloc(DIM_MAX * sizeof(l_flag));
	log_message("Run : %s started with run !\n", p_commandLine);
	/* the command line for the application */
	char l_cmd[DIM_MAX];
	/* form the call string for systemd */
	/* check if template / simple service will be started with run */
	if ((AppExistsInSystem(p_commandLine)) == 1) {
		sprintf(l_cmd, "systemctl start %s.service", p_commandLine);
	}
	/* check if target (group of apps) will be started with run */
	if ((AppExistsInSystem(p_commandLine)) == 2) {
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
	if (p_isFg == TRUE)
		strcpy(l_flag, "foreground");
	else
		strcpy(l_flag, "background");
	/* change the state of the application given by pid */
	log_message("Run : Application %s will run in %s \n",
		    p_commandLine, l_flag);
	/* systemd invocation */
	l_ret = system(l_cmd);
	if (l_ret == -1) {
		if ((AppExistsInSystem(p_commandLine)) == 1) {
			log_error_message
			    ("Run : Application cannot be started with run! Err: %s\n",
			     strerror(errno));
		}
		if ((AppExistsInSystem(p_commandLine)) == 2) {
			log_error_message
			    ("Run : Applications group cannot be started with run! Err: %s\n",
			     strerror(errno));
		}
		return;
	}
	log_debug_message("Run : %s was started with run !\n",
			  p_commandLine);
}

void RunAs(char *p_commandLine, int p_parentPID, bool p_isFg, int p_euid,
	   int p_egid)
{
	/* store the return code */
	int l_ret;
	/* application PID */
	int l_pid;
	/* local handlers for user and group to be written in the service file */
	char *l_user = malloc(DIM_MAX * sizeof(l_user));
	char *l_group = malloc(DIM_MAX * sizeof(l_group));
	log_message("RunAs : %s started with runas !\n",
		    p_commandLine);
	/* the command line for the application */
	char l_cmd[DIM_MAX];
	/* the service file path */
	char l_srv_path[DIM_MAX];
	/* string that will store the state */
	char *l_flag = malloc(DIM_MAX * sizeof(l_flag));
	/* check if template */
	char *l_temp = ExtractUnitNameTemplate(p_commandLine);
	if (l_temp == NULL) {
		log_error_message
		    ("RunAs : Cannot allocate template handler for %s !\n",
		     p_commandLine);
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
	if (MapUidToUser(p_euid, l_user) != 0) {
		log_error_message
		    ("RunAs : Cannot map uid to user for %s\n",
		     p_commandLine);
		return;
	}
	if (MapGidToGroup(p_egid, l_group) != 0) {
		log_error_message
		    ("RunAs : Cannot map gid to user for %s\n",
		     p_commandLine);
		return;
	}
	/* SetupUnitFileKey call to setup the key and corresponding values, if not existing will be created */
	/* the service file will be updated with proper Group and User information */
	SetupUnitFileKey(l_srv_path, "User", l_user, p_commandLine);
	SetupUnitFileKey(l_srv_path, "Group", l_group, p_commandLine);
	log_debug_message
	    ("RunAs : Service file was updated with User=%s and Group=%s information !\n",
	     l_user, l_group);
	/* test application state */
	if (p_isFg == TRUE)
		strcpy(l_flag, "foreground");
	else
		strcpy(l_flag, "background");
	/* issue daemon reload to apply and acknowledge modifications to the service file on the disk */
	l_ret = system("systemctl daemon-reload --system");
	if (l_ret == -1) {
		log_error_message
		    ("RunAs : After setting the service file reload systemd manager configuration failed ! Err: %s\n",
		     strerror(errno));
		return;
	}
	/* change the state of the application given by pid */
	log_debug_message
	    ("RunAs : Application %s will runas %s in %s \n",
	     p_commandLine, l_user, l_flag);

	/* systemd invocation */
	l_ret = system(l_cmd);
	if (l_ret == -1) {
		log_error_message
		    ("RunAs : Application cannot be started with runas! Err: %s\n",
		     strerror(errno));
		return;
	}
	log_debug_message("RunAs : %s was started with runas !\n",
			  p_commandLine);
}

void Suspend(int p_pid)
{
	/* return code */
	int l_ret;
	/* to suspend the application a SIGSTOP signal is sent */
	if ((l_ret = kill(p_pid, SIGSTOP)) == -1) {
		log_error_message
		    ("Suspend : %s cannot be suspended ! Err : %s\n",
		     p_pid, strerror(errno));
	}
}

void Resume(int p_pid)
{
	/* return code */
	int l_ret;
	/* to suspend the application a SIGSTOP signal is sent */
	if ((l_ret = kill(p_pid, SIGCONT)) == -1) {
		log_error_message
		    ("Resume : %s cannot be resumed ! Err : %s\n",
		     p_pid, strerror(errno));
	}
}

void Stop(int p_pid)
{
	/* store the return code */
	int l_ret;
	/* stores the application name */
	char *l_app_name = malloc(DIM_MAX * sizeof(l_app_name));
	/* command line for the application */
	char *l_commandLine = malloc(DIM_MAX * sizeof(l_app_name));
	l_ret = (int)AppNameFromPid(p_pid, l_app_name);
	if (l_ret == 1) {
		l_commandLine = l_app_name;
		char l_cmd[DIM_MAX];
		log_debug_message("Stop : %s stopped with stop !\n",
				  l_commandLine);
		/* form the systemd command line string */
		/* check if single service will be stopped */
		if ((AppExistsInSystem(l_commandLine)) == 1) {
			sprintf(l_cmd, "systemctl stop %s.service",
				l_commandLine);
		}
		/* call systemd */
		l_ret = system(l_cmd);
		if (l_ret != 0) {
			log_error_message
			    ("Stop : Application cannot be stopped with stop! Err: %s\n",
			     strerror(errno));
		}
	} else {
		log_error_message
		    ("Stop : Application %s cannot be stopped because is already stopped !\n",
		     l_commandLine);
	}
}

void StopAs(int p_pid, int p_euid, int p_egid)
{
	/* store the return code */
	int l_ret;
	/* stores the application name */
	char *l_app_name = malloc(DIM_MAX*sizeof(l_app_name));
	/* command line for the application */
	char *l_commandLine = malloc(DIM_MAX * sizeof(l_commandLine));
	/* extracted user and group values from service file */
	char *l_group = malloc(DIM_MAX * sizeof(l_group));
	char *l_user = malloc(DIM_MAX * sizeof(l_user));
	/* application service fiel path */
	char *l_srv_path = malloc(DIM_MAX*sizeof(l_srv_path));
	/* group and user strings */
	char *l_str_egid = malloc(DIM_MAX * sizeof(l_str_egid));
	char *l_str_euid = malloc(DIM_MAX * sizeof(l_str_euid));
	/* test if application runs in the system */
	if (AppNameFromPid(p_pid, l_app_name) != 0) {
		l_commandLine = l_app_name;
		/* for the path to the application service */
		sprintf(l_srv_path, "/lib/systemd/system/%s.service",
			l_commandLine);
		/* low level command to systemd */
		char l_cmd[DIM_MAX];
		log_debug_message
		    ("StopAs : %s stopped with stopas !\n",
		     l_app_name);
		/* form the systemd command line string */
		sprintf(l_cmd, "systemctl stop %s.service", l_commandLine);
		/* test ownership and rights before stopping application */
		log_debug_message
		    ("StopAs : Extracting ownership info for %s\n",
		     l_commandLine);
		ExtractOwnershipInfo(l_user, l_group, l_srv_path);
		log_debug_message
		    ("StopAs : The ownership information was extracted properly [ user : %s ] and [ group : %s ]\n",
		     l_user, l_group);
		/* extract user name and group name from uid and gid */
		if (MapUidToUser(p_euid, l_str_euid) != 0) {
			log_error_message
			    ("StopAs : Cannot map uid to user for %s\n",
			     l_commandLine);
			goto free_res;
		}
		if (MapGidToGroup(p_egid, l_str_egid) != 0) {
			log_error_message
			    ("StopAs : Cannot map gid to user for %s\n",
			     l_commandLine);
			goto free_res;
		}
		log_message
		    ("StopAs : The input ownership information [ user: %s ] and [ group : %s ]\n",
		     l_str_euid, l_str_egid);
		if ((strcmp(l_user, l_str_euid) != 0)
		    && (strcmp(l_group, l_str_egid) != 0)) {
			log_error_message
			    ("StopAs : The current user doesn't have permissions to stopas %s!\n",
			     l_commandLine);
			goto free_res;
		}
		/* call systemd */
		l_ret = system(l_cmd);
		if (l_ret != 0) {
			log_error_message
			    ("StopAs : Application cannot be stopped! Err:%s\n",
			     strerror(errno));
		    goto free_res;
		}
	} else {
		log_error_message
		    ("StopAs : Application %s cannot be stopped because is already stopped !\n",
		     l_commandLine);
		return;
	}
	
	return;
free_res:
	if(l_app_name) free(l_app_name);
	if(l_commandLine) free(l_commandLine);
	if(l_group) free(l_group);
	if(l_user) free(l_user);
	if(l_srv_path) free(l_srv_path);
	if(l_str_egid) free(l_str_egid);
	if(l_str_euid) free(l_str_euid);
	return;
}

void TaskStarted(int p_pid, char *p_imagePath)
{
	log_debug_message
	    ("TaskStarted Signal : Task %d %s was started and signal %s was emitted!\n",
	     &p_pid, p_imagePath, AL_SIGNAME_TASK_STARTED);
}

void TaskStopped(int p_pid, char *p_imagePath)
{
	log_debug_message
	    ("TaskStopped Signal : Task %d %s was stopped and signal %s was emitted!\n",
	     &p_pid, p_imagePath, AL_SIGNAME_TASK_STOPPED);
}

void ChangeTaskState(int p_pid, bool p_isFg)
{
	char *l_flag = malloc(DIM_MAX * sizeof(l_flag));
	if (p_isFg == TRUE)
		strcpy(l_flag, "foreground");
	else
		strcpy(l_flag, "background");
	/* change the state of the application given by pid */
	log_debug_message
	    ("ChangeTaskState : Application with pid %d changed state to %s \n",
	     p_pid, l_flag);
}

/* 
 * Function responsible with restarting an application when the SHM component detects
 * an abnormal operation of the application
 */
void Restart(char *p_app_name)
{
	int l_ret;
	/* the command line for the application */
	char l_cmd[DIM_MAX];
	log_debug_message("Restart : %s will be restarted !\n",
			  p_app_name);
	/* check if single service will be restarted */
	if ((AppExistsInSystem(p_app_name)) == 1) {
		sprintf(l_cmd, "systemctl restart %s.service", p_app_name);
	}
	/* check if target (group of apps) will be restarted */
	if ((AppExistsInSystem(p_app_name)) == 2) {
		sprintf(l_cmd, "systemctl restart %s.target", p_app_name);
	}
	/* systemd invocation */
	l_ret = system(l_cmd);
	if (l_ret != 0) {
		if ((AppExistsInSystem(p_app_name)) == 1) {
			log_error_message
			    ("Restart : Application cannot be restarted with restart! Err: %s\n",
			     strerror(errno));
		}
		if ((AppExistsInSystem(p_app_name)) == 2) {
			log_error_message
			    ("Restart : Applications group cannot be restarted with restart! Err: %s\n",
			     strerror(errno));
		}
	}
}

