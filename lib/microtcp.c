/*
 * microtcp, a lightweight implementation of TCP for teaching,
 * and academic purposes.
 *
 * Copyright (C) 2015-2017  Manolis Surligas <surligas@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "microtcp.h"
#include "crc32.h"
#include "allocator/allocator.h"
#include "logging/logger.h"
#include "microtcp_macro_functions.h"
#include "microtcp_defines.h"
#include "microtcp_helpers.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * We won't allocate memory to 'recvbuf' in microtcp_socket(), as newer TCP          *
 * implementations allocate TCP's receive buffer after 3-way handshake in accept().  *
 * This is done to limit resource depletion in case of SYN-flood attack.             *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
microtcp_sock_t microtcp_socket(int _domain, int _type, int _protocol)
{
        microtcp_sock_t new_socket = {
            .sd = UDP_SOCKET_FAILURE_VALUE,     /* We assume socket descriptor contains FAILURE value; Should change from UDP's socket() */
            .state = INVALID,                   /* Socket state is INVALID until we get a POSIX's socket descriptor. */
            .init_win_size = MICROTCP_WIN_SIZE, /* The window size, that will be advertised, to other side. */
            .curr_win_size = 0,                 /* We assume window side of other side to be zero, we wait for other side to advertise it window size in 3-way handshake. */
            .recvbuf = NULL,                    /* Buffer gets allocated in 3-way handshake. */
            .buf_fill_level = 0,
            .cwnd = MICROTCP_INIT_CWND,
            .ssthresh = MICROTCP_INIT_SSTHRESH,
            .seq_number = 0, /* Default value, waiting 3 way. */
            .ack_number = 0, /* Default value */
            .packets_send = 0,
            .packets_received = 0,
            .packets_lost = 0,
            .bytes_send = 0,
            .bytes_received = 0,
            .bytes_lost = 0};

        if (MICROTCP_SOCKET_PARAMETER_IS_VALID(_domain, AF_INET) &&
            MICROTCP_SOCKET_PARAMETER_IS_VALID(_type, SOCK_DGRAM) &&
            MICROTCP_SOCKET_PARAMETER_IS_VALID(_protocol, IPPROTO_UDP))
                PRINT_ERROR_RETURN(new_socket, "Failing to proceed due to bad input parameters");

        if ((new_socket.sd = socket(_domain, _type, _protocol)) == UDP_SOCKET_FAILURE_VALUE)
                PRINT_ERROR_RETURN(new_socket, "Failed to create underlying UDP socket. (%s = %d | %s = %d | %s = %d)",
                                   STRINGIFY(_domain), _domain,
                                   STRINGIFY(_type), _type,
                                   STRINGIFY(_protocol), _protocol);

        new_socket.state = CLOSED; /* Socket enters CLOSED state (from INVALID). */
        PRINT_INFO("Socket successfully created. (sd = %d | state = %s)", new_socket.sd, get_microtcp_state_to_string(new_socket.state));
        return new_socket;
}

int microtcp_bind(microtcp_sock_t *_socket, const struct sockaddr *_address, socklen_t _address_len)
{
        /* _socket checks: */
        if (_socket == NULL)
                PRINT_ERROR_RETURN(MICROTCP_BIND_FAILURE, "NULL pointer passed to variable '%s'.", STRINGIFY(_socket));
        if (_socket->state != CLOSED)
                PRINT_ERROR_RETURN(MICROTCP_BIND_FAILURE, "Expected socket state %s; (state = %s).", STRINGIFY(CLOSED), get_microtcp_state_to_string(_socket->state));
        if (_socket->sd < 0)
                PRINT_ERROR_RETURN(MICROTCP_BIND_FAILURE, "Invalid MicroTCP socket descriptor; (sd = %d).", _socket->sd);

        /* _address checks: */
        if (_address == NULL)
                PRINT_ERROR_RETURN(MICROTCP_BIND_FAILURE, "NULL pointer passed to variable '%s'.", STRINGIFY(_address));

        /* _address_len checks: */
        if (_address_len != sizeof(struct sockaddr))
                PRINT_WARNING("Address length mismatch: %s (got %d, expected %zu)", STRINGIFY(_address_len), _address_len, sizeof(struct sockaddr));

        int bind_result = bind(_socket->sd, _address, _address_len);
        if (bind_result == POSIX_BIND_FAILURE)
                PRINT_ERROR_RETURN(MICROTCP_BIND_FAILURE, "Failed to bind socket descriptor to generic sockaddr.");
        else if (bind_result != POSIX_BIND_SUCCESS)
                PRINT_ERROR_RETURN(MICROTCP_BIND_FAILURE, "Unknown error occurred on POSIX's bind()");
        _socket->state = BOUND;
        PRINT_INFO_RETURN(bind_result, "Bind operation succeeded.");
}

int microtcp_connect(microtcp_sock_t *socket, const struct sockaddr *address,
                     socklen_t address_len)
{
        /* Your code here */
}
/* Remember to allocate the receiver buffer (socket's recvbuf).*/
int microtcp_accept(microtcp_sock_t *socket, struct sockaddr *address,
                    socklen_t address_len)
{
        /* Your code here */
}

int microtcp_shutdown(microtcp_sock_t *socket, int how)
{
        /* Your code here */
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
