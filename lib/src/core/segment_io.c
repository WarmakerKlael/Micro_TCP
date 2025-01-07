#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include "core/segment_io.h"
#include "core/segment_processing.h"
#include "logging/microtcp_logger.h"
#include "microtcp.h"
#include "microtcp_core_macros.h"
#include "microtcp_helper_functions.h"
#include "microtcp_helper_macros.h"
#include <errno.h>
#include "microtcp.h"
#include "core/segment_processing.h"
#include "core/socket_stats_updater.h"
#include "core/send_queue.h"
#include "core/segment_io.h"
#include "smart_assert.h"
#include "microtcp_core_macros.h"
#include "logging/microtcp_logger.h"

static __always_inline uint32_t get_most_recent_ack(const uint32_t _ack1, const uint32_t _ack2);
static inline ssize_t receive_bytestream(microtcp_sock_t *_socket, struct sockaddr *_address, socklen_t _address_len, _Bool _block);
static inline ssize_t receive_segment(microtcp_sock_t *_socket, struct sockaddr *_address, socklen_t _address_len, uint16_t _required_control, _Bool _block);
static inline ssize_t send_segment(microtcp_sock_t *_socket, const struct sockaddr *const _address, const socklen_t _address_len, microtcp_segment_t *_segment);
static inline ssize_t send_control_segment(microtcp_sock_t *const _socket, const struct sockaddr *const _address, const socklen_t _address_len,
                                           uint16_t _required_control, mircotcp_state_t _required_state);

/* Internal MACRO defines. */
#define NO_SENDTO_FLAGS 0
#define NO_RECVFROM_FLAGS 0

#define RECVFROM_SHUTDOWN 0
#define RECVFROM_ERROR -1
#define SENDTO_ERROR -1

/* Declarations of static functions implemented in this file: */
/**
 * @returns the number of bytes, validly send into the socket.
 * This implies that a packet was validly send into the socket.
 */
static ssize_t send_control_segment(microtcp_sock_t *const _socket, const struct sockaddr *const _address,
                                    const socklen_t _address_len, uint16_t _control, mircotcp_state_t _required_state);
static ssize_t receive_control_segment(microtcp_sock_t *const _socket, struct sockaddr *const _address, socklen_t _address_len,
                                       uint32_t _required_ack_number, uint16_t _required_control, const mircotcp_state_t _required_state);

ssize_t send_syn_control_segment(microtcp_sock_t *const _socket, const struct sockaddr *const _address, const socklen_t _address_len)
{
        return send_control_segment(_socket, _address, _address_len, SYN_BIT, CLOSED);
}

ssize_t send_synack_control_segment(microtcp_sock_t *const _socket, const struct sockaddr *const _address, const socklen_t _address_len)
{
        return send_control_segment(_socket, _address, _address_len, SYN_BIT | ACK_BIT, LISTEN);
}

ssize_t send_ack_control_segment(microtcp_sock_t *const _socket, const struct sockaddr *const _address, const socklen_t _address_len)
{
        return send_control_segment(_socket, _address, _address_len, ACK_BIT, ~INVALID);
}

ssize_t send_finack_control_segment(microtcp_sock_t *const _socket, const struct sockaddr *const _address, const socklen_t _address_len)
{
        return send_control_segment(_socket, _address, _address_len, FIN_BIT | ACK_BIT, CLOSING_BY_HOST | CLOSING_BY_PEER);
}

ssize_t send_rstack_control_segment(microtcp_sock_t *const _socket, const struct sockaddr *const _address, const socklen_t _address_len)
{
        /* There is not equivilant receive function. Nobody awaits to receive RST, it just happens :D    */
        /* Every receive function returns special code if RST was received. So that's how you detect it... */
        return send_control_segment(_socket, _address, _address_len, RST_BIT | ACK_BIT, LISTEN | ESTABLISHED | CLOSING_BY_HOST | CLOSING_BY_PEER);
}

ssize_t receive_syn_control_segment(microtcp_sock_t *const _socket, struct sockaddr *const _address, const socklen_t _address_len)
{
#define ACK_NUMBER_NOT_REQUIRED 0
        return receive_control_segment(_socket, _address, _address_len, ACK_NUMBER_NOT_REQUIRED, SYN_BIT, LISTEN);
#undef ACK_NUMBER_NOT_REQUIRED
}

ssize_t receive_synack_control_segment(microtcp_sock_t *_socket, struct sockaddr *_address, socklen_t _address_len, uint32_t _required_ack_number)
{
        return receive_control_segment(_socket, _address, _address_len, _required_ack_number, SYN_BIT | ACK_BIT, CLOSED);
}

