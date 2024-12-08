#include <errno.h>

#include "microtcp.h"
#include "crc32.h"
#include "allocator/allocator.h"
#include "logging/logger.h"
#include "microtcp_helper_functions.h"
#include "microtcp_core_macros.h"
#include "microtcp_core_utils.h"
#include "microtcp_defines.h"
#include "microtcp_common_macros.h"
#include "microtcp_helper_functions.h"
#include "state_machines/state_machines.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * We won't allocate memory to 'recvbuf' in microtcp_socket(), as newer TCP          *
 * implementations allocate TCP's receive buffer after 3-way handshake in accept().  *
 * This is done to limit resource depletion in case of SYN-flood attack.             *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
microtcp_sock_t microtcp_socket(int _domain, int _type, int _protocol)
{
        microtcp_sock_t new_socket = {
            .sd = POSIX_SOCKET_FAILURE_VALUE,   /* We assume socket descriptor contains FAILURE value; Should change from POSIX's socket() */
            .state = INVALID,                   /* Socket state is INVALID until we get a POSIX's socket descriptor. */
            .init_win_size = MICROTCP_WIN_SIZE, /* Our initial window size. */
            .curr_win_size = MICROTCP_WIN_SIZE, /* Our window size. */
            .peer_win_size = 0,                 /* We assume window side of other side to be zero, we wait for other side to advertise it window size in 3-way handshake. */
            .recvbuf = NULL,                    /* Buffer gets allocated in 3-way handshake. */
            .buf_fill_level = 0,
            .cwnd = MICROTCP_INIT_CWND,
            .ssthresh = MICROTCP_INIT_SSTHRESH,
            .seq_number = 0, /* Default value, waiting 3 way. */
            .ack_number = 0, /* Default value */
            .packets_sent = 0,
            .packets_received = 0,
            .packets_lost = 0,
            .bytes_sent = 0,
            .bytes_received = 0,
            .bytes_lost = 0};

        if (!FUNCTION_MICROTCP_SOCKET_PARAMETER_IS_VALID(_domain, AF_INET) ||
            !FUNCTION_MICROTCP_SOCKET_PARAMETER_IS_VALID(_type, SOCK_DGRAM) ||
            !FUNCTION_MICROTCP_SOCKET_PARAMETER_IS_VALID(_protocol, IPPROTO_UDP))
                LOG_ERROR_RETURN(new_socket, "Failing to proceed due to bad input parameters");

        if ((new_socket.sd = socket(_domain, _type, _protocol)) == POSIX_SOCKET_FAILURE_VALUE)
                LOG_ERROR_RETURN(new_socket, "Failed to create underlying UDP socket. (%s = %d | %s = %d | %s = %d) --> ERRNO(%d): %s ",
                                 STRINGIFY(_domain), _domain,
                                 STRINGIFY(_type), _type,
                                 STRINGIFY(_protocol), _protocol,
                                 errno, strerror(errno));

        new_socket.state = CLOSED; /* Socket successfully transitions to CLOSED state (from INVALID). */
        LOG_INFO("Socket successfully created. (sd = %d | state = %s)", new_socket.sd, get_microtcp_state_to_string(new_socket.state));
        return new_socket;
}

int microtcp_bind(microtcp_sock_t *_socket, const struct sockaddr *_address, socklen_t _address_len)
{
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(MICROTCP_BIND_FAILURE, _socket, CLOSED);
        RETURN_ERROR_IF_SOCKADDR_INVALID(MICROTCP_BIND_FAILURE, _address);
        RETURN_ERROR_IF_SOCKET_ADDRESS_LENGTH_INVALID(MICROTCP_BIND_FAILURE, _address_len, sizeof(struct sockaddr));

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
        LOG_INFO("Connect operation begun.");
        /* Validate input parameters. */
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(MICROTCP_CONNECT_FAILURE, _socket, CLOSED);
        RETURN_ERROR_IF_SOCKADDR_INVALID(MICROTCP_CONNECT_FAILURE, _address);
        RETURN_ERROR_IF_SOCKET_ADDRESS_LENGTH_INVALID(MICROTCP_CONNECT_FAILURE, _address_len, sizeof(struct sockaddr));

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
        LOG_INFO("Accept operation begun.");
        /* Validate input parameters. */
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(MICROTCP_CONNECT_FAILURE, _socket, LISTEN);
        RETURN_ERROR_IF_SOCKADDR_INVALID(MICROTCP_CONNECT_FAILURE, _address);
        RETURN_ERROR_IF_SOCKET_ADDRESS_LENGTH_INVALID(MICROTCP_CONNECT_FAILURE, _address_len, sizeof(struct sockaddr));

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
