/* 
* lum.c, contains the implementation of the daemon last user mode functionality
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

#include "lum.h"
#include "al-daemon.h"
#include "dbus_interface.h"

/* 
 * Function responsible to get the current user as specified in the current_user GConf key.
 * This will be called once the daemon starts for last user mode functionality
 */

int GetCurrentUser(GConfClient* p_client, GConfEntry *p_key, char *p_user)
{ 
  /* this will hold a pointer to the name of the key */
  const gchar* l_key = NULL;
  /* contains the user name */
  gchar* l_str_val = NULL;
  /* error handler */
  GError *l_err = NULL;
  /* duplicate for extracted name */
  char *l_usr;
  /* get key and test for errors */
  if ((l_key = gconf_entry_get_key(p_key)) == NULL) {
    log_error_message("Get Current User : Cannot acces current user key !\n", 0);
    return 0; 
  }
  /* get the current user and check for errors */
  if((l_str_val = gconf_client_get_string(p_client, l_key, &l_err)) == NULL){
    if(NULL!=l_err){
      log_error_message("Get Current User : The current user key has invalid content! Err : %s\n",l_err->message);
      g_error_free(l_err);
    } else {
      log_error_message("Get Current User : The current user key has invalid content!%s\n","");
    }
      return 0;
   } 
  /* to make the user name available when returning */
  l_usr = strdup(l_str_val);
  strcpy(p_user, l_usr);
  log_debug_message("Get Current User : The current user is USER=%s\n", p_user);
  return 1;
}

/* Function responsible to start the specific applications for the current user mode */
int StartUserModeApps(GConfClient *p_client, char *p_user)
{
  /* stores the key to acces last user mode list of applications */
  char *l_last_mode_key = malloc(DIM_MAX*sizeof(l_last_mode_key));
  /* pointer to the last_mode key */
  GSList *l_app_list;
  /* error handler for list get */
  GError *l_err = NULL;
  /* index in user mode application list */
  int l_idx;
  /* current entry in application list */
  char *l_app;
  /* pid for currently started application in the list */
  int *l_app_pid;
  /* test user existence in gconftree file */
  if(p_user==NULL){
	log_error_message("Start User Mode Apps : The gconftree file doesn't exist or the user was not created !\n Skipping last user mode application startup !\n", 0);
	goto free_res;
  }
  /* test client validity */
  if(p_client==NULL){
	log_error_message("Start User Mode Apps : The gconf client is not valid !\n Skipping last user mode application startup !\n", 0);
	goto free_res;
  }
  /* form the specific last_mode key for user */
  strcpy(l_last_mode_key, "/"); 
  strcat(l_last_mode_key, p_user);
  strcat(l_last_mode_key, AL_GCONF_LAST_USER_MODE_KEY);
  /* get pointer to key (not a copy ) */
  if((l_app_list = gconf_client_get_list(p_client, (gchar*)l_last_mode_key, GCONF_VALUE_STRING, &l_err)) == NULL){
        /* test if list is empty */
        if((gconf_client_get(p_client, (gchar*)l_last_mode_key, NULL)) == NULL){
		log_error_message("Start User Mode Apps : Application list for current user mode is empty !%s\n", (NULL!=l_err?l_err->message:""));
		goto free_res;
        }else{
	 	log_error_message("Start User Mode Apps : Cannot get list from current user key !%s\n", (NULL!=l_err?l_err->message:""));
		goto free_res;
        }
     }
  /* call run for each entry in the user mode application list */
  for(l_idx = 0; l_idx<(int)g_slist_length(l_app_list); l_idx++){
	 /* get app name */
	 l_app = (char*)g_slist_nth_data(l_app_list, l_idx);
         /* run application */
 	 Run(l_app, 0, TRUE);
	 log_debug_message("Start User Mode Apps : Started %s for user %s !\n", l_app, p_user);
  }
  log_debug_message("Start User Mode Apps : Last user mode applications for user %s was setup!\n", p_user);
   g_slist_free(l_app_list);
   return 1;
free_res:
	if(l_app_list)  g_slist_free(l_app_list);
        if(NULL!=l_err) g_error_free(l_err);
  return 0;
}

/* Function responsible to initialize the last user mode at daemon startup */
int InitializeLastUserMode()
{
  /* reference to the GConfClient object */
  GConfClient* l_client = NULL;
  /* initialize error */
  GError* l_error = NULL;
  /* current user from current_user key */
  char *l_current_user=malloc(DIM_MAX*sizeof(l_current_user));
  /* current user key pointer */
  GConfEntry *l_current_user_key = NULL;
  /* Last-User-Mode functionality implementation */
  /* initialize GType system */
  g_type_init();
  /* create a new GConfClient object using the default settings. */
  if(((l_client = gconf_client_get_default()) == NULL)){
    log_error_message("Last User Mode Init : Failed to create client for last-user-mode!\n", 0);
    goto free_res;
  }
  /* extract entry */
  if((l_current_user_key = gconf_client_get_entry(l_client, AL_GCONF_CURRENT_USER_KEY, NULL, FALSE, &l_error)) ==  NULL){
	log_error_message("Last User Mode Init : Failed to get entry for current user key ! %s!\n",
            l_error->message);
    	g_clear_error(&l_error);
	goto free_res;
  }
  /* get the current value for the current_user key */
  if(!GetCurrentUser(l_client, l_current_user_key, l_current_user)){
  		log_error_message("Last User Mode Init : Cannot extract current user !\n", 0);
 		goto free_res;
  }
  /* start current user mode applications */
  if(!StartUserModeApps(l_client, l_current_user)){
	log_error_message("Last User Mode Init : Cannot start user mode applications !\n", 0);
		goto free_res;
  }
   g_object_unref(l_client) ;
   return 1;
free_res:
  if(l_client) g_object_unref(l_client) ;
  return 0;
}