ssize_t receive_ack_control_segment(microtcp_sock_t *_socket, struct sockaddr *_address, socklen_t _address_len, uint32_t _required_ack_number)
{
        return receive_control_segment(_socket, _address, _address_len, _required_ack_number, ACK_BIT, ~INVALID);
}

ssize_t receive_finack_control_segment(microtcp_sock_t *_socket, struct sockaddr *_address, socklen_t _address_len, uint32_t _required_ack_number)
{
        return receive_control_segment(_socket, _address, _address_len, _required_ack_number,
                                       FIN_BIT | ACK_BIT, ESTABLISHED | CLOSING_BY_HOST | CLOSING_BY_PEER);
}

/**
 * @returns the number of bytes, it validly received.
 * This also implies that a packet was correctly received.
 */
static inline ssize_t receive_control_segment(microtcp_sock_t *const _socket, struct sockaddr *const _address, socklen_t _address_len,
                                              uint32_t _required_ack_number, uint16_t _required_control, const mircotcp_state_t _required_state)
{
        /* Quick argument check. */
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(RECV_SEGMENT_FATAL_ERROR, _socket, _required_state);
        RETURN_ERROR_IF_SOCKADDR_INVALID(RECV_SEGMENT_FATAL_ERROR, _address);
        RETURN_ERROR_IF_SOCKET_ADDRESS_LENGTH_INVALID(RECV_SEGMENT_FATAL_ERROR, _address_len, sizeof(struct sockaddr));

        ssize_t receive_segment_ret_val = receive_segment(_socket, _address, _address_len, _required_control, TRUE);
        if (receive_segment_ret_val <= RECV_SEGMENT_EXCEPTION_THRESHOLD)
                return receive_segment_ret_val;

        microtcp_segment_t *control_segment = _socket->segment_receive_buffer;
        if (control_segment->header.data_len != 0)
                LOG_WARNING_RETURN(RECV_SEGMENT_CARRIES_DATA, "Received segment %s contains %d bytes of payload.",
                                   get_microtcp_control_to_string(control_segment->header.control), control_segment->header.data_len);
        /* IGNORE check if waiting to receive SYN (server side). */
        if (_required_control != SYN_BIT && control_segment->header.ack_number != _required_ack_number)
                LOG_ERROR_RETURN(RECV_SEGMENT_ERROR, "ACK number mismatch occured. (Got = %d)|(Required = %d)",
                                 control_segment->header.ack_number, _socket->seq_number + 1);

        if (RARE_CASE(_required_control & SYN_BIT)) /* It is RARE_CASE as it only happen in the beggining of the connection. */
                _socket->ack_number = control_segment->header.seq_number + SYN_SEQ_NUMBER_INCREMENT;
        else
                _socket->ack_number = get_most_recent_ack(_socket->ack_number, control_segment->header.seq_number + 1);

        _socket->peer_win_size = control_segment->header.window;
        LOG_INFO_RETURN(receive_segment_ret_val, "%s segment received.", get_microtcp_control_to_string(_required_control));
}

/* rt: stands for receiver_thread. */
ssize_t receive_rt_segment(microtcp_sock_t *const _socket, const _Bool _block)
{
        return receive_segment(_socket,
                               _socket->peer_address,
                               sizeof(*_socket->peer_address),
                               DATA_SEGMENT_CONTROL_FLAGS,
                               _block);
}

/* Just receive a data segment, and pass it to the receive buffer... not your job to pass it to assembly. */
ssize_t receive_data_segment(microtcp_sock_t *const _socket, const _Bool _block)
{
#ifdef DEBUG_MODE
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(RECV_SEGMENT_FATAL_ERROR, _socket, ESTABLISHED);
#endif /* DEBUG_MODE */
        ssize_t receive_segment_ret_val = receive_segment(_socket, _socket->peer_address, sizeof(*_socket->peer_address), DATA_SEGMENT_CONTROL_FLAGS, _block);
        if (receive_segment_ret_val <= RECV_SEGMENT_EXCEPTION_THRESHOLD)
                return receive_segment_ret_val;

        microtcp_segment_t *data_segment = _socket->segment_receive_buffer;
        _socket->ack_number = get_most_recent_ack(_socket->ack_number, data_segment->header.seq_number + 1);

        DEBUG_SMART_ASSERT(receive_segment_ret_val > 0);

        LOG_INFO_RETURN(data_segment->header.data_len, "data segment received; seq_number = %lu, data_len = %lu",
                        data_segment->header.seq_number, data_segment->header.data_len);
}

