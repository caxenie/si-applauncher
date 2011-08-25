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

/* Function responsible to parse the service unit and extract ownership info */
void ExtractOwnershipInfo(char *p_euid, char *p_egid, char *p_file)
{
  /* key file group length */
  gsize l_groups_length;
  /* store the groups in the keyfile */
  char **l_groups;
  /* local store of egid and euid */
  char *l_gid = malloc(DIM_MAX*sizeof(l_gid));
  char *l_uid = malloc(DIM_MAX*sizeof(l_uid));
  log_debug_message("AL Daemon Unit File Parser : Creating new key file to support ownership info for %s \n", p_file);
  /* the created GKeyFile for the given file on disk */
  GKeyFile *l_out_new_key_file = g_key_file_new();
  if(!l_out_new_key_file) {
	log_error_message("AL Daemon Unit File Parser : The key file cannot be created for %s \n", p_file);
	return;
  }
  log_debug_message("AL Daemon Unit File Parser : Initialize the error for uid/gid extraction for %s \n", p_file);
  /* initialize the error */
  GError *l_err = NULL;
  log_debug_message("AL Daemon Unit File Parser : Load key file from disk for %s \n", p_file);
  /* load key file structure from file on disk */
  if (!g_key_file_load_from_file
      (l_out_new_key_file, p_file, G_KEY_FILE_NONE, &l_err)) {
          if(NULL!=l_err){
              log_error_message
	          ("AL Daemon Unit File Parser : Cannot load key structure from service unit key file! (%d: %s)\n",
                   l_err->code, l_err->message);
              g_error_free(l_err);
          } else {
              log_error_message
	          ("AL Daemon Unit File Parser : Cannot load key structure from service unit key file!%s\n", "");
          }
    g_key_file_free(l_out_new_key_file);
    return;
  }
  log_debug_message("AL Daemon Unit File Parser : Extracting groups from key file structure for %s \n", p_file);
  /* extract groups from key file structure */
  l_groups = g_key_file_get_groups(l_out_new_key_file, &l_groups_length);
  log_debug_message
	("AL Daemon Ownership Info Extractor : Extracted groups from key file for %s \n",
	 p_file);
  if (l_groups == NULL) {
    log_error_message
	("AL Daemon Ownership Info Extractor : Could not retrieve groups from %s service unit file!\n",
	 p_file);
      g_key_file_free(l_out_new_key_file);
      return;
  }
  unsigned long l_i;
  /* loop to get keys from key fle structure */
  for (l_i = 0; l_i < l_groups_length; l_i++) {
    gsize l_keys_length;
    char **l_keys;
    /* get current key from file */
    l_keys =
	g_key_file_get_keys(l_out_new_key_file, l_groups[l_i],
			    &l_keys_length, &l_err);
    log_debug_message
	("AL Daemon Ownership Info Extractor : Extracted keys from key file for %s \n",
	 p_file);
    /* check if the file is properly structured */
    if (l_keys == NULL) {
        if(NULL!=l_err){
            log_error_message
	        ("al daemon ownership info extractor : error in retrieving keys in service unit file! (%d: %s)", 
                 l_err->code, l_err->message);
            g_error_free(l_err);
        } else {
            log_error_message
	        ("al daemon ownership info extractor : error in retrieving keys in service unit file!%s", "");
        }
    } else {
      unsigned long l_j;
      /* loop to get key values */
      for (l_j = 0; l_j < l_keys_length; l_j++) {
	char *l_str_value;
	/* extract current value for a given key */
	l_str_value =
	    g_key_file_get_string(l_out_new_key_file, l_groups[l_i],
				  l_keys[l_j], &l_err); 
        if(strcmp(l_keys[l_j], "User")==0){
		l_uid = l_str_value;		
 	}
        if(strcmp(l_keys[l_j], "Group")==0){
		l_gid = l_str_value;		
 	}
	/* check value validity */
	if (l_str_value == NULL) {
            if(NULL!=l_err){
                log_error_message
                    ("AL Daemon Ownership Info Extractor : Error retrieving key's value in service unit file. (%d, %s)\n",
                     l_err->code, l_err->message);
                g_error_free(l_err);
            } else {
                log_error_message
                    ("AL Daemon Ownership Info Extractor : Error retrieving key's value in service unit file.%s\n", "");
            }
	}
      }
    }
  }
  /* the extracted ownership values */
  log_debug_message("AL Daemon Ownership Info Extractor : Preparing keys to be returned for %s \n",
	 p_file);
  strcpy(p_egid, l_gid);
  log_debug_message("AL Daemon Ownership Info Extractor : Extracted keys from key file for %s \n",
	 p_file);
  strcpy(p_euid, l_uid);
  log_debug_message("AL Daemon Ownership Info Extractor : Extracted keys from key file for %s \n",
	 p_file);
  if(l_out_new_key_file) g_key_file_free(l_out_new_key_file);
}

