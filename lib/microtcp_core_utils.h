#ifndef MICROTCP_CORE_UTILS_H
#define MICROTCP_CORE_UTILS_H

#include <stdint.h>
#include <sys/socket.h>

#include "microtcp_core_macros.h"

#define ACK_BIT (1 << 12)
#define RST_BIT (1 << 13)
#define SYN_BIT (1 << 14)
#define FIN_BIT (1 << 15)

#define NO_SENDTO_FLAGS 0

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

uint32_t generate_initial_sequence_number(void);
microtcp_segment_t *create_microtcp_segment(microtcp_sock_t *_socket, uint16_t _control, microtcp_payload_t _payload);
void * serialize_microtcp_segment(microtcp_segment_t *segment);
ssize_t send_syn_segment(microtcp_sock_t *_socket, const struct sockaddr *_address, socklen_t _address_len);


#endif /* MICROTCP_CORE_UTILS_H */