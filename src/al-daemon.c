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
#include <fcntl.h>
#include <getopt.h>
#include <glib/gstdio.h>
#include <glib.h>
#include <libgen.h>
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

/* Function responsible with the command line interface output */
void AlPrintCLI()
{
  fprintf(stdout,
	  "Syntax: \n"
	  "   al-daemon --start|-S options \n"
	  "   al-daemon --stop|-K\n"
	  "   al-daemon --version|-V\n"
	  "   al-daemon --help|-H\n"
	  "\n"
	  "Options: \n"
	  "   --verbose|-v   prints the internal daemon log messages\n");
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
      fprintf(stdout, "\nAL Daemon version %s\n", AL_VERSION);
      exit(0);
    case 'v':			/* enable verbose */
      g_verbose = 1;
      break;
    default:
      AlPrintCLI();
      return;
    }
  }
  if (argc < 2)
    AlPrintCLI();
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
    printf
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
 * It searches for the associated .service file for a string representing a name.
 */
int AppExistsInSystem(char *p_app_name)
{
  /* full name string */
  char full_name[DIM_MAX];
  /* get the full path name */
  sprintf(full_name, "/lib/systemd/system/%s.service", p_app_name);
  /* contains stat info */
  struct stat file_stat;
  int ret = -1;
  /* get stat information */
  ret = stat(full_name, &file_stat);
  if (!ret) {
    /* file exists */
    return 1;
  }
  /* file not found */
  return 0;
}

/* Function responsible to parse the .timer unit and extract the triggering key */
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
      (l_out_new_key_file, p_file, G_KEY_FILE_NONE, &l_err)) {
    log_message
	("AL Daemon Unit File Parser : Cannot load from timer unit key file! (%d: %s)\n",
	 l_err->code, l_err->message);
    return NULL;
    g_error_free(l_err);
  }
  /* extract groups from key file structure */
  l_groups = g_key_file_get_groups(l_out_new_key_file, &l_groups_length);
  if (l_groups == NULL) {
    log_message
	("AL Daemon Unit File Parser : Could not retrieve groups from %s timer unit file!\n",
	 p_file);
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
      log_message
	  ("AL Daemon Unit File Parser : Error in retrieving keys in timer unit file! (%d: %s)",
	   l_err->code, l_err->message);
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
	  log_message
	      ("AL Daemon Unit File Parser : Error retrieving key's value in timer unit file. (%d, %s)\n",
	       l_err->code, l_err->message);
	  g_error_free(l_err);
	}
      }
    }
  }
  /* return the parsed key file structure */
  return l_out_new_key_file;
}