/* Function responsible to extract the user name from the uid */
int MapUidToUser(int p_uid, char *p_user){
  /* handler for passwd */
  struct passwd *l_pwd;
  /* storage for uid */
  uid_t l_uid = (uid_t)p_uid;
  /* check id existence */
  if ((l_pwd = getpwuid(p_uid))==NULL){
         log_error_message("AL Daemon UID to User Mapper : UID %d is not associated with any existing user !\n", p_uid);
	 return -1;
	 }
  /* extract the user from the structure */
  strcpy(p_user, l_pwd->pw_name);
  log_debug_message("AL Daemon UID to User Mapper : uid=%d(%s) \n", p_uid, p_user);
  return 0;
}

/* Function responsible to extract the group name from the gid */
int MapGidToGroup(int p_gid, char *p_group){
  /* handler for group info */
  struct group *l_gp;
  /* storage for gid */
  gid_t l_gid = (gid_t)p_gid;
  /* check group */
  if ((l_gp = getgrgid(l_gid))==NULL){
	log_error_message("AL Daemon GID to User Mapper : GID %d is not associated with any existing group !\n", p_gid);
	return -1;	
	}
  /* extract guid information */
  strcpy(p_group, l_gp->gr_name);
  log_debug_message("AL Daemon GID to Group Mapper : gid=%d(%s) \n", p_gid, p_group);
  return 0;
}

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
    log_error_message("AL Daemon Get Current User : Cannot acces current user key !\n", 0);
    return 0; 
  }
  /* get the current user and check for errors */
  if((l_str_val = gconf_client_get_string(p_client, l_key, &l_err)) == NULL){
    if(NULL!=l_err){
      log_error_message("AL Daemon Get Current User : The current user key has invalid content! Err : %s\n",l_err->message);
      g_error_free(l_err);
    } else {
      log_error_message("AL Daemon Get Current User : The current user key has invalid content!%s\n","");
    }
      return 0;
   } 
  /* to make the user name available when returning */
  l_usr = strdup(l_str_val);
  strcpy(p_user, l_usr);
  log_debug_message("AL Daemon Get Current User : The current user is USER=%s\n", p_user);
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
  int l_app_pid;
  /* test user existence in gconftree file */
  if(p_user==NULL){
	log_error_message("AL Daemon Start User Mode Apps : The gconftree file doesn't exist or the user was not created !\n Skipping last user mode application startup !\n", 0);
	goto free_res;
  }
  /* test client validity */
  if(p_client==NULL){
	log_error_message("AL Daemon Start User Mode Apps : The gconf client is not valid !\n Skipping last user mode application startup !\n", 0);
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
		log_error_message("AL Daemon Start User Mode Apps : Application list for current user mode is empty !%s\n", (NULL!=l_err?l_err->message:""));
		goto free_res;
        }else{
	 	log_error_message("AL Daemon Start User Mode Apps : Cannot get list from current user key !%s\n", (NULL!=l_err?l_err->message:""));
		goto free_res;
        }
     }
  /* call run for each entry in the user mode application list */
  for(l_idx = 0; l_idx<(int)g_slist_length(l_app_list); l_idx++){
	 /* get app name */
	 l_app = (char*)g_slist_nth_data(l_app_list, l_idx);
         /* run application */
 	 Run(l_app, 0, TRUE, l_app_pid);
	 log_debug_message("AL Daemon Start User Mode Apps : Started %s for user %s !\n", l_app, p_user);
  }
  log_debug_message("AL Daemon Start User Mode Apps : Last user mode applications for user %s was setup!\n", p_user);
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
    log_error_message("AL Daemon Last User Mode Init : Failed to create client for last-user-mode!\n", 0);
    goto free_res;
  }
  /* extract entry */
  if((l_current_user_key = gconf_client_get_entry(l_client, AL_GCONF_CURRENT_USER_KEY, NULL, FALSE, &l_error)) ==  NULL){
	log_error_message("AL Daemon Last User Mode Init : Failed to get entry for current user key ! %s!\n",
            l_error->message);
    	g_clear_error(&l_error);
	goto free_res;
  }
  /* get the current value for the current_user key */
  if(!GetCurrentUser(l_client, l_current_user_key, l_current_user)){
  		log_error_message("AL Daemon Last User Mode Init : Cannot extract current user !\n", 0);
 		goto free_res;
  }
  /* start current user mode applications */
  if(!StartUserModeApps(l_client, l_current_user)){
	log_error_message("AL Daemon Last User Mode Init : Cannot start user mode applications !\n", 0);
		goto free_res;
  }
   g_object_unref(l_client) ;
   return 1;
free_res:
  if(l_client) g_object_unref(l_client) ;
  return 0;
}


