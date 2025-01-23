#include "core/socket_stats_updater.h"
#include <stddef.h>
#include "logging/microtcp_logger.h"
#include "microtcp.h"
#include "microtcp_helper_macros.h"
#include "smart_assert.h"

void update_socket_sent_counters(microtcp_sock_t *_socket, size_t _bytes_sent)
{
        SMART_ASSERT(_socket != NULL);
        if (_bytes_sent == 0)
        {
                LOG_ERROR("Failed to update socket's sent counters; %s = 0", STRINGIFY(_bytes_sent));
                return;
        }
        if (_bytes_sent < sizeof(microtcp_header_t))
                LOG_WARNING("Updating socket's sent counters, but %s = %d, which is less than valid transmission size.",
                            STRINGIFY(_bytes_sent), _bytes_sent);

        _socket->bytes_sent += _bytes_sent;
        _socket->packets_sent++;
        LOG_INFO("Socket's sent counters updated.");
}

void update_socket_received_counters(microtcp_sock_t *_socket, size_t _bytes_received)
{
        SMART_ASSERT(_socket != NULL);
        if (_bytes_received == 0)
        {
                LOG_ERROR("Failed to update socket's received counters; %s = 0", STRINGIFY(_bytes_received));
                return;
        }
        if (_bytes_received < sizeof(microtcp_header_t))
                LOG_WARNING("Updating socket's received counters, but %s = %d, which is less than valid transmission size.",
                            STRINGIFY(_bytes_received), _bytes_received);

        _socket->bytes_received += _bytes_received;
        _socket->packets_received++;
        LOG_INFO("Socket's received counters updated.");
}

void update_socket_lost_counters(microtcp_sock_t *_socket, size_t _bytes_lost)
{
        SMART_ASSERT(_socket != NULL);
        if (_bytes_lost == 0)
        {
                LOG_ERROR("Failed to update socket's lost counters; %s = 0", STRINGIFY(_bytes_lost));
                return;
        }
        if (_bytes_lost < sizeof(microtcp_header_t))
                LOG_WARNING("Updating socket's lost counters, but %s = %d, which is less than valid transmission size.",
                            STRINGIFY(_bytes_lost), _bytes_lost);

        _socket->bytes_lost += _bytes_lost;
        _socket->packets_lost++;
        LOG_INFO("Socket's lost counters updated.");
}