/* Function responsible to parse the .timer unit and setup the triggering key value */
void SetupUnitFileKey(char *p_file, char *p_key, char *p_val, char *p_unit)
{
  /* key file handlers */
  int l_fd;

  log_message
      ("AL Daemon Timer Unit Setup : Entered the Setup timer unit sequence for %s \n",
       p_file);

  /* test file existence and create if not exists */
  if (!g_file_test(p_file, G_FILE_TEST_EXISTS)) {
    /* initialize the error */
    GError *l_error = NULL;
    /* entries for the special reboot and poweroff units */
    char *l_reboot_entry =
	"[Unit]\n Description=Timer for deferred reboot\n [Timer]\n OnActiveSec=0s\n Unit=reboot.service\n";
    char *l_shutdown_entry =
	"[Unit]\n Description=Timer for deferred shutdown\n [Timer]\n OnActiveSec=0s\n Unit=poweroff.service\n";
    /* create the key file with permissions */
    l_fd = creat(p_file, 777);
    log_message("AL Daemon : File %s created !\n", p_file);
    /* open the key file for write */
    if (!g_fopen(p_file, "w")) {
      log_message
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

    log_message("AL Daemon Timer Unit Setup : File %s is wrote !\n",
		p_file);
  }
  /* parse the key value file */
  GKeyFile *l_key_file = ParseUnitFile(p_file);
  gsize l_file_length;

  /* modify the entries according input params  */
  g_key_file_set_string(l_key_file, "Timer", p_key, p_val);

  GError *l_err = NULL;
  /* write new data to file */
  gchar *l_new_file_data =
      g_key_file_to_data(l_key_file, &l_file_length, &l_err);
  /* free the file  */
  g_key_file_free(l_key_file);
  /* test if file is accessible */
  if (l_new_file_data == NULL) {
    log_message
	("AL Daemon Timer Unit Setup : Could not get new file data for timer unit! (%d: %s)",
	 l_err->code, l_err->message);
    g_error_free(l_err);
    return;
  }
  /* setup the new content in the key value file */
  if (!g_file_set_contents(p_file, l_new_file_data, l_file_length, &l_err)) {
    log_message
	("AL Daemon Timer Unit Setup : Could not save new file for timer unit! (%d: %s)",
	 l_err->code, l_err->message);
    g_error_free(l_err);
    return;
  }
}

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

void AlAppStateNotifier(DBusConnection * p_conn, char *p_app_name)
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

/* High level interface for the AL Daemon */
void Run(int p_newPID, bool p_isFg, int p_parentPID, char *p_commandLine)
{
  // TODO isFg and parentPID usage when starting applications
  // [out] newPID: INT32, isFg: BOOLEAN, parentPID: INT32, commandLine: STRING
  /* store the return code */
  int l_ret;
  /* application PID */
  int l_pid;
  log_message("%s started with run !\n", p_commandLine);
  /* the command line for the application */
  char l_cmd[DIM_MAX];
  /* form the call string for systemd */
  sprintf(l_cmd, "systemctl start %s.service", p_commandLine);
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
  /* systemd invocation */
  l_ret = system(l_cmd);
  if (l_ret == -1) {
    log_message
	("AL Daemon : Application cannot be started with run! Err: %s\n",
	 strerror(errno));
    return;
  }
  /* extract the PID from the application name only if the start is valid */
  l_pid = (int)AppPidFromName(p_commandLine);
  p_newPID = l_pid;
  log_message("%s was started with run and has PID : %d\n", p_commandLine, p_newPID);
}

void RunAs(int p_egid, int p_euid, int p_newPID, bool p_isFg, int p_parentPID,
	   char *p_commandLine)
{
  // TODO egid, euid, isFg and parentPID usage when starting applications 
  // egid: INT32, euid: INT32, [out] newPID: INT32, isFg: BOOLEAN, parentPID: INT32, commandLine: STRING
  /* store the return code */
  int l_ret;
  /* application PID */
  int l_pid;
  log_message("%s started with runas !\n", p_commandLine);
  /* the command line for the application */
  char l_cmd[DIM_MAX];
  /* form the call string for systemd */
  sprintf(l_cmd, "systemctl start %s.service", p_commandLine);
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
  /* systemd invocation */
  l_ret = system(l_cmd);
  if (l_ret == -1) {
    log_message
	("AL Daemon : Application cannot be started with runas! Err: %s\n",
	 strerror(errno));
     return;
  }
  /* extract the PID from the application name only if the start is valid */
  l_pid = (int)AppPidFromName(p_commandLine);
  p_newPID = l_pid;
  log_message("%s was started with runas and has PID : %d\n", p_commandLine, p_newPID);
}

void Suspend(int p_pid)
{
  /* store the return code */
  int l_ret;
  /* stores the application name */
  char l_app_name[DIM_MAX];

  /* command line for the application */
  char *l_commandLine;
  /* extract the application name from the pid */
  if(AppNameFromPid(p_pid, l_app_name)!=0){
  l_commandLine = l_app_name;
  char l_cmd[DIM_MAX];
  log_message("%s canceled with suspend !\n", l_commandLine);
  /* form the systemd command line string */
  sprintf(l_cmd, "systemctl cancel %s.service", l_commandLine);
  /* call systemd */
  l_ret = system(l_cmd);

  if (l_ret == -1) {
    log_message
	("AL Daemon : Application cannot be suspended! Err:%s\n",
	 strerror(errno));
   }
  }else{
	log_message("AL Daemon : Application cannot be suspended because is already stopped !%s", "\n"); 
 }
}

void Resume(int p_pid)
{
  /* store the return code */
  int l_ret;
  /* stores the application name */
  char l_app_name[DIM_MAX];
  /* command line for the application */
  char *l_commandLine;
  /* extract the application name from the pid */
  if(AppNameFromPid(p_pid, l_app_name)!=0){
  l_commandLine = l_app_name;
  char l_cmd[DIM_MAX];
  log_message("%s restarted with resume !\n", l_commandLine);
  /* form the systemd command line string */
  sprintf(l_cmd, "systemctl restart %s.service", l_commandLine);
  /* call systemd */
  l_ret = system(l_cmd);

  if (l_ret == -1) {
    log_message("AL Daemon : Application cannot be resumed! Err:%s\n",
		strerror(errno));
    }
   }else{
	log_message("AL Daemon : Application cannot be resumed because is stopped !%s", "\n"); 
 }
}

void Stop(int p_pid)
{
  /* store the return code */
  int l_ret;
  /* stores the application name */
  char l_app_name[DIM_MAX];
  /* command line for the application */
  char *l_commandLine;
  if(AppNameFromPid(p_pid, l_app_name)!=0){
  l_commandLine = l_app_name;
  char l_cmd[DIM_MAX];
  log_message("%s stopped with stop !\n", l_app_name);
  /* form the systemd command line string */
  sprintf(l_cmd, "systemctl stop %s.service", l_app_name);
  /* call systemd */
  l_ret = system(l_cmd);

  if (l_ret != 0) {
    log_message("AL Daemon : Application cannot be stopped! Err:%s\n",
		strerror(errno));
   }
  }else{
	log_message("AL Daemon : Application cannot be stopped because is already stopped !%s", "\n"); 
 }
}

void StopAs(int p_pid, int p_euid, int p_egid){ 
 // TODO Read the euid / egid information from service file and compare to input param
 /* store the return code */
  int l_ret;
  /* stores the application name */
  char l_app_name[DIM_MAX];
  /* command line for the application */
  char *l_commandLine;
  if(AppNameFromPid(p_pid, l_app_name)!=0){
  l_commandLine = l_app_name;
  char l_cmd[DIM_MAX];
  log_message("%s stopped with stop !\n", l_app_name);
  /* form the systemd command line string */
  sprintf(l_cmd, "systemctl stop %s.service", l_app_name);
  /* call systemd */
  l_ret = system(l_cmd);

  if (l_ret != 0) {
    log_message("AL Daemon : Application cannot be stopped! Err:%s\n",
		strerror(errno));
   }
  }else{
	log_message("AL Daemon : Application cannot be stopped because is already stopped !%s", "\n"); 
 }
}

void TaskStarted(char *p_imagePath, int p_pid)
{
  log_message
      ("AL Daemon : Task %d %s was started and signal %s was emitted!\n",
       p_pid, p_imagePath, AL_SIGNAME_TASK_STARTED);
}

void TaskStopped(char *p_imagePath, int p_pid)
{
  log_message
      ("AL Daemon : Task %d %s was stopped and signal %s was emitted!\n",
       p_pid, p_imagePath, AL_SIGNAME_TASK_STOPPED);
}

/* Function responsible with restarting an application when the SHM component detects
 * an abnormal operation of the application
 */
void Restart(char *p_app_name)
{
  /* return code for system call */
  int l_ret;
  /* the command line for the application */
  char l_cmd[DIM_MAX];
  log_message("%s will be restarted !\n", p_app_name);
  sprintf(l_cmd, "systemctl restart %s.service", p_app_name);
  /* systemd invocation */
  l_ret = system(l_cmd);
  if (l_ret != 0) {
    log_message
	("AL Daemon : Application cannot be restarted with restart! Err: %s\n",
	 strerror(errno));
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

  /* FIXME adjust the pid after extracting it from ExecMainPID */
  l_pid = l_pid + 1;

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

/* Receive the method calls and reply */
void AlReplyToMethodCall(DBusMessage * p_msg, DBusConnection * p_conn)
{
  /* reply message */
  DBusMessage *l_reply;
  /* reply arguments */
  DBusMessageIter l_args;
  bool l_stat = true;
  /* reply status */
  dbus_uint32_t l_level = 21614;
  dbus_uint32_t l_serial = 0;
  /* initialized parameter */
  char *l_param = "";

  /* read the arguments */
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

  /* create a reply from the message */
  l_reply = dbus_message_new_method_return(p_msg);

  /* add the arguments to the reply */
  dbus_message_iter_init_append(l_reply, &l_args);
  if (!dbus_message_iter_append_basic(&l_args, DBUS_TYPE_BOOLEAN, &l_stat)) {
    log_message
	("AL Daemon Reply to Method Call : Arg Bool Out Of Memory!%s",
	 "\n");
    return;
  }
  if (!dbus_message_iter_append_basic(&l_args, DBUS_TYPE_UINT32, &l_level)) {
    log_message
	("AL Daemon Reply to Method Call : Arg Int Out Of Memory!%s",
	 "\n");
    return;
  }
  /* send the reply && flush the connection */
  if (!dbus_connection_send(p_conn, l_reply, &l_serial)) {
    log_message
	("AL Daemon Reply to Method Call : Connection Out Of Memory!%s",
	 "\n");
    return;
  }
  /* flush the connection */
  dbus_connection_flush(p_conn);

  /* free the reply */
  dbus_message_unref(l_reply);
}

/* Server that exposes a method call and waits for it to be called */
void AlListenToMethodCall()
{
  /* define message and reply */
  DBusMessage *l_msg, *l_reply, *l_msg_notif, *l_reply_notif;
  /* define connection for default method calls and notification signals */
  DBusConnection *l_conn;
  /* error definition */
  DBusError l_err;
  /* return code */
  int l_ret, l_r;
  /* auxiliary storage for name handling */
  char *l_app;
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
  
  log_message
      ("AL Daemon Method Call Listener : Listening for method calls!%s",
       "\n");

  /* initialise the error */
  dbus_error_init(&l_err);

  /* connect to the bus and check for errors */
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
    return;
  }
  /* request our name on the bus and check for errors */
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
	("AL Daemon Method Call Listener : Not Primary Owner (%d)! \n",
	 l_ret);
    log_message
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
    log_message
	("AL Daemon Method Call Listener : Failed to add signal matcher: %s\n",
	 l_err.message);
    if (l_msg)
      dbus_message_unref(l_msg);
    dbus_error_free(&l_err);
  }
  /* add matcher for method calls */
  dbus_bus_add_match(l_conn,
		     "type='method_call',"
		     "interface='org.GENIVI.AppL.method'", &l_err);

  if (dbus_error_is_set(&l_err)) {
    log_message
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
    log_message
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
    log_message
	("AL Daemon Method Call Listener : Failed to issue method call: %s \n",
	 l_err.message);
    if (l_msg_notif)
      dbus_message_unref(l_msg_notif);
    if (l_reply_notif)
      dbus_message_unref(l_reply_notif);
    dbus_error_free(&l_err);
  }

  /* loop, testing for new messages */
  while (true) {
    /* non blocking read of the next available message */
    dbus_connection_read_write(l_conn, 0);
    l_msg = dbus_connection_pop_message(l_conn);

    /* loop again if we haven't got a message */
    if (NULL == l_msg) {
      sleep(1);
      continue;
    }

    /* check if this is a method call for the right interface amd method */
    if (dbus_message_is_method_call(l_msg, AL_METHOD_INTERFACE, "Run")) {
      AlReplyToMethodCall(l_msg, l_conn);
      /* extract application name and arguments */
      dbus_message_get_args(l_msg, &l_err, DBUS_TYPE_STRING,
			    &l_app, DBUS_TYPE_STRING, &l_app_args,
			    DBUS_TYPE_INVALID);
      log_message("AL Daemon Method Call Listener : Run app: %s\n", l_app);
      /* if reboot / shutdown unit add deferred functionality in timer file */
      if (strcmp(l_app, "reboot") == 0) {
	SetupUnitFileKey("/lib/systemd/system/reboot.timer",
			 "OnActiveSec", l_app_args, "reboot");
      }
      if (strcmp(l_app, "poweroff") == 0) {
	SetupUnitFileKey("/lib/systemd/system/poweroff.timer",
			 "OnActiveSec", l_app_args, "poweroff");
      }

      /* check for application service file existence */
      if (!AppExistsInSystem(l_app)) {
	log_message
	    ("AL Daemon Method Call Listener : Cannot run %s !\n Application %s is not found in the system !\n",
	     l_app, l_app);
	continue;
      } else {
	if ((l_r = (int) AppPidFromName(l_app)) != 0) {
	  log_message
	      ("AL Daemon Method Call Listener : Cannot run %s !\n",
	       l_app);

	  /* check the application current state before starting it */
	  /* extract the application state for testing existence */
	  if (AlGetAppState
	      (l_conn, strcat(l_app, ".service"), l_state_info) == 0) {

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
	      log_message
		  ("AL Daemon Method Call Listener : Cannot run %s !\n Application %s is already running in the system !\n",
		   l_app, l_app);
	      continue;
	    }
	  }
	}
      }
      /* run the application */
      Run(l_pid, true, 0, l_app);
    }

    /* check if this is a method call for the right interface amd method */
    if (dbus_message_is_method_call(l_msg, AL_METHOD_INTERFACE, "RunAs")) {
      AlReplyToMethodCall(l_msg, l_conn);
      /* extract application name and arguments */
      dbus_message_get_args(l_msg, &l_err, DBUS_TYPE_STRING,
			    &l_app, DBUS_TYPE_STRING, &l_app_args,
			    DBUS_TYPE_INVALID);
      /* if reboot / shutdown unit add deferred functionality in timer file */
      log_message("AL Daemon Method Call Listener : RunAs app: %s\n",
		  l_app);
      if (strcmp(l_app, "reboot") == 0) {
	SetupUnitFileKey("/lib/systemd/system/reboot.timer",
			 "OnActiveSec", l_app_args, "reboot");
      }
      if (strcmp(l_app, "poweroff") == 0) {
	SetupUnitFileKey("/lib/systemd/system/poweroff.timer",
			 "OnActiveSec", l_app_args, "poweroff");
      }
      /* check for application service file existence */
      if (!AppExistsInSystem(l_app)) {
	log_message
	    ("AL Daemon Method Call Listener : Cannot runas %s !\n Application %s is not found in the system !\n",
	     l_app, l_app);
	continue;
      } else {
	if ((l_r = (int) AppPidFromName(l_app)) != 0) {
	  log_message
	      ("AL Daemon Method Call Listener : Cannot runas %s !\n",
	       l_app);

	  /* check the application current state before starting it */
	  /* extract the application state for testing existence */
	  if (AlGetAppState
	      (l_conn, strcat(l_app, ".service"), l_state_info) == 0) {

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
	      log_message
		  ("AL Daemon Method Call Listener : Cannot runas %s !\n Application %s is already running in the system !\n",
		   l_app, l_app);
	      continue;
	    }
	  }
	}
      }
      /* runas the application */
      RunAs(0, 0, l_pid, true, 0, l_app);
    }

    /* check if this is a method call for the right interface amd method */
    if (dbus_message_is_method_call(l_msg, AL_METHOD_INTERFACE, "Stop")) {
      AlReplyToMethodCall(l_msg, l_conn);
      /* extract application name */
      dbus_message_get_args(l_msg, &l_err, DBUS_TYPE_STRING,
			    &l_app, DBUS_TYPE_INVALID);
      log_message
	  ("AL Daemon Method Call Listener : Stopping application %s\n",
	   l_app);
      /* extract application pid from its name */
      if (!(l_r = (int) AppPidFromName(l_app))) {
	/* test for application service file existence */
	if (!AppExistsInSystem(l_app)) {
	  log_message
	      ("AL Daemon Method Call Listener : Cannot stop %s !\n Application %s is not found in the system !\n",
	       l_app, l_app);
	  continue;
	}
	log_message
	    ("AL Daemon Method Call Listener : Cannot stop %s !\n",
	     l_app, l_app);

	/* check the application current state before starting it */
	/* extract the application state for testing existence */
	if (AlGetAppState(l_conn, strcat(l_app, ".service"), l_state_info)
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

	  log_message
	      ("AL Daemon Method Call Listener : Cannot stop %s !\n Application %s is already stopped !\n",
	       l_app, l_app);
	  continue;
	}
      }
      /* stop the application */
      Stop(0, 0, (int) AppPidFromName(l_app));
    }

    /* check if this is a method call for the right interface amd method */
    if (dbus_message_is_method_call(l_msg, AL_METHOD_INTERFACE, "Resume")) {
      AlReplyToMethodCall(l_msg, l_conn);
      /* extract the application name */
      dbus_message_get_args(l_msg, &l_err, DBUS_TYPE_STRING,
			    &l_app, DBUS_TYPE_INVALID);
      log_message
	  ("AL Daemon Method Call Listener : Resuming application %s\n",
	   l_app);
      /* extract the application pid from its name */
      if (!(l_r = (int) AppPidFromName(l_app))) {
	/* test for application service file existence */
	if (!AppExistsInSystem(l_app)) {
	  log_message
	      ("AL Daemon Method Call Listener : Cannot resume %s !\n Application %s is not found in the system !\n",
	       l_app, l_app);
	  continue;
	}
	log_message
	    ("AL Daemon Method Call Listener : Cannot resume %s !\n",
	     l_app, l_app);

	/* check the application current state before starting it */
	/* extract the application state for testing existence */
	if (AlGetAppState(l_conn, strcat(l_app, ".service"), l_state_info)
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
	    log_message
		("AL Daemon Method Call Listener : Cannot run %s !\n Application %s is already running in the system !\n",
		 l_app, l_app);
	    continue;
	  }
	}
      }
      /* resume the application */
      Resume((int) AppPidFromName(l_app));

    }

    /* check if this is a method call for the right interface amd method */
    if (dbus_message_is_method_call(l_msg, AL_METHOD_INTERFACE, "Suspend")) {
      AlReplyToMethodCall(l_msg, l_conn);
      /* extract the application name */
      dbus_message_get_args(l_msg, &l_err, DBUS_TYPE_STRING,
			    &l_app, DBUS_TYPE_INVALID);
      log_message
	  ("AL Daemon Method Call Listener : Suspending application %s\n",
	   l_app);
      /* extract the application pid from its name */
      if (!(l_r = (int) AppPidFromName(l_app))) {
	/* test for application service file existence */
	if (!AppExistsInSystem(l_app)) {
	  log_message
	      ("AL Daemon Method Call Listener : Cannot suspend %s !\n Application %s is not found in the system !\n",
	       l_app, l_app);
	  continue;
	}
	log_message
	    ("AL Daemon Method Call Listener : Cannot suspend %s !\n",
	     l_app, l_app);

	/* check the application current state before starting it */
	/* extract the application state for testing existence */
	if (AlGetAppState(l_conn, strcat(l_app, ".service"), l_state_info)
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

	  log_message
	      ("AL Daemon Method Call Listener : Cannot suspend %s !\n Application %s is already suspended !\n",
	       l_app, l_app);
	  continue;
	}
      }
      /* suspend the application */
      Suspend((int) AppPidFromName(l_app));
    }

   /* check if this is a method call for the right interface amd method */
    if (dbus_message_is_method_call(l_msg, AL_METHOD_INTERFACE, "Restart")) {
      AlReplyToMethodCall(l_msg, l_conn);
      /* extract application name and arguments */
      dbus_message_get_args(l_msg, &l_err, DBUS_TYPE_STRING,
			    &l_app, DBUS_TYPE_STRING, &l_app_args,
			    DBUS_TYPE_INVALID);
      log_message("AL Daemon Method Call Listener : Restart app: %s\n", l_app);
      /* check for application service file existence */
      if (!AppExistsInSystem(l_app)) {
	log_message
	    ("AL Daemon Method Call Listener : Cannot restart %s !\n Application %s is not found in the system !\n",
	     l_app, l_app);
	continue;
      } 
      Restart(l_app); 
    }

    /* consider only property change notification signals */
    if (dbus_message_is_signal
	(l_msg, "org.freedesktop.DBus.Properties", "PropertiesChanged")) {
      /* handling variables for object path, interface and property */
      const char *l_path, *l_interface, *l_property = "Id";
      /* initialize reply iterators */
      DBusMessageIter l_iter, l_sub_iter;

      log_message
	  ("AL Daemon Signal Listener : An application changed state !%s",
	   "\n");
      /* get object path for message */
      l_path = dbus_message_get_path(l_msg);
      /* extract the interface name from message */
      if (!dbus_message_get_args(l_msg, &l_err,
				 DBUS_TYPE_STRING, &l_interface,
				 DBUS_TYPE_INVALID)) {
	log_message
	    ("AL Daemon Method Call Listener : Failed to parse message: %s",
	     l_err.message);
	if (l_msg)
	  dbus_message_unref(l_msg);

	dbus_error_free(&l_err);
	continue;
      }
      /* filter only the unit and service specific interfaces */
      if ((strcmp(l_interface, "org.freedesktop.systemd1.Unit") != 0)
	  && (strcmp(l_interface, "org.freedesktop.systemd1.Service") !=
	      0)) {
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
	log_message
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
	log_message
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
	log_message
	    ("AL Daemon Signal Listener : Failed to issue method call: %s",
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
	log_message
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
	  log_message
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
	log_message("AL Daemon Method Call Listener : Unit %s changed.\n",
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
}

/* application launcher daemon entrypoint */
int main(int argc, char **argv)
{
  /* setup a PID and a SID for our AL Interface daemon */
  pid_t l_al_pid, l_al_sid;
  int l_ret;

  /* fork off the parent process */
  l_al_pid = fork();
  if (l_al_pid < 0) {
    log_message("AL Daemon : Cannot fork off parent process!%s", "\n");
    exit(EXIT_FAILURE);
  }

  /* if the PID is ok then exit the parent process */
  if (l_al_pid > 0) {
    exit(EXIT_FAILURE);
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
  AlParseCLIOptions(argc, argv);

  if (g_stop) {
    fprintf(stdout, "AL Daemon : Daemon process was stopped !\n");
    system("killall -9 al-daemon");
    return 0;
  }

  if (g_start) {
    fprintf(stdout, "AL Daemon : Daemon process was started !\n");
    AlListenToMethodCall();
  }

  return 0;
}
