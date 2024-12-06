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
#include "crc32.h"

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
        uint32_t high = rand() & 0xFFFF;   /* Get only the 16 lower bits. */
        uint32_t low = rand() & 0xFFFF;    /* Get only the 16 lower bits. */
        uint32_t isn = (high << 16) | low; /* Combine 16-bit random of low and 16-bit random of high, to create a 32-bit random ISN. */
        return isn;
}

microtcp_segment_t *create_microtcp_segment(microtcp_sock_t *_socket, uint16_t _control, microtcp_payload_t _payload)
{
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(NULL, _socket, ANY);
        RETURN_ERROR_IF_MICROTCP_PAYLOAD_INVALID(NULL, _payload);

        microtcp_segment_t *new_segment = CALLOC_LOG(new_segment, sizeof(microtcp_segment_t));
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
        uint16_t bytestream_buffer_length = header_length + payload_length;
        void *bytestream_buffer = CALLOC_LOG(bytestream_buffer, bytestream_buffer_length);
        if (bytestream_buffer == NULL)
                return NULL;

        memcpy(bytestream_buffer, &(_segment->header), header_length);
        memcpy(bytestream_buffer + header_length, _segment->raw_payload_bytes, payload_length);

        /* Calculate crc32 checksum: */
        uint32_t checksum_result = crc32(bytestream_buffer, bytestream_buffer_length);

        /* Implant the CRC checksum. */
        ((microtcp_header_t *)bytestream_buffer)->checksum = checksum_result;

        return bytestream_buffer;
}

static inline int is_valid_microtcp_bytestream(void *_bytestream_buffer, size_t _bytestream_buffer_length)
{
        uint32_t extracted_checksum = ((microtcp_header_t *)_bytestream_buffer)->checksum;

        /* Zeroing checksum to calculate crc. */
        ((microtcp_header_t *)_bytestream_buffer)->checksum = 0;
        uint32_t calculated_checksum = crc32(_bytestream_buffer, _bytestream_buffer_length);

        return calculated_checksum == extracted_checksum;
}

microtcp_segment_t *extract_microtcp_segment(void *_bytestream_buffer, size_t _bytestream_buffer_length)
{
        if (_bytestream_buffer_length < sizeof(microtcp_header_t))
                PRINT_ERROR_RETURN(NULL, "MicroTCP segment size can not be less that sizeof(%s)", STRINGIFY(microtcp_header_t));

        size_t payload_size = _bytestream_buffer_length - sizeof(microtcp_header_t);

        microtcp_segment_t *new_segment = CALLOC_LOG(new_segment, sizeof(microtcp_segment_t));
        if (new_segment == NULL)
                return NULL;

        new_segment->raw_payload_bytes = CALLOC_LOG(new_segment->raw_payload_bytes, payload_size);
        if (new_segment->raw_payload_bytes == NULL)
        {
                FREE_NULLIFY_LOG(new_segment); /* Free partial allocated memory */
                return NULL;
        }

        memcpy(&(new_segment->header), _bytestream_buffer, sizeof(microtcp_header_t));
        memcpy(new_segment->raw_payload_bytes, _bytestream_buffer + sizeof(microtcp_header_t), payload_size);

        return new_segment;
}

ssize_t send_syn_segment(microtcp_sock_t *_socket, const struct sockaddr *_address, socklen_t _address_len)
{
        microtcp_segment_t *syn_segment = create_microtcp_segment(_socket, SYN_BIT, (microtcp_payload_t){.raw_bytes = NULL, .size = 0});
        if (syn_segment == NULL)
                PRINT_ERROR_RETURN(MICROTCP_CONNECT_FAILURE, "Failed creating %s.", STRINGIFY(syn_segment));
        void *syn_bytestream_buffer = serialize_microtcp_segment(syn_segment);
        if (syn_bytestream_buffer == NULL)
                PRINT_ERROR_RETURN(MICROTCP_CONNECT_FAILURE, "Failed to serialize %s", STRINGIFY(syn_segment));
        size_t syn_segment_length = ((sizeof(syn_segment->header)) + syn_segment->header.data_len);
        ssize_t sendto_ret_val = sendto(_socket->sd, syn_bytestream_buffer, syn_segment_length, NO_SENDTO_FLAGS, _address, _address_len);
        FREE_NULLIFY_LOG(syn_segment);
        FREE_NULLIFY_LOG(syn_bytestream_buffer);
        if (sendto_ret_val != syn_segment_length)
                PRINT_WARNING_RETURN(sendto_ret_val, "SYN segment sending failed.");
        PRINT_INFO_RETURN(sendto_ret_val, "SYN segment sent.");
}

