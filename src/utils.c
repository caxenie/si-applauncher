/* 
* utils.c, contains the implementation for various utility functions used in the daemon code
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
#include "utils.h"

/* Function responsible with the daemonization procedure */
void AlDaemonize()
{
  /* file descriptor */
  int l_fp;
  /* container for the pid */
  char l_str[10];
  /* setup a PID and a SID for our AL daemon */
  pid_t l_al_sid, l_al_pid;

   /* fork process */ 
   l_al_pid=fork();

    /* test for forking errors */
    if (l_al_pid<0) {
        log_error_message("AL Daemon : Cannot fork off parent process!\n", 0);
        exit(EXIT_FAILURE);
        }

    /* check parent exit */
    if (l_al_pid>0) {
        log_debug_message("AL Daemon : Parent process exited!\n", 0);
        exit(0);
    }
    /* child (daemon) continues */
    l_al_sid = setsid();
    /* obtain a new process group */
    if (l_al_sid < 0) {
          log_error_message("AL Daemon : Cannot set SID for the process!\n", 0);
          exit(EXIT_FAILURE);
      }

     /* set newly created file permissions */
     umask(0);

    l_fp=open(AL_PID_FILE, O_RDWR|O_CREAT, 0640);

    /* test if pid file can be open */
    if (l_fp<0) {
        log_error_message("AL Daemon : Cannot open pid file\n", 0);
        exit(1);
    }

    /* test if pid file can locked */
    if (lockf(l_fp, F_TLOCK,0)<0) {
        log_error_message("AL Daemon : Cannot obtain lock on pid file\n", 0);
        exit(0);
	}

    /* first instance continues */
    sprintf(l_str, "%d\n", getpid());
    /* record pid to lockfile */
    write(l_fp, l_str, strlen(l_str)); 
 
    /* close unneeded file descriptors to maintain security */
    close(STDIN_FILENO);
}

/* Function responsible to shutdown the daemon process */
void AlDaemonShutdown(){
	remove(AL_PID_FILE);
	log_debug_message("AL Daemon : Daemon process was stopped !\n", 0);
	system("killall al-daemon");
}

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
    log_error_message
	("AL Daemon Application Pid Extractor : Cannot open directory for %s\n!",
	 p_app_name);
    return 0;
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
      return l_pid;
    }
  }
  return (pid_t) 0;
}


/* Find application name from PID */
int AppNameFromPid(int p_pid, char *p_app_name)
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
  if(l_fp == NULL)
	return 0;
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
  return 1;
}

/* 
 * Function responsible to test if a given application exists in the system.
 * It searches for the associated .service or .target file for a string representing a name.
 * The target is responsible to implement applications group specific functionality.
 * The function returns : 1 - service found | 2 - target found | 0 - no unit found for name
 */
int AppExistsInSystem(char *p_app_name)
{
  /* full name string */
  char full_name_srv[DIM_MAX];
  char full_name_trg[DIM_MAX];
  /* check if template */
  char *l_temp = ExtractUnitNameTemplate(p_app_name);
  /* get the full path name */
  sprintf(full_name_srv, "/lib/systemd/system/%s.service", l_temp);
  sprintf(full_name_trg, "/lib/systemd/system/%s.target", p_app_name);
  /* contains stat info for service / target */
  struct stat file_stat;
  int ret = -1;
  /* get stat information for service */
  if ((ret = stat(full_name_srv, &file_stat))==0) {
    /* service file was found */
    return 1;
  }
  /* get stat information for target */
  if ((ret = stat(full_name_trg, &file_stat))==0) {
    /* target file was found */
    return 2;
  }
  /* nor service file, nor target file found */
  return 0;
}

/* 
 * Function responsible to parse the .timer unit and extract the triggering key 
 * also it can acces a service file to extract its structure 
 */

