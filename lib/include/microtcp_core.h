#ifndef MICROTCP_CORE_H
#define MICROTCP_CORE_H

#include <stdint.h>
#include <sys/socket.h>

#include "microtcp_core_macros.h"

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

microtcp_sock_t initialize_microtcp_socket(void);
void generate_initial_sequence_number(microtcp_sock_t *_socket);
int set_recvfrom_timeout(microtcp_sock_t *_socket, time_t _sec, time_t _usec);
int get_recvfrom_timeout(microtcp_sock_t *_socket, time_t *_sec, time_t *_usec);
microtcp_segment_t *create_microtcp_segment(microtcp_sock_t *_socket, uint16_t _control, microtcp_payload_t _payload);
void *serialize_microtcp_segment(microtcp_segment_t *_segment);
void release_and_reset_handshake_resources(microtcp_sock_t *_socket, mircotcp_state_t _rollback_state);
_Bool is_valid_microtcp_bytestream(void *_bytestream_buffer, size_t _bytestream_buffer_length);
microtcp_segment_t *extract_microtcp_segment(void *_bytestream_buffer, size_t _bytestream_buffer_length);
/**
 * @returns pointer to the newly allocated recvbuf. If allocation fails returns NULL;
 * @brief There are two states where recvbuf memory allocation is possible.
 * Client allocates its recvbuf in connect(), socket in CLOSED state.
 * Server allocates its recvbuf in accept(),  socket in LISTEN  state.
 * So we use ANY for state parameter on the following socket check.
 */
status_t allocate_receive_buffer(microtcp_sock_t *_socket);
/**
 * @returns the number of bytes, validly send into the socket.
 * This implies that a packet was validly send into the socket.
 */

/**
 * @brief Frees and nullifies the receive buffer of a microTCP socket.
 *
 * @param _socket A pointer to the `microtcp_sock_t` structure.
 */
void deallocate_receive_buffer(microtcp_sock_t *_socket);

/**
 * @returns the number of bytes, it validly received.
 * This also implies that a packet was correctly received.
 */
void deallocate_handshake_required_buffers(microtcp_sock_t *_socket);

ssize_t send_syn_control_segment(microtcp_sock_t *const _socket, const struct sockaddr *const _address, const socklen_t _address_len);
ssize_t send_synack_control_segment(microtcp_sock_t *const _socket, const struct sockaddr *const _address, const socklen_t _address_len);
ssize_t send_ack_control_segment(microtcp_sock_t *const _socket, const struct sockaddr *const _address, const socklen_t _address_len);
ssize_t send_finack_control_segment(microtcp_sock_t *const _socket, const struct sockaddr *const _address, const socklen_t _address_len);
ssize_t send_rstack_control_segment(microtcp_sock_t *const _socket, const struct sockaddr *const _address, const socklen_t _address_len);

ssize_t receive_syn_control_segment(microtcp_sock_t *const _socket, struct sockaddr *const _address, const socklen_t _address_len);
ssize_t receive_synack_control_segment(microtcp_sock_t *const _socket, struct sockaddr *const _address, const socklen_t _address_len);
ssize_t receive_ack_control_segment(microtcp_sock_t *const _socket, struct sockaddr *const _address, const socklen_t _address_len);
ssize_t receive_finack_control_segment(microtcp_sock_t *const _socket, struct sockaddr *const _address, const socklen_t _address_len);
void update_socket_sent_counters(microtcp_sock_t *_socket, size_t _bytes_sent);
void update_socket_received_counters(microtcp_sock_t *_socket, size_t _bytes_received);
void update_socket_lost_counters(microtcp_sock_t *_socket, size_t _bytes_lost);


/**
 * @brief Allocates buffers required for MicroTCP's handshake.
 *
 * Allocates `bytestream_builder_buffer` and `bytestream_extraction_buffer`.
 * Cleans up on failure to prevent memory leaks.
 *
 * @param _socket Pointer to the `microtcp_sock_t` socket.
 * @returns 1 on success, 0 on failure.
 */
status_t allocate_handshake_required_buffers(microtcp_sock_t *_socket);

void cleanup_microtcp_socket(microtcp_sock_t *_socket);

#endif /* MICROTCP_CORE_UTILS_H */