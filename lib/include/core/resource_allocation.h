#ifndef CORE_RESOURCE_ALLOCATION_H
#define CORE_RESOURCE_ALLOCATION_H

#include "microtcp_defines.h"
#include "microtcp.h"

void release_and_reset_connection_resources(microtcp_sock_t *_socket, microtcp_state_t _rollback_state);

/**
 * @brief Allocates buffers required for MicroTCP's handshake.
 *
 * Allocates `bytestream_build_buffer` and `bytestream_receive_buffer`.
 * Cleans up on failure to prevent memory leaks.
 *
 * @param _socket Pointer to the `microtcp_sock_t` socket.
 * @returns 1 on success, 0 on failure.
 */
status_t allocate_pre_handshake_buffers(microtcp_sock_t *_socket);
void deallocate_pre_handshake_buffers(microtcp_sock_t *_socket);

status_t allocate_post_handshake_buffers(microtcp_sock_t *_socket);
status_t deallocate_post_handshake_buffers(microtcp_sock_t *_socket);

#endif /* CORE_RESOURCE_ALLOCATION_H */