ssize_t receive_data_ack_segment(microtcp_sock_t *const _socket, const _Bool _block)
{
#ifdef DEBUG_MODE
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(RECV_SEGMENT_FATAL_ERROR, _socket, ESTABLISHED);
#endif /* DEBUG_MODE */
        ssize_t receive_segment_ret_val = receive_segment(_socket, _socket->peer_address, sizeof(*_socket->peer_address), ACK_BIT, _block);
        if (receive_segment_ret_val <= RECV_SEGMENT_EXCEPTION_THRESHOLD)
                return receive_segment_ret_val;

        microtcp_segment_t *control_segment = _socket->segment_receive_buffer;
        if (control_segment->header.data_len != 0)
                LOG_WARNING_RETURN(RECV_SEGMENT_CARRIES_DATA, "Received segment %s contains %d bytes of payload.",
                                   get_microtcp_control_to_string(control_segment->header.control), control_segment->header.data_len);

        _socket->ack_number = get_most_recent_ack(_socket->ack_number, control_segment->header.seq_number + 1);
        LOG_INFO_RETURN(receive_segment_ret_val, "%s segment received.", get_microtcp_control_to_string(ACK_BIT));
}

#define LOG_WARNING_RETURN_CONTROL_MISMATCH(_return_value, _received, _expected)                \
        LOG_WARNING_RETURN((_return_value), "Control-field: Received = `%s`; Expected = `%s`.", \
                           get_microtcp_control_to_string((_received)), get_microtcp_control_to_string((_expected)))

static inline ssize_t receive_segment(microtcp_sock_t *_socket, struct sockaddr *const _address, const socklen_t _address_len,
                                      const uint16_t _required_control, const _Bool _block)
{
        ssize_t receive_bytestream_ret_val = receive_bytestream(_socket, _address, _address_len, _block);
        if (receive_bytestream_ret_val <= RECV_SEGMENT_EXCEPTION_THRESHOLD)
                return receive_bytestream_ret_val;

        extract_microtcp_segment(&_socket->segment_receive_buffer, _socket->bytestream_receive_buffer, receive_bytestream_ret_val);
        microtcp_segment_t *segment = _socket->segment_receive_buffer;
        DEBUG_SMART_ASSERT(segment != NULL);

        if (RARE_CASE(segment->header.control & RST_BIT)) /* We test if RST is contained in control field, ACK_BIT might also be contained. (Combinations can singal reasons of why RST was sent). */
                LOG_WARNING_RETURN_CONTROL_MISMATCH(RECV_SEGMENT_RST_RECEIVED, segment->header.control, _required_control);
        if (RARE_CASE((_required_control & SYN_BIT) && !(segment->header.control & SYN_BIT)))
                LOG_WARNING_RETURN_CONTROL_MISMATCH(RECV_SEGMENT_SYN_EXPECTED, segment->header.control, _required_control);
        if (RARE_CASE((segment->header.control == (FIN_BIT | ACK_BIT)) && (_required_control == ACK_BIT)))
                LOG_WARNING_RETURN_CONTROL_MISMATCH(RECV_SEGMENT_FINACK_UNEXPECTED, segment->header.control, _required_control);
        if (RARE_CASE(segment->header.control != _required_control))
                LOG_WARNING_RETURN_CONTROL_MISMATCH(RECV_SEGMENT_ERROR, segment->header.control, _required_control);
        return receive_bytestream_ret_val;
}
#undef LOG_CONTROL_MISMATCH_WARNING

static inline ssize_t receive_bytestream(microtcp_sock_t *_socket, struct sockaddr *const _address, socklen_t _address_len, const _Bool _block)
{
        void *const bytestream_buffer = _socket->bytestream_receive_buffer;
        const int recvfrom_flags = MSG_TRUNC | (_block ? 0 : MSG_DONTWAIT);

        ssize_t recvfrom_ret_val = recvfrom(_socket->sd, bytestream_buffer, MICROTCP_MTU, recvfrom_flags, _address, &_address_len);
        DEBUG_SMART_ASSERT(_address_len == sizeof(struct sockaddr));
        DEBUG_SMART_ASSERT(recvfrom_ret_val != RECVFROM_SHUTDOWN); /* Underlying protocol is UDP, this should be impossible. */
        if (recvfrom_ret_val == RECVFROM_ERROR && errno == EWOULDBLOCK)
                return RECV_SEGMENT_TIMEOUT;
        if (RARE_CASE(recvfrom_ret_val == RECVFROM_ERROR))
                LOG_ERROR_RETURN(RECV_SEGMENT_FATAL_ERROR, "Receiving segment failed; recvfrom() set errno(%d):%s.", recvfrom_ret_val, errno, strerror(errno));
        if (!is_valid_microtcp_bytestream(bytestream_buffer, recvfrom_ret_val))
                LOG_WARNING_RETURN(RECV_SEGMENT_ERROR, "Received microtcp bytestream is corrupted.");
        return recvfrom_ret_val;
}

