#ifndef CORE_MISC_H
#define CORE_MISC_H

#include <sys/time.h>
#include "microtcp.h"
struct timeval; 

microtcp_sock_t initialize_microtcp_socket(void);
void generate_initial_sequence_number(microtcp_sock_t *_socket);

int set_socket_recvfrom_timeout(microtcp_sock_t *_socket, struct timeval _tv);
struct timeval get_socket_recvfrom_timeout(microtcp_sock_t *_socket);

void cleanup_microtcp_socket(microtcp_sock_t *_socket);

#endif /* CORE_MISC_H */