#ifndef CORE_CONTROL_SEGMENTS_IO_H
#define CORE_CONTROL_SEGMENTS_IO_H

#include <sys/socket.h>
#include <sys/types.h>
#include "microtcp.h"

#define SEND_SEGMENT_ERROR -1
#define SEND_SEGMENT_FATAL_ERROR -2

#define RECV_SEGMENT_TIMEOUT 0
#define RECV_SEGMENT_ERROR -1
#define RECV_SEGMENT_FATAL_ERROR -2
#define RECV_SEGMENT_RST_BIT -3
#define RECV_SEGMENT_NOT_SYN_BIT -4
#define RECV_SEGMENT_UNEXPECTED_FINACK -5

/* CONTROL */
ssize_t send_syn_control_segment(microtcp_sock_t *const _socket, const struct sockaddr *const _address, const socklen_t _address_len);
ssize_t send_synack_control_segment(microtcp_sock_t *const _socket, const struct sockaddr *const _address, const socklen_t _address_len);
ssize_t send_ack_control_segment(microtcp_sock_t *const _socket, const struct sockaddr *const _address, const socklen_t _address_len);
ssize_t send_finack_control_segment(microtcp_sock_t *const _socket, const struct sockaddr *const _address, const socklen_t _address_len);
ssize_t send_rstack_control_segment(microtcp_sock_t *const _socket, const struct sockaddr *const _address, const socklen_t _address_len);

ssize_t receive_syn_control_segment(microtcp_sock_t *const _socket, struct sockaddr *const _address, const socklen_t _address_len);
ssize_t receive_synack_control_segment(microtcp_sock_t *const _socket, struct sockaddr *const _address, const socklen_t _address_len);
ssize_t receive_ack_control_segment(microtcp_sock_t *const _socket, struct sockaddr *const _address, const socklen_t _address_len);
ssize_t receive_finack_control_segment(microtcp_sock_t *const _socket, struct sockaddr *const _address, const socklen_t _address_len);

ssize_t receive_ack_control_segment_async(microtcp_sock_t *const _socket, _Bool _block);


/* DATA */
size_t send_data_segment(microtcp_sock_t *_socket, const void *_buffer, size_t _segment_size);

#endif /* CORE_CONTROL_SEGMENTS_IO_H */