#ifndef STATE_MACHINES_H
#define STATE_MACHINES_H

#include "microtcp.h"

int microtcp_connect_fsm(microtcp_sock_t *_socket, const struct sockaddr *_address, socklen_t _address_len);
int microtcp_accept_fsm(microtcp_sock_t *_socket, struct sockaddr *_address, socklen_t _address_len);
int microtcp_shutdown_active_fsm(microtcp_sock_t *_socket, struct sockaddr *_address, socklen_t _address_len);
int microtcp_shutdown_passive_fsm(microtcp_sock_t *_socket, struct sockaddr *_address, socklen_t _address_len);
// int receive_fsm(microtcp_sock_t *const _socket, struct sockaddr *_address, socklen_t _address_len);
int microtcp_send_fsm(microtcp_sock_t *_socket, const void * _buffer, size_t _length);

ssize_t till_timeout(microtcp_sock_t *_socket, void *_buffer, size_t _length);
#endif /* STATE_MACHINES_H */