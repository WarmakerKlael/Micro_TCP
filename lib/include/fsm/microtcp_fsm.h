#ifndef FSM_MICROTCP_FSM_H
#define FSM_MICROTCP_FSM_H

#include "microtcp.h"

int microtcp_connect_fsm(microtcp_sock_t *_socket, const struct sockaddr *_address, socklen_t _address_len);
int microtcp_accept_fsm(microtcp_sock_t *_socket, struct sockaddr *_address, socklen_t _address_len);
ssize_t microtcp_send_fsm(microtcp_sock_t *_socket, const void *_buffer, size_t _length);
int microtcp_shutdown_active_fsm(microtcp_sock_t *_socket, struct sockaddr *_address, socklen_t _address_len);
int microtcp_shutdown_passive_fsm(microtcp_sock_t *_socket, struct sockaddr *_address, socklen_t _address_len);

#endif /* FSM_MICROTCP_FSM_H */