GKeyFile *ParseUnitFile(char *p_file)
{
  /* key file group length */
  gsize l_groups_length;
  /* store the groups in the keyfile */
  char **l_groups;
  /* the created GKeyFile for the given file on disk */
  GKeyFile *l_out_new_key_file = g_key_file_new();
  /* initialize the error */
  GError *l_err = NULL;
  /* load key file structure from file on disk */
  if (!g_key_file_load_from_file
      (l_out_new_key_file, p_file, G_KEY_FILE_KEEP_COMMENTS | G_KEY_FILE_KEEP_TRANSLATIONS, &l_err)) {
   /* test if unit is a timer or a normal service */
   if(strstr(p_file, ".timer")==NULL){
    log_error_message
	("AL Daemon Unit File Parser : Cannot load key structure from service unit key file! (%d: %s)\n",
	 l_err->code, l_err->message);
    }else{
    log_error_message
	("AL Daemon Unit File Parser : Cannot load key structure from timer unit key file! (%d: %s)\n",
	 l_err->code, l_err->message);
    }
    g_error_free(l_err); 
    g_key_file_free(l_out_new_key_file);
    return NULL;
  }
  /* extract groups from key file structure */
  l_groups = g_key_file_get_groups(l_out_new_key_file, &l_groups_length);
  if (l_groups == NULL) {
   if(strstr(p_file,".timer")==NULL){
    log_error_message
	("AL Daemon Unit File Parser : Could not retrieve groups from %s service unit file!\n",
	 p_file);
    }else{	
    log_error_message
	("AL Daemon Unit File Parser : Could not retrieve groups from %s timer unit file!\n",
	 p_file);
    }
    return NULL;
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
    /* check if the file is properly structured */
    if (l_keys == NULL) {
     if(strstr(p_file,".timer")==NULL){
       log_error_message
	  ("AL Daemon Unit File Parser : Error in retrieving keys in service unit file! (%d: %s)",
	   l_err->code, l_err->message);
     }else{
      log_error_message
	  ("AL Daemon Unit File Parser : Error in retrieving keys in timer unit file! (%d: %s)",
	   l_err->code, l_err->message);
     } 
     g_error_free(l_err);
    } else {
      unsigned long l_j;
      /* loop to get key values */
      for (l_j = 0; l_j < l_keys_length; l_j++) {
	char *l_str_value;
	/* extract current value for a given key */
	l_str_value =
	    g_key_file_get_string(l_out_new_key_file, l_groups[l_i],
				  l_keys[l_j], &l_err);
	/* check value validity */
	if (l_str_value == NULL) {
	  if(strstr(p_file,".timer")==NULL){
	   log_error_message
	      ("AL Daemon Unit File Parser : Error retrieving key's value in service unit file. (%d, %s)\n",
	       l_err->code, l_err->message);
	  }else{
	   log_error_message
	      ("AL Daemon Unit File Parser : Error retrieving key's value in timer unit file. (%d, %s)\n",
	       l_err->code, l_err->message);
	  }
	  g_error_free(l_err);
	}
      }
    }
  }
  /* return the parsed key file structure */
  return l_out_new_key_file;
}

/* 
 * Function responsible to parse the .timer unit and setup the triggering key value
 * also it can setup the specific egid and euid values 
 */

