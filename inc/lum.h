/* 
* lum.h, contains the declarations for the daemon last user mode functionality
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

/* Function responsible to parse the service unit and extract ownership info */
extern void ExtractOwnershipInfo(char *euid, char *egid, char *file);
/* Function responsible to extract the user name from the uid */
extern int MapUidToUser(int uid, char *user);
/* Function responsible to extract the group name from the gid */
extern int MapGidToGroup(int gid, char *group);
/* Function responsible to get the current user as specified in the current_user GConf key and start the last user mode apps */
extern int GetCurrentUser(GConfClient* client, GConfEntry* key, char *user);
/* Function responsible to start the specific applications for the current user mode */
extern int StartUserModeApps(GConfClient *client, char *user);
/* Function responsible to initialize the last user mode at daemon startup */
extern int InitializeLastUserMode();
