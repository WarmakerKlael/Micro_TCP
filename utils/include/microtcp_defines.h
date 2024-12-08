#ifndef MICROTCP_DEFINES_H
#define MICROTCP_DEFINES_H

#define PROJECT_NAME "MicroTCP"

#define TRUE 1
#define FALSE 0

/* Priority numbers of constructors/destructors. */
#define LOGGER_CONSTRUCTOR_PRIORITY 1001
#define LOGGER_DESTRUCTOR_PRIORITY LOGGER_CONSTRUCTOR_PRIORITY
#define RNG_CONSTRUCTOR_PRIORITY 1002
#define RNG_DESTRUCTOR_PRIOTITY RNG_CONSTRUCTOR_PRIORITY

/* POSIX's socket() function returns a file descriptor in successful calls. */
#define POSIX_SOCKET_FAILURE_VALUE -1

/* microtcp_bind() possible return values. */
#define MICROTCP_BIND_SUCCESS 0
#define MICROTCP_BIND_FAILURE -1

/* microtcp_connect() possible return values. */
#define MICROTCP_CONNECT_SUCCESS 0
#define MICROTCP_CONNECT_FAILURE -1

/* microtcp_accept() possible return values. */
#define MICROTCP_ACCEPT_SUCCESS 0
#define MICROTCP_ACCEPT_FAILURE -1

/* POSIX's bind() possible return values. */
#define POSIX_BIND_SUCCESS 0
#define POSIX_BIND_FAILURE -1

/* PROJECT_TOP_LEVEL_DIRECTORY is defined by Top Level CMakeLists.txt
   I inlclude the following macro to make Intellisense stop showing error.*/
#ifdef __INTELLISENSE__
#define PROJECT_TOP_LEVEL_DIRECTORY ""
#endif

#endif /* MICROTCP_DEFINES_H */