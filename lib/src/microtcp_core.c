#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>

#include "microtcp.h"
#include "microtcp_core.h"
#include "logging/logger.h"
#include "microtcp_defines.h"
#include "microtcp_common_macros.h"
#include "allocator/allocator.h"
#include "crc32.h"

#define RECVFROM_SHUTDOWN 0
#define RECVFROM_ERROR -1
#define SENDTO_ERROR -1

/* Declarations of static functions implemented in this file: */
/**
 * @returns the number of bytes, validly send into the socket.
 * This implies that a packet was validly send into the socket.
 */
static ssize_t send_handshake_segment(microtcp_sock_t *const _socket, const struct sockaddr *const _address,
                                      const socklen_t _address_len, uint16_t _control, mircotcp_state_t _required_state);
static ssize_t receive_handshake_segment(microtcp_sock_t *const _socket, struct sockaddr *const _address,
                                         socklen_t _address_len, uint16_t _control, mircotcp_state_t _required_state);

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

int set_socket_timeout(microtcp_sock_t *_socket, time_t _sec, time_t _usec)
{
        struct timeval timeout = {.tv_sec = _sec, .tv_usec = _usec};
        return setsockopt(_socket->sd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
}

void generate_initial_sequence_number(microtcp_sock_t *_socket)
{
        /* We let it cause a segmentation fault if '_socket' == NULL. */
        uint32_t high = rand() & 0xFFFF;   /* Get only the 16 lower bits. */
        uint32_t low = rand() & 0xFFFF;    /* Get only the 16 lower bits. */
        uint32_t isn = (high << 16) | low; /* Combine 16-bit random of low and 16-bit random of high, to create a 32-bit random ISN. */
        _socket->seq_number = isn;

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
            .bytes_lost = 0,
            ._workaround_shutdown_address_ = NULL};
        return new_socket;
}