ssize_t receive_synack_segment(microtcp_sock_t *_socket, struct sockaddr *_address, socklen_t _address_len)
{
        // size_t expected_segment_size = sizeof(microtcp_segment_t);
        // // void *const synack_bytestream_buffer = _socket->recvbuf + _socket->buf_fill_level;

        // if (_socket->buf_fill_level + expected_segment_size > MICROTCP_RECVBUF_LEN)
        //         PRINT_WARNING_RETURN(0, "Not enough space in the receive buffer, to receive syn-ack segment. (%s = %u)|(%s = %u)|(Limit = %d)",
        //                              STRINGIFY(_socket->buf_fill_level), _socket->buf_fill_level,
        //                              STRINGIFY(expected_segment_size), expected_segment_size,
        //                              MICROTCP_RECVBUF_LEN);

        // ssize_t recvfrom_ret_val = recvfrom(_socket->sd, synack_bytestream_buffer, expected_segment_size, NO_SENDTO_FLAGS, _address, &_address_len);
        // if (recvfrom_ret_val != expected_segment_size)
        //         PRINT_WARNING("Received bytestream size mismatch.");

        // if (!is_valid_microtcp_bytestream(synack_bytestream_buffer, recvfrom_ret_val))
        //         PRINT_WARNING("Received microtcp bytestream is corrupted.");

        // microtcp_segment_t *synack_segment = extract_microtcp_segment(synack_bytestream_buffer, recvfrom_ret_val);
        // /* You where heere, CAUSe compile eeror to return hre: */
        /* You where heere, CAUSe compile eeror to return hre: */
        /* You where heere, CAUSe compile eeror to return hre: */
        /* You where heere, CAUSe compile eeror to return hre: */
        /* You where heere, CAUSe compile eeror to return hre: */
        /* You where heere, CAUSe compile eeror to return hre: */
        /* You where heere, CAUSe compile eeror to return hre: */
        /* You where heere, CAUSe compile eeror to return hre: */
        /* You where heere, CAUSe compile eeror to return hre: */
        /* You where heere, CAUSe compile eeror to return hre: */
        /* You where heere, CAUSe compile eeror to return hre: */
        /* You where heere, CAUSe compile eeror to return hre: */
        /* You where heere, CAUSe compile eeror to return hre: */
        /* You where heere, CAUSe compile eeror to return hre: */
        /* You where heere, CAUSe compile eeror to return hre: */
        /* You where heere, CAUSe compile eeror to return hre: */
        /* You where heere, CAUSe compile eeror to return hre: */
        /* You where heere, CAUSe compile eeror to return hre: */
        /* You where heere, CAUSe compile eeror to return hre: */
        /* You where heere, CAUSe compile eeror to return hre: */
        /* You where heere, CAUSe compile eeror to return hre: */
        /* You where heere, CAUSe compile eeror to return hre: */
        /* You where heere, CAUSe compile eeror to return hre: */
        return 0 ;
}

/**
 * @returns pointer to the newly allocated cb_recvbuf. If allocation fails returns NULL;
 */
void *allocate_receive_buffer(microtcp_sock_t *_socket)
{
        /* There are two states where cb_recvbuf memory allocation is possible.
         * Client allocates its cb_recvbuf in connect(), socket in CLOSED state.
         * Server allocates its cb_recvbuf in accept(),  socket in BOUND  state.
         * So we use ANY for state parameter on the following socket check.
         */
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(NULL, _socket, ANY);
        if (_socket->cb_recvbuf != NULL) /* Maybe memory is freed() but still we expect such pointers to be zeroed. */
                PRINT_WARNING("cb_recvbuf is not NULL before allocation; possible memory leak. Previous address: %x", _socket->cb_recvbuf);

        if (_socket->buf_fill_level != 0)
                PRINT_ERROR_RETURN(NULL, "recvbuf is not empty; allocation aborted. %s = %d",
                                   STRINGIFY(_socket->buf_fill_level), _socket->buf_fill_level);

        return _socket->cb_recvbuf = cb_create(MICROTCP_RECVBUF_LEN);
}