#include "microtcp.h"
#include <errno.h>     // for errno
#include <stdio.h>     // for printf
#include <string.h>    // for strerror
#include "core/misc.h" // for generate_initial_sequence_nu...
#include "core/microtcp_recv_impl.h"
#include "core/resource_allocation.h"
#include "fsm/microtcp_fsm.h"           // for microtcp_accept_fsm, microtc...
#include "logging/microtcp_logger.h"    // for LOG_ERROR_RETURN, LOG_INFO_R...
#include "microtcp_core_macros.h"       // for RETURN_ERROR_IF_MICROTCP_SOC...
#include "microtcp_defines.h"           // for MICROTCP_CONNECT_FAILURE
#include "microtcp_helper_functions.h"  // for get_microtcp_state_to_string
#include "microtcp_helper_macros.h"     // for STRINGIFY
#include "settings/microtcp_settings.h" // for get_microtcp_ack_timeout
#include "smart_assert.h"               // for SMART_ASSERT

microtcp_sock_t microtcp_socket(int _domain, int _type, int _protocol)
{
        microtcp_sock_t new_socket = initialize_microtcp_socket();
        RETURN_ERROR_IF_FUNCTION_PARAMETER_MICROTCP_SOCKET_INVALID(new_socket, _domain, AF_INET);
        RETURN_ERROR_IF_FUNCTION_PARAMETER_MICROTCP_SOCKET_INVALID(new_socket, _type, SOCK_DGRAM);
        RETURN_ERROR_IF_FUNCTION_PARAMETER_MICROTCP_SOCKET_INVALID(new_socket, _protocol, IPPROTO_UDP);

        if ((new_socket.sd = socket(_domain, _type, _protocol)) == POSIX_SOCKET_FAILURE_VALUE)
                LOG_ERROR_RETURN(new_socket, "Failed to create underlying UDP socket. (%s = %d | %s = %d | %s = %d) --> ERRNO(%d): %s ",
                                 STRINGIFY(_domain), _domain, STRINGIFY(_type), _type,
                                 STRINGIFY(_protocol), _protocol, errno, strerror(errno));

        if (set_socket_recvfrom_timeout(&new_socket, get_microtcp_ack_timeout()) == POSIX_SETSOCKOPT_FAILURE)
        {
                microtcp_close(&new_socket);
                LOG_ERROR_RETURN(new_socket, "Failed to set timeout on socket descriptor.");
        }

        new_socket.state = CLOSED; /* Socket successfully transitions to CLOSED state (from INVALID). */
        LOG_INFO("Socket successfully created. (sd = %d | state = %s)", new_socket.sd, get_microtcp_state_to_string(new_socket.state));
        return new_socket;
}

int microtcp_bind(microtcp_sock_t *_socket, const struct sockaddr *_address, socklen_t _address_len)
{
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(MICROTCP_BIND_FAILURE, _socket, CLOSED);
        RETURN_ERROR_IF_SOCKADDR_INVALID(MICROTCP_BIND_FAILURE, _address);
        RETURN_ERROR_IF_SOCKET_ADDRESS_LENGTH_INVALID(MICROTCP_BIND_FAILURE, _address_len, sizeof(*_address));

        /* Bind socket address with POSIX socket descriptor. */
        int bind_result = bind(_socket->sd, _address, _address_len);
        if (bind_result == POSIX_BIND_FAILURE)
                LOG_ERROR_RETURN(MICROTCP_BIND_FAILURE, "Failed to bind socket descriptor to generic sockaddr.");
        else if (bind_result != POSIX_BIND_SUCCESS)
                LOG_ERROR_RETURN(MICROTCP_BIND_FAILURE, "Unknown error occurred on POSIX's bind()");

        _socket->state = LISTEN; /* Socket successfully binded to sockaddr */
        LOG_INFO_RETURN(bind_result, "Bind operation succeeded. (sd = %d)", _socket->sd);
}

/**
 * @brief From client's side, connection is considered established.
Server might not receive that last ACK, then server will resent its
SYN-ACK segments as from its side a timeout will occur. A timeout
might be avoided, if client starts sending packets rightaway into
the connection. The ACK numbers on those packets will show the server,
that client sent the last ACK on the 3-way handshake, but it was probably lost.
(That is why in normal TCP, last ACK of 3-way handshake can contain data).
 */
int microtcp_connect(microtcp_sock_t *_socket, const struct sockaddr *const _address, socklen_t _address_len)
{
        LOG_INFO("Connect operation initiated.");
        /* Validate input parameters. */
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(MICROTCP_CONNECT_FAILURE, _socket, CLOSED);
        RETURN_ERROR_IF_SOCKADDR_INVALID(MICROTCP_CONNECT_FAILURE, _address);
        RETURN_ERROR_IF_SOCKET_ADDRESS_LENGTH_INVALID(MICROTCP_CONNECT_FAILURE, _address_len, sizeof(*_address));

        /* Initialize socket handshake reuired resources for connection.*/
        generate_initial_sequence_number(_socket);
        if (allocate_pre_handshake_buffers(_socket) == FAILURE)
                goto connect_failure_cleanup;

        /* Run the `connect's` state machine. */
        if (microtcp_connect_fsm(_socket, _address, _address_len) == MICROTCP_CONNECT_FAILURE)
                goto connect_failure_cleanup;

        if (allocate_post_handshake_buffers(_socket) == FAILURE)
                goto connect_failure_cleanup;

        printf("Connect sent %zu bytes for succeful connection.\n", _socket->bytes_sent);
        LOG_INFO_RETURN(MICROTCP_CONNECT_SUCCESS, "Connect operation succeeded; Post handshake buffer allocate.");

connect_failure_cleanup:
        release_and_reset_connection_resources(_socket, CLOSED);
        LOG_ERROR_RETURN(MICROTCP_CONNECT_FAILURE, "Connect operation failed.");
}