void SetupUnitFileKey(char *p_file, char *p_key, char *p_val, char *p_unit)
{
  /* key file handlers */
  FILE *l_fd_open; 
  int l_fd_create;
  /* variable to store stat info */
  struct stat l_buf;
  /* test application service file existence and exit with error if it doesn't exist */
  if(strstr(p_file,".timer")==NULL){
	log_debug_message("AL Daemon Service Unit Setup : Test service file existence for %s\n", p_unit);
	if(g_stat(p_file,&l_buf)!=0){
		log_error_message("AL Daemon Service Unit Setup : Service file %s stat !\n",
		p_file);
		return;
	}
	/* open the corresponding key file for the service to write */
	log_debug_message("AL Daemon Service Unit Setup : Test service file open for %s\n", p_unit);
        if ((l_fd_open = g_fopen(p_file, "r+"))==NULL) {
      	  log_error_message
	    ("AL Daemon Service Unit Setup : Cannot open service unit file %s for adding data !\n",
	     p_file);
          return;
        }
  }else{
  log_debug_message("AL Daemon Service Unit Setup : Test timer file existence for %s\n", p_unit);
  /* test timer file existence and create if not exists */
  if(g_stat(p_file,&l_buf)!=0){
    /* initialize the error */
    GError *l_error = NULL;
    /* entries for the special reboot and poweroff units */
    char *l_reboot_entry =
	"[Unit]\n Description=Timer for deferred reboot\n [Timer]\n OnActiveSec=0s\n Unit=reboot.service\n";
    char *l_shutdown_entry =
	"[Unit]\n Description=Timer for deferred shutdown\n [Timer]\n OnActiveSec=0s\n Unit=poweroff.service\n";
    /* create the key file with permissions */
    log_debug_message("AL Daemon Timer Unit Setup : Creating timer file for %s\n", p_unit);
    l_fd_create = g_creat(p_file, O_RDWR | O_CREAT);
    if(l_fd_create==-1){
	log_error_message("AL Daemon Timer Unit Setup : Cannot create timer file for %s\n", p_unit);
 	return;
    }
    log_debug_message("AL Daemon Timer Unit Setup : Timer file %s created !\n", p_file);
    /* open the key file for write */
    if ((l_fd_open = g_fopen(p_file, "r+"))==NULL) {
      log_error_message
	  ("AL Daemon Timer Unit Setup : Cannot open timer unit file %s for adding data !\n",
	   p_file);
      return;
    }
    /* the timer units are available only for reboot and shutdown */
    if (strcmp(p_unit, "reboot") == 0) {
      g_file_set_contents(p_file, l_reboot_entry, strlen(l_reboot_entry),
			  &l_error);
    }
    /* the timer units are available only for reboot and shutdown */
    if (strcmp(p_unit, "poweroff") == 0) {
      g_file_set_contents(p_file, l_shutdown_entry,
			  strlen(l_shutdown_entry), &l_error);
    }

   log_debug_message("AL Daemon Timer Unit Setup : File %s is wrote !\n",
		p_file);
   }
  }
  /* parse the key value file */
  GKeyFile *l_key_file = ParseUnitFile(p_file);
  log_debug_message("AL Daemon Unit Setup : Key file %s was parsed !\n",
		p_file);
  /* key file length */
  gsize l_file_length;
  log_debug_message("AL Daemon Unit Setup : Key file %s is checked !\n",
		p_file);
  /* test key file validity */
  if(!l_key_file){
	log_error_message("AL Daemon Unit Setup : Key file %s could not be parsed !\n",
		p_file);
	return;
  }
  log_debug_message("AL Daemon Unit Setup : Setup entry for unit type (timer/service) for %s!\n",
		p_file);
  /* modify the entries according input params  */
  if(strstr(p_file,".timer")==NULL){
  	/* if the unit file is an application service file */
  	/* the setup will be done in the Service group adding values for User and Group */
  	g_key_file_set_string(l_key_file, "Service", p_key, p_val);
  }else{
  	/* if the unit is a timer access the Timerstr group in the key file */
  	g_key_file_set_string(l_key_file, "Timer", p_key, p_val);
  }
  
  /* setup the error */
  GError *l_err = NULL;
  /* write new data to file */
  gchar *l_new_file_data =
      g_key_file_to_data(l_key_file, &l_file_length, &l_err);
  /* test if file is accessible */
  if (l_new_file_data == NULL) {
   /* if the key file is for a service file */
   if(strstr(p_file,".timer")==NULL){
    log_error_message
	("AL Daemon Service Unit Setup : Could not get new file data for service unit! (%d: %s)",
	 l_err->code, l_err->message);
    g_error_free(l_err);
    return;
   }else{ /* if the key file corresponds to timer file */
    log_error_message
	("AL Daemon Timer Unit Setup : Could not get new file data for timer unit! (%d: %s)",
	 l_err->code, l_err->message);
    g_error_free(l_err);
    return;
   }
  }
  /* setup the new content in the key value file */
  if (!g_file_set_contents(p_file, l_new_file_data, l_file_length, &l_err)) {
   /* if the key file corresponds to a service file */
   if(strstr(p_file,".timer")==NULL){
    log_error_message
	("AL Daemon Service Unit Setup : Could not save new file for service unit! (%d: %s)",
	 l_err->code, l_err->message);
    g_error_free(l_err);
    return;
   }else{ /* if the key file corresponds to timer file */
    log_error_message
	("AL Daemon Timer Unit Setup : Could not save new file for timer unit! (%d: %s)",
	 l_err->code, l_err->message);
    g_error_free(l_err);
    return;
   }
  }
}

/**
 * Function responsible for getting unit object path
 * NOTE: result must be freed using free() 
 * */