void cleanup_microtcp_socket(microtcp_sock_t *_socket)
{
        if (_socket == NULL)
        {
                LOG_ERROR("Invalid Argument: %s = NULL", STRINGIFY(_socket));
                return;
        }
        if (_socket->recvbuf != NULL)
                deallocate_receive_buffer(_socket);
        if (_socket->sd != POSIX_SOCKET_FAILURE_VALUE)
                close(_socket->sd);
        _socket->sd = POSIX_SOCKET_FAILURE_VALUE;
        _socket->buf_fill_level = 0;
        _socket->state = INVALID;
        _socket->_workaround_shutdown_address_ = NULL;
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

void *serialize_microtcp_segment(microtcp_segment_t *_segment)
{
        if (_segment == NULL)
                LOG_ERROR_RETURN(NULL, "Given pointer to _segment was NULL.");
        if (_segment->header.checksum != 0)
        {
                _segment->header.checksum = 0;
                LOG_WARNING("Checksum should be zeroed before serialization; Checksum field zeroed.");
        }

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

_Bool is_valid_microtcp_bytestream(void *_bytestream_buffer, size_t _bytestream_buffer_length)
{
        if (_bytestream_buffer == NULL || _bytestream_buffer_length < sizeof(microtcp_header_t))
                LOG_ERROR_RETURN(FALSE, "Invalid Arguments: %s = %x || %s = %d.",
                                 STRINGIFY(_bytestream_buffer), _bytestream_buffer,
                                 STRINGIFY(_bytestream_buffer_length), _bytestream_buffer_length);

        uint32_t extracted_checksum = ((microtcp_header_t *)_bytestream_buffer)->checksum;

        /* Zeroing checksum to calculate crc. */
        ((microtcp_header_t *)_bytestream_buffer)->checksum = 0;
        uint32_t calculated_checksum = crc32(_bytestream_buffer, _bytestream_buffer_length);
        ((microtcp_header_t *)_bytestream_buffer)->checksum = extracted_checksum;

        return calculated_checksum == extracted_checksum;
}

microtcp_segment_t *extract_microtcp_segment(void *_bytestream_buffer, size_t _bytestream_buffer_length)
{
        if (_bytestream_buffer == NULL || _bytestream_buffer_length < sizeof(microtcp_header_t))
                LOG_ERROR_RETURN(FALSE, "Invalid Arguments: %s = %x || %s = %d.",
                                 STRINGIFY(_bytestream_buffer), _bytestream_buffer,
                                 STRINGIFY(_bytestream_buffer_length), _bytestream_buffer_length);

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

/**
 * @returns pointer to the newly allocated recvbuf. If allocation fails returns NULL;
 * @brief There are two states where recvbuf memory allocation is possible.
 * Client allocates its recvbuf in connect(), socket in CLOSED state.
 * Server allocates its recvbuf in accept(),  socket in LISTEN  state.
 * So we use ANY for state parameter on the following socket check.
 */
void *allocate_receive_buffer(microtcp_sock_t *_socket)
{
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(NULL, _socket, ANY);
        if (_socket->recvbuf != NULL) /* Maybe memory is freed() but still we expect such pointers to be zeroed. */
                LOG_WARNING("recvbuf is not NULL before allocation; possible memory leak. Previous address: %x", _socket->recvbuf);

        if (_socket->buf_fill_level != 0)
                LOG_ERROR_RETURN(NULL, "recvbuf is not empty; allocation aborted. %s = %d",
                                 STRINGIFY(_socket->buf_fill_level), _socket->buf_fill_level);

        return _socket->recvbuf = CALLOC_LOG(_socket->recvbuf, MICROTCP_RECVBUF_LEN);
}

void deallocate_receive_buffer(microtcp_sock_t *_socket)
{
        if (_socket == NULL)
                LOG_ERROR("Invalid argument: %s = NULL.", STRINGIFY(_socket));
        /* Let it cause a segmentation fault.*/
        FREE_NULLIFY_LOG(_socket->recvbuf);
}

void update_socket_sent_counters(microtcp_sock_t *_socket, size_t _bytes_sent)
{
        /* Let it cause a segmentation fault if '_socket' == NULL. */
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
        /* Let it cause a segmentation fault if '_socket' == NULL. */
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
        /* Let it cause a segmentation fault if '_socket' == NULL. */
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

/**
 * @returns the number of bytes, it validly received.
 * This also implies that a packet was correctly received.
 */
static ssize_t send_handshake_segment(microtcp_sock_t *const _socket, const struct sockaddr *const _address,
                                      const socklen_t _address_len, uint16_t _required_control, mircotcp_state_t _required_state)
{
        /* Quick argument check. */
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(SEND_SEGMENT_FATAL_ERROR, _socket, _required_state);
        RETURN_ERROR_IF_SOCKADDR_INVALID(SEND_SEGMENT_FATAL_ERROR, _address);
        RETURN_ERROR_IF_SOCKET_ADDRESS_LENGTH_INVALID(SEND_SEGMENT_FATAL_ERROR, _address_len, sizeof(struct sockaddr));

        /* Create handshake segment. */
        microtcp_segment_t *handshake_segment = create_microtcp_segment(_socket, _required_control, (microtcp_payload_t){.raw_bytes = NULL, .size = 0});
        if (handshake_segment == NULL)
                LOG_ERROR_RETURN(SEND_SEGMENT_FATAL_ERROR, "Failed creating %s segment.", get_microtcp_control_to_string(_required_control));

        /* Convert it to bytestream. */
        void *bytestream_buffer = serialize_microtcp_segment(handshake_segment);
        if (bytestream_buffer == NULL)
                LOG_ERROR_RETURN(SEND_SEGMENT_FATAL_ERROR, "Failed to serialize %s segment", get_microtcp_control_to_string(_required_control));

        /* Send handshake segment to server with UDP's sendto(). */
        size_t segment_length = ((sizeof(handshake_segment->header)) + handshake_segment->header.data_len);
        ssize_t sendto_ret_val = sendto(_socket->sd, bytestream_buffer, segment_length, NO_SENDTO_FLAGS, _address, _address_len);

        /* Clean-up resources. */
        FREE_NULLIFY_LOG(handshake_segment);
        FREE_NULLIFY_LOG(bytestream_buffer);

        /* Log operation's outcome. */
        if (sendto_ret_val == SENDTO_ERROR)
                LOG_ERROR_RETURN(SEND_SEGMENT_FATAL_ERROR, "Sending %s segment failed. sendto() errno(%d):%s.",
                                 get_microtcp_state_to_string(_required_control), errno, strerror(errno));
        if (sendto_ret_val != segment_length)
                LOG_WARNING_RETURN(SEND_SEGMENT_ERROR, "Failed sending %s segment. sendto() sent %d bytes, but was asked to sent %d bytes",
                                   get_microtcp_state_to_string(_required_control), sendto_ret_val, segment_length);
        LOG_INFO_RETURN(sendto_ret_val, "%s segment sent.", get_microtcp_control_to_string(_required_control));
}

/**
 * @returns the number of bytes, it validly received.
 * This also implies that a packet was correctly received.
 */
static ssize_t receive_handshake_segment(microtcp_sock_t *const _socket, struct sockaddr *const _address,
                                         socklen_t _address_len, uint16_t _required_control, mircotcp_state_t _required_state)
{
        /* Quick argument check. */
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(RECV_SEGMENT_FATAL_ERROR, _socket, _required_state);
        RETURN_ERROR_IF_SOCKADDR_INVALID(RECV_SEGMENT_FATAL_ERROR, _address);
        RETURN_ERROR_IF_SOCKET_ADDRESS_LENGTH_INVALID(RECV_SEGMENT_FATAL_ERROR, _address_len, sizeof(struct sockaddr));

        if ((_required_control & SYN_BIT) == SYN_BIT && _socket->buf_fill_level != 0)
                LOG_ERROR_RETURN(RECV_SEGMENT_FATAL_ERROR, "Receive buffer contains registered bytes before establishing a connection.");

        /* All handshake segments contain no data. */
        size_t expected_segment_size = sizeof(microtcp_header_t);
        void *const bytestream_buffer = _socket->recvbuf;

        ssize_t recvfrom_ret_val = recvfrom(_socket->sd, bytestream_buffer, expected_segment_size, NO_RECVFROM_FLAGS, _address, &_address_len);
        if (recvfrom_ret_val == RECVFROM_ERROR && (errno == EAGAIN || errno == EWOULDBLOCK))
                LOG_WARNING_RETURN(RECV_SEGMENT_TIMEOUT, "recvfrom timeout.");
        if (recvfrom_ret_val == RECVFROM_SHUTDOWN)
                LOG_ERROR_RETURN(RECV_SEGMENT_FATAL_ERROR, "recvfrom returned 0, which points a closed connection; but underlying protocol is UDP, so this should not happen.");
        if (recvfrom_ret_val == RECVFROM_ERROR)
                LOG_ERROR_RETURN(RECV_SEGMENT_FATAL_ERROR, "recvfrom returned %d, errno(%d):%s.", recvfrom_ret_val, errno, strerror(errno));
        if (recvfrom_ret_val < expected_segment_size)
                LOG_WARNING_RETURN(RECV_SEGMENT_ERROR, "Received bytestream size is less than %s.", STRINGIFY(expected_segment_size));
        if (!is_valid_microtcp_bytestream(bytestream_buffer, recvfrom_ret_val))
                LOG_WARNING_RETURN(RECV_SEGMENT_ERROR, "Received microtcp bytestream is corrupted.");

        microtcp_segment_t *handshake_segment = extract_microtcp_segment(bytestream_buffer, recvfrom_ret_val);
        if (handshake_segment == NULL)
                LOG_ERROR_RETURN(RECV_SEGMENT_FATAL_ERROR, "Extracting SYN-ACK segment resulted to a NULL pointer.");
        if (handshake_segment->header.control != _required_control)
                LOG_ERROR_RETURN(RECV_SEGMENT_ERROR, "Control: Received = `%s`; Expected = `%s`.",
                                 get_microtcp_control_to_string(handshake_segment->header.control), get_microtcp_control_to_string(_required_control));

        /* Ignore check if waiting to receive SYN (server side). */
        if (_required_control != SYN_BIT && handshake_segment->header.ack_number != _socket->seq_number) /* Not `+1` as FSM already incremented SN. */
                LOG_ERROR_RETURN(RECV_SEGMENT_ERROR, "Received segment %s and ACK number mismatch occured. (Got = %d)|(Expected = %d)",
                                 get_microtcp_control_to_string(_required_control), handshake_segment->header.ack_number, _socket->seq_number + 1);

        _socket->ack_number = handshake_segment->header.seq_number + 1;
        _socket->peer_win_size = handshake_segment->header.window;
        LOG_INFO_RETURN(recvfrom_ret_val, "%s segment received.", get_microtcp_control_to_string(_required_control));
}

ssize_t send_syn_segment(microtcp_sock_t *const _socket, const struct sockaddr *const _address, const socklen_t _address_len)
{
        return send_handshake_segment(_socket, _address, _address_len, SYN_BIT, CLOSED);
}

ssize_t send_synack_segment(microtcp_sock_t *const _socket, const struct sockaddr *const _address, const socklen_t _address_len)
{
        return send_handshake_segment(_socket, _address, _address_len, SYN_BIT | ACK_BIT, LISTEN);
}

ssize_t send_ack_segment(microtcp_sock_t *const _socket, const struct sockaddr *const _address, const socklen_t _address_len)
{
        return send_handshake_segment(_socket, _address, _address_len, ACK_BIT, ANY);
}

ssize_t send_finack_segment(microtcp_sock_t *const _socket, const struct sockaddr *const _address, const socklen_t _address_len)
{
        return send_handshake_segment(_socket, _address, _address_len, FIN_BIT | ACK_BIT, CLOSING_BY_HOST);
}

ssize_t receive_syn_segment(microtcp_sock_t *const _socket, struct sockaddr *const _address, const socklen_t _address_len)
{
        return receive_handshake_segment(_socket, _address, _address_len, SYN_BIT, LISTEN);
}

ssize_t receive_synack_segment(microtcp_sock_t *const _socket, struct sockaddr *const _address, const socklen_t _address_len)
{
        return receive_handshake_segment(_socket, _address, _address_len, SYN_BIT | ACK_BIT, CLOSED);
}

ssize_t receive_ack_segment(microtcp_sock_t *const _socket, struct sockaddr *const _address, const socklen_t _address_len)
{
        return receive_handshake_segment(_socket, _address, _address_len, ACK_BIT, ANY);
}

ssize_t receive_finack_segment(microtcp_sock_t *const _socket, struct sockaddr *const _address, const socklen_t _address_len)
{
        return receive_handshake_segment(_socket, _address, _address_len, FIN_BIT | ACK_BIT, CLOSING_BY_HOST);
}

void set_workaround_shutdown_address(microtcp_sock_t *const _socket, struct sockaddr *const _address)
{
        if (_socket == NULL)
                LOG_ERROR("Invalid argument: %s == NULL", STRINGIFY(_socket));
        if (_address == NULL)
                LOG_ERROR("Invalid argument: %s == NULL", STRINGIFY(_address));
        _socket->_workaround_shutdown_address_ = _address;
}
