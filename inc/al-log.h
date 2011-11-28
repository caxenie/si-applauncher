/*
* al-log.h, contains logging definitions for AL
*
* Copyright (c) 2011 Wind River Systems, Inc.
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


#ifndef AL_LOG_H
#define AL_LOG_H

#include <syslog.h>
#define log_message(format,args...) syslog(LOG_INFO,format,args)
#define log_error_message(format,args...) syslog(LOG_ERR,format,args)
#define log_debug_message(format,args...) syslog(LOG_DEBUG,format,args)

#endif // AL_H
