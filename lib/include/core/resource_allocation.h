#ifndef CORE_RESOURCE_ALLOCATION_H
#define CORE_RESOURCE_ALLOCATION_H

#include "microtcp_defines.h"
#include "microtcp.h"

void release_and_reset_handshake_resources(microtcp_sock_t *_socket, mircotcp_state_t _rollback_state);
void deallocate_handshake_required_buffers(microtcp_sock_t *_socket);

/**
 * @brief Allocates buffers required for MicroTCP's handshake.
 *
 * Allocates `bytestream_build_buffer` and `bytestream_receive_buffer`.
 * Cleans up on failure to prevent memory leaks.
 *
 * @param _socket Pointer to the `microtcp_sock_t` socket.
 * @returns 1 on success, 0 on failure.
 */
status_t allocate_handshake_required_buffers(microtcp_sock_t *_socket);
void deallocate_bytestream_assembly_buffer(microtcp_sock_t *_socket);

/**
 * @returns pointer to the newly allocated `bytestream_assembly_buffer`. If allocation fails returns `NULL`;
 * @brief There are two states where bytestream_assembly_buffer memory allocation is possible.
 * Client allocates its bytestream_assembly_buffer in connect(), socket in CLOSED state.
 * Server allocates its bytestream_assembly_buffer in accept(),  socket in LISTEN  state.
 */
status_t allocate_bytestream_assembly_buffer(microtcp_sock_t *_socket);


#endif /* CORE_RESOURCE_ALLOCATION_H */