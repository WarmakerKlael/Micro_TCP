#include "core/misc.h"
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include "core/resource_allocation.h"
#include "core/send_queue.h"
#include "logging/microtcp_logger.h"
#include "microtcp.h"
#include "microtcp_core_macros.h"
#include "microtcp_defines.h"
#include "microtcp_helper_functions.h"
#include "microtcp_helper_macros.h"
#include "smart_assert.h"
#include "status.h"
#include <stdbool.h>
#include "status.h"

__attribute__((constructor(RNG_CONSTRUCTOR_PRIORITY))) static void seed_random_number_generator(void)
{
#define CLOCK_GETTIME_FAILURE -1 /* Specified in man pages. */
        struct timespec ts;
        if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts) == CLOCK_GETTIME_FAILURE)
        {
                LOG_ERROR("RNG seeding failed; %s() -> ERRNO %d: %s", STRINGIFY(clock_gettime), errno, strerror(errno));
                exit(EXIT_FAILURE);
        }
        unsigned int seed = ts.tv_sec ^ ts.tv_nsec;
        srand(seed);
        LOG_INFO("RNG seeding successed; Seed = %u", seed);
}

int set_socket_recvfrom_timeout(microtcp_sock_t *_socket, struct timeval _tv)
{
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(-1, _socket, ~0);
        SMART_ASSERT(_tv.tv_sec >= 0, _tv.tv_usec >= 0);
        normalize_timeval(&_tv);
        return setsockopt(_socket->sd, SOL_SOCKET, SO_RCVTIMEO, &_tv, sizeof(_tv));
}

int set_socket_recvfrom_to_block(microtcp_sock_t *_socket)
{
        return set_socket_recvfrom_timeout(_socket, (struct timeval){0, 0});
}

struct timeval get_socket_recvfrom_timeout(microtcp_sock_t *_socket)
{
        SMART_ASSERT(_socket != NULL);

        struct timeval timeout = {0};
        socklen_t timout_len = sizeof(timeout); /* Ignored as we used getsockopt for just getting recvfrom timeout value. */
        int return_value = getsockopt(_socket->sd, SOL_SOCKET, SO_RCVTIMEO, &timeout, &timout_len);
        if (return_value == -1) /* Failed read. */
                LOG_ERROR("Getting socket's timeout value set on recvfrom failed. ERRNO(%d) = %s.", errno, strerror(errno));
        return timeout;
}

void generate_initial_sequence_number(microtcp_sock_t *_socket)
{
        SMART_ASSERT(_socket != NULL);
        uint32_t high = rand() & 0xFFFF;   /* Get only the 16 lower bits. */
        uint32_t low = rand() & 0xFFFF;    /* Get only the 16 lower bits. */
        uint32_t isn = (high << 16) | low; /* Combine 16-bit random of low and 16-bit random of high, to create a 32-bit random ISN. */
        _socket->seq_number = 0;

        LOG_INFO("ISN generated. %u", _socket->seq_number);
}

microtcp_sock_t initialize_microtcp_socket(void)
{
        microtcp_sock_t new_socket = {
            .sd = POSIX_SOCKET_FAILURE_VALUE,   /* We assume socket descriptor contains FAILURE value; Should change from POSIX's socket() */
            .state = INVALID,                   /* Socket state is INVALID until we get a POSIX's socket descriptor. */
            .init_win_size = MICROTCP_WIN_SIZE, /* Our initial window size. */
            .curr_win_size = MICROTCP_WIN_SIZE, /* Our window size. */
            .peer_win_size = 0,                 /* We assume window side of other side to be zero, we wait for other side to advertise it window size in 3-way handshake. */
            .bytestream_rrb = NULL,             /* Receive-Ring-Buffer gets allocated in 3-way handshake. */
            .cwnd = MICROTCP_INIT_CWND,
            .ssthresh = MICROTCP_INIT_SSTHRESH,
            .seq_number = 0, /* Default value, waiting 3 way. */
            .ack_number = 0, /* Default value */
            .packets_sent = 0,
            .packets_received = 0,
            .packets_lost = 0,
            .bytes_sent = 0,
            .bytes_received = 0,
            .bytes_lost = 0,
            .segment_build_buffer = NULL,
            .bytestream_build_buffer = NULL,
            .send_queue = NULL,
            .bytestream_receive_buffer = NULL,
            .peer_address = NULL};
        return new_socket;
}

void cleanup_microtcp_socket(microtcp_sock_t *_socket)
{
        SMART_ASSERT(_socket != NULL);
        _Bool graceful_operation = true;
        deallocate_pre_handshake_buffers(_socket);
        if (deallocate_post_handshake_buffers(_socket) == FAILURE)
        {
                LOG_ERROR("Cleanup of microtcp partial failure: couldn't deallocate post handshake buffers.");
                graceful_operation = false;
        }

        if (_socket->sd != POSIX_SOCKET_FAILURE_VALUE)
                close(_socket->sd);
        _socket->sd = POSIX_SOCKET_FAILURE_VALUE;
        _socket->state = INVALID;
        _socket->peer_address = NULL;
        if (graceful_operation)
                LOG_INFO("MicroTCP socket, successfully cleaned its resources.");
}
