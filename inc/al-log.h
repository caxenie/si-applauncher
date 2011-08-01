#ifndef AL_LOG_H
#define AL_LOG_H

#include <syslog.h>
#define log_message(format,args...) syslog(LOG_INFO,format,args)
#define log_error_message(format,args...) syslog(LOG_ERR,format,args)
#define log_debug_message(format,args...) syslog(LOG_DEBUG,format,args)

#endif // AL_H
