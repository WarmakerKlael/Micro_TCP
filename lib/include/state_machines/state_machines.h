#ifndef STATE_MACHINES_H
#define STATE_MACHINES_H

#include "microtcp.h"

int microtcp_connect_state_machine(microtcp_sock_t *_socket, const struct sockaddr *const _address, socklen_t _address_len);
int microtcp_accept_state_machine(microtcp_sock_t *_socket, struct sockaddr *const _address, socklen_t _address_len);

#endif /* STATE_MACHINES_H */