size_t send_data_segment(microtcp_sock_t *const _socket, const void *const _buffer, const size_t _segment_size, const uint32_t _seq_number)
{
        DEBUG_SMART_ASSERT(_socket != NULL, _buffer != NULL, _segment_size > 0);
#ifdef DEBUG_MODE
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(SEND_SEGMENT_FATAL_ERROR, _socket, ESTABLISHED);
#endif /* DEBUG_MODE */

        const microtcp_payload_t payload = {.raw_bytes = (uint8_t *)_buffer, .size = _segment_size};
        microtcp_segment_t *data_segment = construct_microtcp_segment(_socket, _seq_number, DATA_SEGMENT_CONTROL_FLAGS, payload);
        DEBUG_SMART_ASSERT(data_segment != NULL); /* If socket is properly initialized, assert should never fail. */

        return send_segment(_socket, _socket->peer_address, sizeof(*_socket->peer_address), data_segment);
}

static inline ssize_t send_control_segment(microtcp_sock_t *const _socket, const struct sockaddr *const _address,
                                           const socklen_t _address_len, uint16_t _required_control, mircotcp_state_t _required_state)
{
        /* Quick argument check. */
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(SEND_SEGMENT_FATAL_ERROR, _socket, _required_state);
        RETURN_ERROR_IF_SOCKADDR_INVALID(SEND_SEGMENT_FATAL_ERROR, _address);
        RETURN_ERROR_IF_SOCKET_ADDRESS_LENGTH_INVALID(SEND_SEGMENT_FATAL_ERROR, _address_len, sizeof(struct sockaddr));

        /* Create handshake segment. */
        const microtcp_payload_t payload = {.raw_bytes = NULL, .size = 0};
        microtcp_segment_t *control_segment = construct_microtcp_segment(_socket, _socket->seq_number, _required_control, payload);
        DEBUG_SMART_ASSERT(control_segment != NULL); /* If socket is properly initialized, assert should never fail. */

        return send_segment(_socket, _address, _address_len, control_segment);
}

static inline ssize_t send_segment(microtcp_sock_t *_socket, const struct sockaddr *const _address, const socklen_t _address_len, microtcp_segment_t *_segment)
{
        DEBUG_SMART_ASSERT(_socket != NULL, _address != NULL, _address_len == sizeof(struct sockaddr), _segment != NULL);

        /* Convert it to bytestream. */
        void *const bytestream_buffer = serialize_microtcp_segment(_socket, _segment);
        DEBUG_SMART_ASSERT(bytestream_buffer != NULL); /* If socket is properly initialized, assert should never fail. */

        const ssize_t segment_length = sizeof(_segment->header) + _segment->header.data_len;
        const ssize_t sendto_ret_val = sendto(_socket->sd, bytestream_buffer, segment_length, NO_SENDTO_FLAGS, _address, _address_len);

        const char *segment_type = (_segment->header.data_len > 0 ? "DATA" : get_microtcp_control_to_string(_segment->header.control));

        /* Log operation's outcome. */
        if (RARE_CASE(sendto_ret_val == SENDTO_ERROR))
                LOG_ERROR_RETURN(SEND_SEGMENT_FATAL_ERROR, "Sending %s segment failed. sendto() set errno(%d):%s.",
                                 segment_type, errno, strerror(errno));
        if (RARE_CASE(sendto_ret_val != segment_length))
                LOG_WARNING_RETURN(SEND_SEGMENT_ERROR, "Sending %s segment failed; sendto() sent %d bytes, microtcp_segment was %d bytes",
                                   segment_type, sendto_ret_val, segment_length);
        update_socket_sent_counters(_socket, sendto_ret_val);
        LOG_INFO_RETURN(sendto_ret_val, "%s segment sent.", segment_type);
}

/*-------------------UTILITY-------------------*/

/* Function to determine if `_ack1` is newer than `_ack2` */
static __always_inline uint32_t get_most_recent_ack(const uint32_t _ack1, const uint32_t _ack2)
{
        return (int32_t)(_ack1 - _ack2) > 0 ? _ack1 : _ack2;
}