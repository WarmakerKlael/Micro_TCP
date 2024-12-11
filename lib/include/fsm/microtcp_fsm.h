#ifndef STATE_MACHINES_H
#define STATE_MACHINES_H

#include "microtcp.h"

/* Accept()'s FSM settings. */
size_t get_accept_synack_retries(void);
void set_accept_synack_retries(size_t _retries_count);

/* Shutdown()'s FSM settings. */
size_t get_shutdown_finack_retries(void);
void set_shutdown_finack_retries(size_t _retries_count);
void get_shutdown_time_wait_period(time_t *_sec, time_t *_usec);
void set_shutdown_time_wait_period(time_t _sec, time_t _usec);

int microtcp_connect_fsm(microtcp_sock_t *_socket, const struct sockaddr *_address, socklen_t _address_len);
int microtcp_accept_fsm(microtcp_sock_t *_socket, struct sockaddr *_address, socklen_t _address_len);
int microtcp_shutdown_fsm(microtcp_sock_t *_socket, struct sockaddr *_address, socklen_t _address_len);
// int receive_fsm(microtcp_sock_t *const _socket, struct sockaddr *_address, socklen_t _address_len);

#endif /* STATE_MACHINES_H */