char *GetUnitObjectPath(DBusConnection *p_conn, char *p_unit_name)
{
  /* initialize message and reply */
  DBusMessage *l_msg = NULL, *l_reply = NULL;
  /* error definition*/
  DBusError l_error;
  /* initialize the path */
  const char *l_path = NULL;

  /* error initialization */
  dbus_error_init(&l_error);

  /* new method call to get unit information */
  if (NULL == (l_msg = dbus_message_new_method_call("org.freedesktop.systemd1",
                                                    "/org/freedesktop/systemd1",
                                                    "org.freedesktop.systemd1.Manager",
                                                    "GetUnit"))) 
  {
    log_error_message
            ("AL Daemon Get Unit Object Path : Could not allocate message for %s\n", p_unit_name);
    goto free_res;
  }

  /* append application name as argument to method call */
  if (FALSE == dbus_message_append_args(l_msg,
                                        DBUS_TYPE_STRING, &p_unit_name,
                                        DBUS_TYPE_INVALID))
  {
    log_error_message
            ("AL Daemon Get Unit Object Path : Could not append arguments to message for %s\n", p_unit_name);
    goto free_res;
  }

  /* send the message on the bus and wait for a reply */
  if (NULL == (l_reply = dbus_connection_send_with_reply_and_block(p_conn, l_msg, -1, &l_error))) 
  {
    log_error_message
            ("AL Daemon Get Unit Object Path : Unknown information for %s \n", p_unit_name);
    if (dbus_error_is_set(&l_error))
    {
      log_error_message
              ("AL Daemon Get Unit Object Path : Error [%s: %s]\n", l_error.name, l_error.message);
    }
    goto free_res;
  }

  /* extract arguments from the reply; the object path is useful for property fetch */
  if (FALSE == dbus_message_get_args(l_reply, &l_error,
                                     DBUS_TYPE_OBJECT_PATH, &l_path,
                                     DBUS_TYPE_INVALID)) 
  {
    log_error_message
            ("AL Daemon Get Unit Object Path : Failed to parse reply for %s\n", p_unit_name);
    if (dbus_error_is_set(&l_error))
    {
      log_error_message
              ("AL Daemon Get Unit Object Path : Error [%s: %s]\n", l_error.name, l_error.message);
    }
    goto free_res;
  }

  dbus_message_unref(l_msg);
  dbus_message_unref(l_reply);
  return strdup(l_path);

free_res:
  if (l_msg)
    dbus_message_unref(l_msg);
  if (l_reply)
    dbus_message_unref(l_reply);
  dbus_error_free(&l_error);
  return NULL;
}

/* 
 * Function responsible to setup the (fg/bg) state when starting the application
 * for the first time using Run or RunAs */
