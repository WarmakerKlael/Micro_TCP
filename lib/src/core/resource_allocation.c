#include "core/resource_allocation.h"
#include <stddef.h>
#include <sys/socket.h>
#include "allocator/allocator_macros.h"
#include "core/control_segments_io.h"
#include "core/misc.h"
#include "core/segment_processing.h"
#include "logging/microtcp_logger.h"
#include "microtcp_core_macros.h"
#include "settings/microtcp_settings.h"
#include "smart_assert.h"

/* Declarations of static functions implemented in this file: */
static microtcp_segment_t *allocate_segment_build_buffer(microtcp_sock_t *_socket);
static void *allocate_bytestream_build_buffer(microtcp_sock_t *_socket);
static microtcp_segment_t *allocate_segment_extraction_buffer(microtcp_sock_t *_socket);
static void *allocate_bytestream_receive_buffer(microtcp_sock_t *_socket);
static void deallocate_segment_build_buffer(microtcp_sock_t *_socket);
static void deallocate_bytestream_build_buffer(microtcp_sock_t *_socket);
static void deallocate_segment_extraction_buffer(microtcp_sock_t *_socket);
static void deallocate_bytestream_receive_buffer(microtcp_sock_t *_socket);

/**
 * @returns pointer to the newly allocated `bytestream_assembly_buffer`. If allocation fails returns `NULL`;
 * @brief There are two states where bytestream_assembly_buffer memory allocation is possible.
 * Client allocates its bytestream_assembly_buffer in connect(), socket in CLOSED state.
 * Server allocates its bytestream_assembly_buffer in accept(),  socket in LISTEN  state.
 */
status_t allocate_bytestream_assembly_buffer(microtcp_sock_t *_socket)
{
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(NULL, _socket, ESTABLISHED);

        /* Tried to reallocate buffer before free()ing and NULLifying. */
        SMART_ASSERT(_socket->bytestream_assembly_buffer == NULL);
        /* `buf_fill_level` must be 0  before allocating buffer. */
        SMART_ASSERT(_socket->buf_fill_level == 0);

        _socket->bytestream_assembly_buffer = CALLOC_LOG(_socket->bytestream_assembly_buffer, get_bytestream_assembly_buffer_len());
        if (_socket->bytestream_assembly_buffer == NULL)
                LOG_ERROR_RETURN(FAILURE, "Failed to allocate socket's `bytestream_assembly_buffer`.");
        LOG_INFO_RETURN(SUCCESS, "Succesful allocation of `bytestream_assembly_buffer`.");
}

void deallocate_bytestream_assembly_buffer(microtcp_sock_t *_socket)
{
        SMART_ASSERT(_socket != NULL);
        FREE_NULLIFY_LOG(_socket->bytestream_assembly_buffer);
}

status_t allocate_handshake_required_buffers(microtcp_sock_t *_socket)
{
        SMART_ASSERT(_socket != NULL);
        SMART_ASSERT(_socket->state != ESTABLISHED);

        /* Buffers meant for making ack sending packets. */
        if (allocate_segment_build_buffer(_socket) == NULL)
                goto failure_cleanup;
        if (allocate_bytestream_build_buffer(_socket) == NULL)
                goto failure_cleanup;

        /* Buffers meant for receiving and extracting segments. */
        if (allocate_bytestream_receive_buffer(_socket) == NULL)
                goto failure_cleanup;
        if (allocate_segment_extraction_buffer(_socket) == NULL)
                goto failure_cleanup;

        return SUCCESS;
failure_cleanup:
        deallocate_handshake_required_buffers(_socket);
        return FAILURE;
}

void deallocate_handshake_required_buffers(microtcp_sock_t *_socket)
{
        SMART_ASSERT(_socket != NULL);
        SMART_ASSERT(_socket->state != ESTABLISHED);
        deallocate_segment_build_buffer(_socket);
        deallocate_bytestream_build_buffer(_socket);
        deallocate_bytestream_receive_buffer(_socket);
        deallocate_segment_extraction_buffer(_socket);
}

void release_and_reset_handshake_resources(microtcp_sock_t *_socket, mircotcp_state_t _rollback_state)
{
        SMART_ASSERT(_socket != NULL, _rollback_state != ESTABLISHED);

        /* Reset connection if established (its not this function's job to terminate gracefully).
         * We dont check if RST_BIT is actually received; Its a best effort to warn client,
         * that something went wrong with the connection, and we have to terminate immediately. */
        if (_socket->state == ESTABLISHED)
                send_rstack_control_segment(_socket, _socket->peer_socket_address, sizeof(*(_socket->peer_socket_address)));

        _socket->state = _rollback_state;
        _socket->peer_socket_address = NULL;
        deallocate_bytestream_assembly_buffer(_socket);
        deallocate_handshake_required_buffers(_socket);
        if (set_socket_recvfrom_timeout(_socket, get_microtcp_ack_timeout()) == POSIX_SETSOCKOPT_FAILURE)
                LOG_ERROR("Failed resetting socket's timeout period.");
        LOG_INFO("Socket's handshake resources, released.");
}

