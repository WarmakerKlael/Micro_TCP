#ifndef MICROTCP_DEFINES_H
#define MICROTCP_DEFINES_H

#include "microtcp_helper_macros.h"

#define TRANSPORT_PROTOCOL_NAME "μTCP"

#define WIN_BIT (1 << 11) /* Requests window size from peer (intented to be used when peer's window is 0). */
#define ACK_BIT (1 << 12)
#define RST_BIT (1 << 13)
#define SYN_BIT (1 << 14)
#define FIN_BIT (1 << 15)

#define DATA_SEGMENT_CONTROL_FLAGS ACK_BIT

/* In TCP, segments containing control flags (e.g., SYN, FIN),
 * other than pure ACKs, are treated as carrying a virtual payload.
 * That is the reason for the +1 increment.
 */
#define SYN_SEQ_NUMBER_INCREMENT 1
#define FIN_SEQ_NUMBER_INCREMENT 1

/* Priority numbers of constructors/destructors. */
#define LOGGER_CONSTRUCTOR_PRIORITY 1001
#define LOGGER_DESTRUCTOR_PRIORITY LOGGER_CONSTRUCTOR_PRIORITY
#define RNG_CONSTRUCTOR_PRIORITY 1002
#define RNG_DESTRUCTOR_PRIOTITY RNG_CONSTRUCTOR_PRIORITY
#define SOCKET_MANAGER_CONSTRUCTOR_PRIORITY 1003
#define SOCKET_MANAGER_DESTRUCTOR_PRIORITY SOCKET_MANAGER_CONSTRUCTOR_PRIORITY

/* POSIX's socket() function returns a file descriptor in successful calls. */
#define POSIX_SOCKET_FAILURE_VALUE -1

/* microtcp_bind() possible return values. */
#define MICROTCP_BIND_SUCCESS 0
#define MICROTCP_BIND_FAILURE -1

/* microtcp_connect() possible return values. (and its FSM) */
#define MICROTCP_CONNECT_SUCCESS 0
#define MICROTCP_CONNECT_FAILURE -1

/* microtcp_accept() possible return values. (and its FSM) */
#define MICROTCP_ACCEPT_SUCCESS 0
#define MICROTCP_ACCEPT_FAILURE -1

/* microtcp_shutdown() possible return values. (and its FSM) */
#define MICROTCP_SHUTDOWN_SUCCESS 0
#define MICROTCP_SHUTDOWN_FAILURE -1

/* microtcp_recv() possible return values. (and its FSM) */
#define MICROTCP_SEND_FAILURE -1

/* microtcp_recv() possible return values. (and its FSM) */
#define MICROTCP_RECV_TIMEOUT 0
#define MICROTCP_RECV_FAILURE -1

/* POSIX's bind() possible return values. */
#define POSIX_BIND_SUCCESS 0
#define POSIX_BIND_FAILURE -1

/* POSIX's setsockopt() possible return values. */
#define POSIX_SETSOCKOPT_SUCCESS 0
#define POSIX_SETSOCKOPT_FAILURE -1

/* PROJECT_TOP_LEVEL_DIRECTORY is defined by Top Level CMakeLists.txt
   I inlclude the following macro to make Intellisense stop showing error.*/
#ifdef __INTELLISENSE__
#define PROJECT_TOP_LEVEL_DIRECTORY ""
#endif

#define RECVFROM_SHUTDOWN (0)
#define RECVFROM_ERROR (-1)
#define SENDTO_ERROR (-1)

#endif /* MICROTCP_DEFINES_H */

_Static_assert(MICROTCP_RECV_TIMEOUT == 0, "Unexpected return value(" STRINGIFY_EXPANDED(MICROTCP_RECV_TIMEOUT) ") for timeout in microtcp_recv().");
_Static_assert(MICROTCP_RECV_FAILURE == -1, "Unexpected return value(" STRINGIFY_EXPANDED(MICROTCP_RECV_FAILURE) ") for failure in microtcp_recv().");