int SetupApplicationStartupState(DBusConnection *p_conn, char *p_app, bool l_fg_state)
{
  /* DBus message and reply for task change state */
  DBusMessage *l_msg_state, *l_reply_state;
  /* error */
  DBusError l_err;
  /* object path for the application */
  char *l_path;
  /* iterators for variant type embedding for state info */
  DBusMessageIter l_iter, l_variant;
  /* full unit name */
  char *l_unit = malloc(DIM_MAX*sizeof(l_unit));
  strcpy(l_unit, p_app);
  strcat(l_unit, ".service");
  /* error initialization */
  dbus_error_init(&l_err);
  /* get unit object path */ 
  if (NULL == (l_path = GetUnitObjectPath(p_conn, l_unit)))
  {
          log_error_message
                  ("AL Daemon Setup Application Startup State : Unable to extract object path for %s", l_unit);
          return -1;
  }
  log_debug_message
          ("AL Daemon Setup Application Startup State : Extracted object path for %s\n",
           l_unit);

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
    return -1;
  }
  log_debug_message
	("AL Daemon Setup Application Startup State  : Called property set method call for %s\n",
	 p_app);
   /* initialize the iterator for arguments append */
   dbus_message_iter_init_append(l_msg_state, &l_iter);
  log_debug_message
	("AL Daemon Setup Application Startup State  : Initialized iterator for property value setup method call for %s\n", l_unit);
   /* append interface for property setting */
   char *l_iface = "org.freedesktop.systemd1.Service";
   if(!dbus_message_iter_append_basic(&l_iter, 
				      DBUS_TYPE_STRING, 
				      &l_iface)){
	log_error_message
	("AL Daemon Setup Application Startup State : Could not append interface to message for %s \n",
	 p_app);
	return -1;
   }
   log_debug_message
	("AL Daemon Setup Application Startup State  : Appended interface name for property set for %s\n",
	 l_unit);
   /* append property name */
   char *l_prop = "Foreground";
   if(!dbus_message_iter_append_basic(&l_iter, 
				      DBUS_TYPE_STRING, 
				      &l_prop)){ 
	log_error_message
	("AL Daemon Setup Application Startup State : Could not append property to message for %s \n",
	 l_unit);
	return -1;
   }
   log_debug_message
	("AL Daemon Setup Application Startup State  : Appended property name to setup for %s\n",
	 l_unit);
   /* append the variant that stores the value for the foreground state property */
    if(!dbus_message_iter_open_container(&l_iter, 
				     DBUS_TYPE_VARIANT, 
				     DBUS_TYPE_BOOLEAN_AS_STRING,
				     &l_variant)){
	log_error_message("AL Daemon Setup Application Startup State : Not enough memory to open container \n", 0);
	return -1;
    }
    log_debug_message
	("AL Daemon Setup Application Startup State  : Opened container for property value setup for %s\n",
	 l_unit);
    dbus_bool_t l_state = (dbus_bool_t)l_fg_state;
    if(!dbus_message_iter_append_basic (&l_variant, 
				        DBUS_TYPE_BOOLEAN,  
                                        &l_state)){
    	log_error_message
	("AL Daemon Setup Application Startup State : Could not append property value to message for %s \n",
	 l_unit);
     	return -1;
    }
    log_debug_message
	("AL Daemon Setup Application Startup State  : Set the state (fg/bg) value in variant for %s\n",
	 l_unit);
    if(!dbus_message_iter_close_container (&l_iter, 
					   &l_variant)){
	log_error_message("AL Daemon Setup Application Startup State : Not enough memory to close container \n", 0);
	return -1;
    }
    log_debug_message
	("AL Daemon Setup Application Startup State  : Closed container for property value setup for %s\n",
	 l_unit);     
   /* wait for the reply from systemd after setting the state (fg/bg) property */
   if (!(l_reply_state =
       dbus_connection_send_with_reply_and_block(p_conn, l_msg_state,
						 -1, &l_err))) {
    	log_error_message
	("AL Daemon Method Call Listener : Didn't received a reply for state property method call: %s \n",l_err.message);
    if (l_msg_state)
      dbus_message_unref(l_msg_state);
    if (l_reply_state)
      dbus_message_unref(l_reply_state);
    dbus_error_free(&l_err);
    return -1;
   }
   log_debug_message("AL Daemon Method Call Listener Setup Application Startup State : Reply after setting state for %s was received!\n ", l_unit);
   if (NULL != l_path)
           free(l_path);
  return 0;
}

/* 
 * Function responsible to extract template name from service file name 
 * when running application with variable command line parameters.
 */
char *ExtractUnitNameTemplate(char *unit_name) {
        const char *l_p;
        char *l_res;
        size_t l_dim;
	/* test if template */
        if (!(l_p = strchr(unit_name, '@')))
                return strdup(unit_name);
        l_dim = l_p - unit_name + 1;
	/* init the result */
        if (!(l_res = (char *) malloc(sizeof(char)*(l_dim + 1)))){
		log_error_message("AL Daemon Template Name Extractor : Cannot allocate template handler for %s !\n", unit_name);
                return NULL;
	}
	/* extract the template name */
        strcpy(mempcpy(l_res, unit_name, l_dim),"");
        return l_res;
}

/* Function responsible to extract the service interface from the path.
 * Useful when determining which properties are available for the 
 * specific service of interest.
 */
gchar * GetInterfaceFromPath (gchar * unit_path){ 
    gchar * interface;    
    
        if (g_str_has_suffix(unit_path, "target")){
            interface = "org.freedesktop.systemd1.Target";
        }else if (g_str_has_suffix(unit_path, "socket")){
            interface = "org.freedesktop.systemd1.Socket";
        }else if (g_str_has_suffix(unit_path, "mount")){
            interface = "org.freedesktop.systemd1.Mount";
        }else if (g_str_has_suffix(unit_path, "swap")){
            interface = "org.freedesktop.systemd1.Swap";
        }else{
            interface = "org.freedesktop.systemd1.Service";
        }
        
        return interface;
}

