#include <errno.h>
#include "logging/logger.h"
#include "microtcp.h"
#include "microtcp_core.h"
#include "microtcp_defines.h"
#include "microtcp_core_macros.h"
#include "microtcp_common_macros.h"
#include "state_machines/state_machines.h"

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

        if (set_socket_timeout(&new_socket, 0, MICROTCP_ACK_TIMEOUT_US) == POSIX_SETSOCKOPT_FAILURE)
                LOG_ERROR_RETURN(new_socket, "Failed to set timeout on socket descriptor.");

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
        LOG_INFO_RETURN(bind_result, "Bind operation succeeded.");
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

        /* Initialize resources for the socket. */
        generate_initial_sequence_number(_socket);
        if (allocate_receive_buffer(_socket) == NULL)
                LOG_ERROR_RETURN(MICROTCP_CONNECT_FAILURE, "Memory allocation for receive buffer failed.");

        /* Run the `connect` state machine. */
        int connect_state_machine_result = microtcp_connect_state_machine(_socket, _address, _address_len);

        /* Clean-up on failure. */
        if (connect_state_machine_result == MICROTCP_CONNECT_FAILURE)
                deallocate_receive_buffer(_socket);

        LOG_INFO_RETURN(connect_state_machine_result, "Connect operation succeeded.");
}

int microtcp_accept(microtcp_sock_t *_socket, struct sockaddr *_address,
                    socklen_t _address_len)
{
        LOG_INFO("Accept operation initiated.");
        /* Validate input parameters. */
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(MICROTCP_CONNECT_FAILURE, _socket, LISTEN);
        RETURN_ERROR_IF_SOCKADDR_INVALID(MICROTCP_CONNECT_FAILURE, _address);
        RETURN_ERROR_IF_SOCKET_ADDRESS_LENGTH_INVALID(MICROTCP_CONNECT_FAILURE, _address_len, sizeof(*_address));

        /* Initialize resources for the socket. */
        generate_initial_sequence_number(_socket);
        if (allocate_receive_buffer(_socket) == NULL)
                LOG_ERROR_RETURN(MICROTCP_CONNECT_FAILURE, "Failed to allocate recvbuf memory.");

        /* Run the `accept` state machine. */
        int accept_state_machine_result = microtcp_accept_state_machine(_socket, _address, _address_len);

        /* Clean-up on failure. */
        if (accept_state_machine_result == MICROTCP_ACCEPT_FAILURE)
                deallocate_receive_buffer(_socket);

        LOG_INFO_RETURN(accept_state_machine_result, "Accept operation succeeded.");
}

int microtcp_shutdown(microtcp_sock_t *_socket, int _how)
{
        LOG_INFO("Shutdown operation initiated.");
        /* Validate input parameters. */
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(MICROTCP_CONNECT_FAILURE, _socket, ESTABLISHED);
}

ssize_t
microtcp_send(microtcp_sock_t *socket, const void *buffer, size_t length,
              int flags)
{
        /* Your code here */
}

ssize_t
microtcp_recv(microtcp_sock_t *socket, void *buffer, size_t length, int flags)
{
        /* Your code here */
}
