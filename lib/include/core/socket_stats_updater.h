#ifndef CORE_SOCKET_STATS_UPDATER_H
#define CORE_SOCKET_STATS_UPDATER_H

#include <stddef.h>
#include "microtcp.h"

void update_socket_sent_counters(microtcp_sock_t *_socket, size_t _bytes_sent);
void update_socket_received_counters(microtcp_sock_t *_socket, size_t _bytes_received);
void update_socket_lost_counters(microtcp_sock_t *_socket, size_t _bytes_lost);

#endif /* CORE_SOCKET_STATS_UPDATER_H */
