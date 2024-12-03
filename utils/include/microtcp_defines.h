#ifndef MICROTCP_DEFINES_H
#define MICROTCP_DEFINES_H

#define PROJECT_NAME "Micro_TCP"
#define TRUE 1
#define FALSE 0

/* POSIX's socket() function returns a file descriptor in successful calls. */
#define POSIX_SOCKET_FAILURE_VALUE -1

/* microtcp_bind() possible return values. */
#define MICROTCP_BIND_SUCCESS 0
#define MICROTCP_BIND_FAILURE -1

/* microtcp_connect() possible return values. */
#define MICROTCP_CONNECT_SUCCESS 0
#define MICROTCP_CONNECT_FAILURE -1

/* POSIX's bind() possible return values. */
#define POSIX_BIND_SUCCESS 0
#define POSIX_BIND_FAILURE -1

#endif /* MICROTCP_DEFINES_H */