static microtcp_segment_t *allocate_segment_build_buffer(microtcp_sock_t *_socket)
{
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(NULL, _socket, (CLOSED | LISTEN));
        SMART_ASSERT(_socket->segment_build_buffer == NULL);

        _socket->segment_build_buffer = CALLOC_LOG(_socket->segment_build_buffer, sizeof(microtcp_segment_t));
        if (_socket->segment_build_buffer == NULL)
                LOG_ERROR_RETURN(_socket->segment_build_buffer, "Failed to allocate socket's `segment_build_buffer`.");
        LOG_INFO_RETURN(_socket->segment_build_buffer, "Succesful allocation of `segment_build_buffer`.");
}

/**
 * @returns pointer to the newly allocated `bytestream_build_buffer`. If allocation fails returns `NULL`;
 * @brief There are two states where `bytestream_build_buffer` memory allocation is possible.
 * Client allocates its `bytestream_build_buffer` in connect(), socket in CLOSED state.
 * Server allocates its `bytestream_build_buffer` in accept(),  socket in LISTEN  state.
 */
static void *allocate_bytestream_build_buffer(microtcp_sock_t *_socket)
{
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(NULL, _socket, (CLOSED | LISTEN));
        SMART_ASSERT(_socket->bytestream_build_buffer == NULL);

        _socket->bytestream_build_buffer = CALLOC_LOG(_socket->bytestream_build_buffer, MICROTCP_MSS);
        if (_socket->bytestream_build_buffer == NULL)
                LOG_ERROR_RETURN(_socket->bytestream_build_buffer, "Failed to allocate socket's `bytestream_build_buffer`.");
        LOG_INFO_RETURN(_socket->bytestream_build_buffer, "Succesful allocation of `bytestream_build_buffer`.");
}

/**
 * @returns pointer to the newly allocated `bytestream_receive_buffer`. If allocation fails returns `NULL`;
 * @brief There are two states where `bytestream_receive_buffer` memory allocation is possible.
 * Client allocates its `bytestream_receive_buffer` in connect(), socket in CLOSED state.
 * Server allocates its `bytestream_receive_buffer` in accept(),  socket in LISTEN  state.
 */
static void *allocate_bytestream_receive_buffer(microtcp_sock_t *_socket)
{
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(NULL, _socket, (CLOSED | LISTEN));
        SMART_ASSERT(_socket->bytestream_receive_buffer == NULL);

        _socket->bytestream_receive_buffer = CALLOC_LOG(_socket->bytestream_receive_buffer, MICROTCP_MSS);
        if (_socket->bytestream_receive_buffer == NULL)
                LOG_ERROR_RETURN(_socket->bytestream_receive_buffer, "Failed to allocate socket's `bytestream_receive_buffer`.");
        LOG_INFO_RETURN(_socket->bytestream_receive_buffer, "Succesful allocation of `bytestream_receive_buffer`.");
}

static microtcp_segment_t *allocate_segment_extraction_buffer(microtcp_sock_t *_socket)
{
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(NULL, _socket, (CLOSED | LISTEN));
        SMART_ASSERT(_socket->segment_receive_buffer == NULL);

        _socket->segment_receive_buffer = CALLOC_LOG(_socket->segment_receive_buffer, sizeof(microtcp_segment_t));
        if (_socket->segment_receive_buffer == NULL)
                LOG_ERROR_RETURN(_socket->segment_receive_buffer, "Failed to allocate socket's `segment_receive_buffer`.");
        LOG_INFO_RETURN(_socket->segment_receive_buffer, "Succesful allocation of `segment_receive_buffer`.");
}

static void deallocate_segment_build_buffer(microtcp_sock_t *_socket)
{
        SMART_ASSERT(_socket != NULL);
        SMART_ASSERT(_socket->state != ESTABLISHED);
        FREE_NULLIFY_LOG(_socket->segment_build_buffer);
}

static void deallocate_bytestream_build_buffer(microtcp_sock_t *_socket)
{
        SMART_ASSERT(_socket != NULL);
        SMART_ASSERT(_socket->state != ESTABLISHED);
        FREE_NULLIFY_LOG(_socket->bytestream_build_buffer);
}

static void deallocate_bytestream_receive_buffer(microtcp_sock_t *_socket)
{
        SMART_ASSERT(_socket != NULL);
        SMART_ASSERT(_socket->state != ESTABLISHED);
        FREE_NULLIFY_LOG(_socket->bytestream_receive_buffer);
}

static void deallocate_segment_extraction_buffer(microtcp_sock_t *_socket)
{
        SMART_ASSERT(_socket != NULL);
        SMART_ASSERT(_socket->state != ESTABLISHED);
        FREE_NULLIFY_LOG(_socket->segment_receive_buffer);
}