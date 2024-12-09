#ifndef STATE_MACHINES_H
#define STATE_MACHINES_H

#include "microtcp.h"

int microtcp_connect_fsm(microtcp_sock_t *_socket, const struct sockaddr *_address, socklen_t _address_len);
int microtcp_accept_fsm(microtcp_sock_t *_socket, struct sockaddr *_address, socklen_t _address_len);
int microtcp_shutdown_fsm(microtcp_sock_t *_socket, struct sockaddr *_address, socklen_t _address_len);
int receive_fsm(microtcp_sock_t *const _socket, struct sockaddr *_address, socklen_t _address_len);

#endif /* STATE_MACHINES_H */