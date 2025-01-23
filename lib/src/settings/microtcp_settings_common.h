#ifndef MICROTCP_SETTINGS_COMMON_H
#define MICROTCP_SETTINGS_COMMON_H

#include <sys/time.h>
#include "microtcp.h"

#define DEFAULT_MICROTCP_ACK_TIMEOUT_SEC 0
#define DEFAULT_MICROTCP_ACK_TIMEOUT_USEC MICROTCP_ACK_TIMEOUT_US
#define DEFAULT_MICROTCP_ACK_TIMEOUT ((struct timeval){.tv_sec = DEFAULT_MICROTCP_ACK_TIMEOUT_SEC, \
                                                       .tv_usec = DEFAULT_MICROTCP_ACK_TIMEOUT_USEC})

#define DEFAULT_CONNECT_RST_RETRIES 3
#define LINUX_DEFAULT_ACCEPT_TIMEOUTS 5
#define MICROTCP_MSL_SECONDS 10 /* Maximum Segment Lifetime. Used for transitioning from TIME_WAIT -> CLOSED */
#define DEFAULT_SHUTDOWN_TIME_WAIT_SEC (2*MICROTCP_MSL_SECONDS)
#define DEFAULT_SHUTDOWN_TIME_WAIT_USEC 0
#define TCP_RETRIES2 15         /* TCP default value for `TCP_RETRIES2` variable. What is this? WELL... per TCP is the timeouts to wait in TIMEWAIT */

#endif /* MICROTCP_SETTINGS_COMMON_H */
