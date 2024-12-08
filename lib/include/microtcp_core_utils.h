#ifndef MICROTCP_CORE_UTILS_H
#define MICROTCP_CORE_UTILS_H

#include <stdint.h>
#include <sys/socket.h>

#include "microtcp_core_macros.h"

/* Internal MACRO defines. */
#define NO_SENDTO_FLAGS 0
#define NO_RECVFROM_FLAGS 0

/* Error values returned internally to microtcp_connect(), if any of its stages fail. */
#define MICROTCP_SEND_SYN_ERROR -1
#define MICROTCP_SEND_SYN_FATAL_ERROR -2

#define MICROTCP_RECV_SYN_ACK_TIMEOUT 0
#define MICROTCP_RECV_SYN_ACK_ERROR -1
#define MICROTCP_RECV_SYN_ACK_FATAL_ERROR -2

#define MICROTCP_SEND_ACK_ERROR -1
#define MICROTCP_SEND_ACK_FATAL_ERROR -2

/* Error values returned internally to microtcp_accept(), if any of its stages fail. */
#define MICROTCP_RECV_SYN_TIMEOUT 0
#define MICROTCP_RECV_SYN_ERROR -1
#define MICROTCP_RECV_SYN_FATAL_ERROR -2

typedef struct
{
        uint8_t *raw_bytes;
        uint16_t size;
} microtcp_payload_t;

typedef struct
{
        microtcp_header_t header;
        uint8_t *raw_payload_bytes;
} microtcp_segment_t;

void generate_initial_sequence_number(microtcp_sock_t *_socket);
microtcp_segment_t *create_microtcp_segment(microtcp_sock_t *_socket, uint16_t _control, microtcp_payload_t _payload);
void *serialize_microtcp_segment(microtcp_segment_t *_segment);
_Bool is_valid_microtcp_bytestream(void *_bytestream_buffer, size_t _bytestream_buffer_length);
microtcp_segment_t *extract_microtcp_segment(void *_bytestream_buffer, size_t _bytestream_buffer_length);
/**
 * @returns pointer to the newly allocated recvbuf. If allocation fails returns NULL;
 * @brief There are two states where recvbuf memory allocation is possible.
 * Client allocates its recvbuf in connect(), socket in CLOSED state.
 * Server allocates its recvbuf in accept(),  socket in LISTEN  state.
 * So we use ANY for state parameter on the following socket check.
 */
void *allocate_receive_buffer(microtcp_sock_t *_socket);
/**
 * @returns the number of bytes, validly send into the socket.
 * This implies that a packet was validly send into the socket.
 */

/**
 * @brief Frees and nullifies the receive buffer of a microTCP socket.
 *
 * @param _socket A pointer to the `microtcp_sock_t` structure.
 */
void *deallocate_receive_buffer(microtcp_sock_t *_socket);

ssize_t send_syn_segment(microtcp_sock_t *const _socket, const struct sockaddr *const _address, const socklen_t _address_len);
/**
 * @returns the number of bytes, it validly received.
 * This also implies that a packet was correctly received.
 */
ssize_t receive_synack_segment(microtcp_sock_t *const _socket, struct sockaddr *const _address, socklen_t _address_len);
/**
 * @returns the number of bytes, validly send into the socket.
 * This implies that a packet was validly send into the socket.
 */
ssize_t send_ack_segment(microtcp_sock_t *const _socket, const struct sockaddr *const _address, const socklen_t _address_len);
void update_socket_sent_counters(microtcp_sock_t *_socket, size_t _bytes_sent);
void update_socket_received_counters(microtcp_sock_t *_socket, size_t _bytes_received);
void update_socket_lost_counters(microtcp_sock_t *_socket, size_t _bytes_lost);

#endif /* MICROTCP_CORE_UTILS_H */