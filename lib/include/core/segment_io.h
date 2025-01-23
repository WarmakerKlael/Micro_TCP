#ifndef CORE_CONTROL_SEGMENTS_IO_H
#define CORE_CONTROL_SEGMENTS_IO_H

#include <sys/socket.h>
#include <sys/types.h>
#include "microtcp.h"

#define SEND_SEGMENT_ERROR (-1)
#define SEND_SEGMENT_FATAL_ERROR (-2)

#define RECV_SEGMENT_EXCEPTION_THRESHOLD 0
#define RECV_SEGMENT_TIMEOUT 0
#define RECV_SEGMENT_ERROR (-1)
#define RECV_SEGMENT_FATAL_ERROR (-2)
#define RECV_SEGMENT_RST_RECEIVED (-3)
#define RECV_SEGMENT_SYN_EXPECTED (-4)
#define RECV_SEGMENT_FINACK_UNEXPECTED (-5)
#define RECV_SEGMENT_CARRIES_DATA (-6)

/* HANDSHAKE CONTROL */
ssize_t send_syn_control_segment(microtcp_sock_t *_socket, const struct sockaddr *_address, socklen_t _address_len);
ssize_t send_synack_control_segment(microtcp_sock_t *_socket, const struct sockaddr *_address, socklen_t _address_len);
ssize_t send_ack_control_segment(microtcp_sock_t *_socket, const struct sockaddr *_address, socklen_t _address_len);
ssize_t send_finack_control_segment(microtcp_sock_t *_socket, const struct sockaddr *_address, socklen_t _address_len);
ssize_t send_rstack_control_segment(microtcp_sock_t *_socket, const struct sockaddr *_address, socklen_t _address_len);

ssize_t receive_syn_control_segment(microtcp_sock_t *_socket, struct sockaddr *_address, socklen_t _address_len);
ssize_t receive_synack_control_segment(microtcp_sock_t *_socket, struct sockaddr *_address, socklen_t _address_len, uint32_t _required_ack_number);
ssize_t receive_ack_control_segment(microtcp_sock_t *_socket, struct sockaddr *_address, socklen_t _address_len, uint32_t _required_ack_number);
ssize_t receive_finack_control_segment(microtcp_sock_t *_socket, struct sockaddr *_address, socklen_t _address_len, uint32_t _required_ack_number);

/* DATA */
size_t send_data_segment(microtcp_sock_t *_socket, const void *_buffer, size_t _segment_size, uint32_t _seq_number);
ssize_t receive_data_segment(microtcp_sock_t *_socket, _Bool _block);
ssize_t receive_data_ack_segment(microtcp_sock_t *_socket, _Bool _block);

#endif /* CORE_CONTROL_SEGMENTS_IO_H */
