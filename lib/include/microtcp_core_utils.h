#ifndef MICROTCP_CORE_UTILS_H
#define MICROTCP_CORE_UTILS_H

#include <stdint.h>
#include <sys/socket.h>

#include "microtcp_core_macros.h"

/* Internal MACRO defines. */
#define ACK_BIT (1 << 12)
#define RST_BIT (1 << 13)
#define SYN_BIT (1 << 14)
#define FIN_BIT (1 << 15)

#define NO_SENDTO_FLAGS 0
#define NO_RECVFROM_FLAGS 0

/* Error values returned internally to microtcp_connect(), if any of its stages fail. */
#define MICROTCP_SEND_SYN_ERROR -1
#define MICROTCP_SEND_SYN_FATAL_ERROR -2

#define MICROTCP_RECV_SYN_ACK_TIMEOUT 0
#define MICROTCP_RECV_SYN_ACK_ERROR -1
#define MICROTCP_RECV_SYN_ACK_FATAL_ERROR -2

#define MICROTCP_SEND_ACK_ERROR -1
#define MICROTCP_SEND_ACK_FATAL_ERROR -2


typedef struct
{
        uint8_t *raw_bytes;
        uint16_t size;
} microtcp_payload_t;

typedef struct
{
        microtcp_header_t header;
        uint8_t *raw_payload_bytes;
} microtcp_segment_t;

void generate_initial_sequence_number(microtcp_sock_t *_socket);
microtcp_segment_t *create_microtcp_segment(microtcp_sock_t *_socket, uint16_t _control, microtcp_payload_t _payload);
void * serialize_microtcp_segment(microtcp_segment_t *segment);
ssize_t send_syn_segment(microtcp_sock_t *_socket, const struct sockaddr *_address, socklen_t _address_len);
ssize_t receive_synack_segment(microtcp_sock_t *_socket, struct sockaddr *_address, socklen_t _address_len);
/**
 * @returns pointer to the beginning of newly allocated buffer, is allocation succeeds, NULL if not.
 */
void *allocate_receive_buffer(microtcp_sock_t *_socket);

void update_socket_sent_counters(microtcp_sock_t *_socket, size_t _bytes_sent);
void update_socket_received_counters(microtcp_sock_t *_socket, size_t _bytes_received);
void update_socket_lost_counters(microtcp_sock_t *_socket, size_t _bytes_lost);


#endif /* MICROTCP_CORE_UTILS_H */