int microtcp_accept(microtcp_sock_t *_socket, struct sockaddr *_address, socklen_t _address_len)
{
        LOG_INFO("Accept operation initiated.");
        /* Validate input parameters. */
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(MICROTCP_CONNECT_FAILURE, _socket, LISTEN);
        RETURN_ERROR_IF_SOCKADDR_INVALID(MICROTCP_CONNECT_FAILURE, _address);
        RETURN_ERROR_IF_SOCKET_ADDRESS_LENGTH_INVALID(MICROTCP_CONNECT_FAILURE, _address_len, sizeof(*_address));

        /* Initialize socket's resources required for 3-way handshake. */
        generate_initial_sequence_number(_socket);
        if (allocate_pre_handshake_buffers(_socket) == FAILURE)
                goto accept_failure_cleanup;

        /* Run the `accept's` state machine. */
        if (microtcp_accept_fsm(_socket, _address, _address_len) == MICROTCP_ACCEPT_FAILURE)
                goto accept_failure_cleanup;

        if (allocate_post_handshake_buffers(_socket) == FAILURE)
                goto accept_failure_cleanup;

        printf("Accept sent %zu bytes for succeful connection.\n", _socket->bytes_sent);
        LOG_INFO_RETURN(MICROTCP_ACCEPT_SUCCESS, "Accept operation succeeded; Post handshake buffer allocated.");

accept_failure_cleanup:
        release_and_reset_connection_resources(_socket, LISTEN);
        LOG_ERROR_RETURN(MICROTCP_ACCEPT_FAILURE, "Accept operation failed.");
}

int microtcp_shutdown(microtcp_sock_t *_socket, int _how)
{
        /* Validate input parameters. */
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(MICROTCP_CONNECT_FAILURE, _socket, ESTABLISHED | CLOSING_BY_PEER);

        struct sockaddr *address = _socket->peer_address;
        socklen_t address_len = sizeof(*(_socket->peer_address));

        switch (_socket->state)
        {
        case ESTABLISHED:
                if (microtcp_shutdown_active_fsm(_socket, address, address_len) == MICROTCP_SHUTDOWN_FAILURE)
                        LOG_ERROR_RETURN(MICROTCP_SHUTDOWN_FAILURE, "Shutdown operation failed.");
                LOG_INFO_RETURN(MICROTCP_SHUTDOWN_SUCCESS, "Shutdown operation succeeded.");
        case CLOSING_BY_PEER:
                if (microtcp_shutdown_passive_fsm(_socket, address, address_len) == MICROTCP_SHUTDOWN_FAILURE)
                        LOG_ERROR_RETURN(MICROTCP_SHUTDOWN_FAILURE, "Shutdown operation failed.");
                LOG_INFO_RETURN(MICROTCP_SHUTDOWN_SUCCESS, "Shutdown operation succeeded.");
        default:
                LOG_ERROR_RETURN(MICROTCP_SHUTDOWN_FAILURE, "Socket state = %s; Shutdown operation NOT allowed.",
                                 get_microtcp_state_to_string(_socket->state));
        }

        /* We dont clean socket... If shutdown fails... it's up to caller to use `microtcp_close_socket()` and recycle microtcp_sock_t. */
}

ssize_t microtcp_send(microtcp_sock_t *_socket, const void *_buffer, size_t _length, int _flags)
{
        SMART_ASSERT(_socket != NULL, _buffer != NULL, _length > 0);
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(MICROTCP_SEND_FAILURE, _socket, ESTABLISHED);

        return microtcp_send_fsm(_socket, _buffer, _length);
}

ssize_t microtcp_recv(microtcp_sock_t *_socket, void *_buffer, size_t _length, int _flags)
{
        _Static_assert(MICROTCP_RECV_TIMEOUT == 0, "DEFAULT value altered");
        _Static_assert(MICROTCP_RECV_FAILURE == -1, "DEFAULT value altered");
        SMART_ASSERT(_buffer != NULL, _length != 0);
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(MICROTCP_CONNECT_FAILURE, _socket, ESTABLISHED);
        if (!ARE_VALID_MICROTCP_RECV_FLAGS(_flags))
                return MICROTCP_RECV_FAILURE;

        return microtcp_recv_impl(_socket, _buffer, _length, _flags);
}

void microtcp_close(microtcp_sock_t *_socket)
{
        SMART_ASSERT(_socket != NULL);

        if (_socket->state == ESTABLISHED)
                microtcp_shutdown(_socket, SHUT_RDWR);
        cleanup_microtcp_socket(_socket);
        LOG_INFO("MicroTCP socket closed successfully.");
}
