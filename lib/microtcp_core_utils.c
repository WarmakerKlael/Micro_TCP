#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include "microtcp.h"
#include "microtcp_core_utils.h"
#include "logging/logger.h"
#include "microtcp_defines.h"
#include "microtcp_helper_macros.h"
#include "allocator/allocator.h"

/* Declarations of static functions implemented in this file: */
__attribute__((constructor(RNG_CONSTRUCTOR_PRIORITY))) static void seed_random_number_generator(void);

static void seed_random_number_generator(void)
{
#define CLOCK_GETTIME_FAILURE -1 /* Specified in man pages. */
        struct timespec ts;
        if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts) == CLOCK_GETTIME_FAILURE)
        {
                PRINT_ERROR("RNG seeding failed; %s() -> ERRNO %d: %s", STRINGIFY(clock_gettime), errno, strerror(errno));
                exit(EXIT_FAILURE);
        }
        unsigned int seed = ts.tv_sec ^ ts.tv_nsec;
        srand(seed);
        PRINT_INFO("RNG seeding successed; Seed = %u", seed);
}

uint32_t generate_initial_sequence_number(void)
{
        uint32_t high = rand() & 0xFFFF; /* Get only the 16 lower bits. */
        uint32_t low = rand() & 0xFFFF;  /* Get only the 16 lower bits. */
        uint32_t isn = (high << 16) | low;
        return isn;
}

microtcp_segment_t *create_microtcp_segment(microtcp_sock_t *_socket, uint16_t _control, microtcp_payload_t _payload)
{
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(NULL, _socket, ANY);
        RETURN_ERROR_IF_MICROTCP_PAYLOAD_INVALID(NULL, _payload);

        microtcp_segment_t *new_segment = MALLOC_LOG(new_segment, sizeof(microtcp_segment_t));
        if (new_segment == NULL)
                return NULL;

        /* Header initialization. */
        new_segment->header.seq_number = _socket->seq_number;
        new_segment->header.ack_number = _socket->ack_number;
        new_segment->header.control = _control;
        new_segment->header.window = _socket->curr_win_size; /* As sender we advertise our receive window, so opposite host wont overflow us .*/
        new_segment->header.data_len = _payload.size;
        new_segment->header.future_use0 = 0; /* TBD in Phase B */
        new_segment->header.future_use1 = 0; /* TBD in Phase B */
        new_segment->header.future_use2 = 0; /* TBD in Phase B */
        new_segment->header.checksum = 0;    /* CRC32 checksum is calculated after streamlining this packet. */

        /* Set the payload pointer. We do not do deep copy, to avoid wasting memory.
         * MicroTCP will have to create a bitstream, where header and payload will have
         * to be compacted together. In that case deep copying is unavoidable.
         */
        new_segment->raw_payload_bytes = _payload.raw_bytes;

        return new_segment;
}

/* TODO: calculate checksum. */
void *serialize_microtcp_segment(microtcp_segment_t *_segment)
{
        if (_segment == NULL)
                PRINT_ERROR_RETURN(NULL, "Given pointer to _segment was NULL.");

        uint16_t header_length = sizeof(_segment->header); /* Valid segments contain at lease sizeof(microtcp_header_t) bytes. */
        uint16_t payload_length = _segment->header.data_len;

        void *bytestream_buffer = MALLOC_LOG(bytestream_buffer, header_length + payload_length);
        if (bytestream_buffer == NULL)
                return NULL;

        memcpy(bytestream_buffer, &(_segment->header), header_length);
        memcpy(bytestream_buffer + header_length, _segment->raw_payload_bytes, payload_length);

        return bytestream_buffer;
}

microtcp_segment_t *extract_microtcp_segment(void *bytestream_buffer)
{

}

ssize_t send_syn_segment(microtcp_sock_t *_socket, const struct sockaddr *_address, socklen_t _address_len)
{
        microtcp_segment_t *syn_segment = create_microtcp_segment(_socket, SYN_BIT, (microtcp_payload_t){.raw_bytes = NULL, .size = 0});
        if (syn_segment == NULL)
                PRINT_ERROR_RETURN(MICROTCP_CONNECT_FAILURE, "Failed creating %s.", STRINGIFY(syn_segment));
        void *serial_syn_segment = serialize_microtcp_segment(syn_segment);
        if (serial_syn_segment == NULL)
                PRINT_ERROR_RETURN(MICROTCP_CONNECT_FAILURE, "Failed to serialize %s", STRINGIFY(syn_segment));
        size_t syn_segment_length = ((sizeof(syn_segment->header)) + syn_segment->header.data_len);
        ssize_t sendto_ret_val = sendto(_socket->sd, serial_syn_segment, syn_segment_length, NO_SENDTO_FLAGS, _address, _address_len);
        FREE_LOG(syn_segment);
        FREE_LOG(serial_syn_segment);
        if (sendto_ret_val != syn_segment_length)
                PRINT_WARNING_RETURN(sendto_ret_val, "SYN segment sending failed.");
        PRINT_INFO_RETURN(sendto_ret_val, "SYN segment sent.");
}

ssize_t receive_synack_segment(microtcp_sock_t *_socket, struct sockaddr *_address, socklen_t _address_len)
{
        size_t expected_segment_size = sizeof(microtcp_segment_t);
        void *const synack_buffer = MALLOC_LOG(synack_buffer, expected_segment_size);
        recvfrom(_socket->sd, synack_buffer, expected_segment_size, NO_SENDTO_FLAGS, _address, &_address_len);



}