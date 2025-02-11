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

static inline ssize_t receive_bytestream(microtcp_sock_t *_socket, struct sockaddr *_address, socklen_t _address_len, int _recvfrom_flgas);
static inline ssize_t receive_segment(microtcp_sock_t *_socket, struct sockaddr *_address, socklen_t _address_len, uint16_t _required_control, _Bool _block);
static inline ssize_t send_segment(microtcp_sock_t *_socket, const struct sockaddr *const _address, const socklen_t _address_len, microtcp_segment_t *_segment);
static inline ssize_t send_control_segment(microtcp_sock_t *const _socket, const struct sockaddr *const _address, const socklen_t _address_len,
                                           uint16_t _control, microtcp_state_t _required_state);

/* Internal MACRO defines. */
#define NO_SENDTO_FLAGS 0
#define NO_RECVFROM_FLAGS 0

#define MAX_CONSECUTIVE_SEND_MISMATCH_ERRORS 100

#define LOG_WARNING_RETURN_CONTROL_MISMATCH(_return_value, _received, _expected)                \
        LOG_WARNING_RETURN((_return_value), "Control-field: Received = `%s`; Expected = `%s`.", \
                           get_microtcp_control_to_string((_received)), get_microtcp_control_to_string((_expected)))

/* Declarations of static functions implemented in this file: */
/**
 * @returns the number of bytes, validly send into the socket.
 * This implies that a packet was validly send into the socket.
 */
static ssize_t send_control_segment(microtcp_sock_t *const _socket, const struct sockaddr *const _address,
                                    const socklen_t _address_len, uint16_t _control, microtcp_state_t _required_state);
static ssize_t receive_control_segment(microtcp_sock_t *const _socket, struct sockaddr *const _address, socklen_t _address_len,
                                       uint32_t _required_ack_number, uint16_t _required_control, const microtcp_state_t _required_state);

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
        return send_control_segment(_socket, _address, _address_len, ACK_BIT, ~(INVALID | RESET));
}

ssize_t send_finack_control_segment(microtcp_sock_t *const _socket, const struct sockaddr *const _address, const socklen_t _address_len)
{
        return send_control_segment(_socket, _address, _address_len, FIN_BIT | ACK_BIT, CLOSING_BY_HOST | CLOSING_BY_PEER);
}

ssize_t send_rstack_control_segment(microtcp_sock_t *const _socket, const struct sockaddr *const _address, const socklen_t _address_len)
{
        /* There is not equivilant receive function. Nobody awaits to receive RST, it just happens :D    */
        /* Every receive function returns special code if RST was received. So that's how you detect it... */
        return send_control_segment(_socket, _address, _address_len, RST_BIT | ACK_BIT, ~(INVALID | RESET));
}

ssize_t send_winack_control_segment(microtcp_sock_t *const _socket)
{
        return send_control_segment(_socket, _socket->peer_address, sizeof(*_socket->peer_address), WIN_BIT | ACK_BIT, ESTABLISHED);
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
        return receive_control_segment(_socket, _address, _address_len, _required_ack_number, ACK_BIT, ~(INVALID | RESET));
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
                                              uint32_t _required_ack_number, uint16_t _required_control, const microtcp_state_t _required_state)
{
        /* Quick argument check. */
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(RECV_SEGMENT_FATAL_ERROR, _socket, _required_state);
        RETURN_ERROR_IF_SOCKADDR_INVALID(RECV_SEGMENT_FATAL_ERROR, _address);
        RETURN_ERROR_IF_SOCKET_ADDRESS_LENGTH_INVALID(RECV_SEGMENT_FATAL_ERROR, _address_len, sizeof(struct sockaddr));

        ssize_t receive_segment_ret_val = receive_segment(_socket, _address, _address_len, _required_control, true);
        if (receive_segment_ret_val <= RECV_SEGMENT_EXCEPTION_THRESHOLD)
                return receive_segment_ret_val;
        microtcp_segment_t *control_segment = _socket->segment_receive_buffer;
        if (RARE_CASE((_required_control & SYN_BIT) && !(control_segment->header.control & SYN_BIT)))
                LOG_WARNING_RETURN_CONTROL_MISMATCH(RECV_SEGMENT_SYN_EXPECTED, control_segment->header.control, _required_control);

        if (control_segment->header.data_len != 0)
                LOG_WARNING_RETURN(RECV_SEGMENT_CARRIES_DATA, "Received segment %s contains %d bytes of payload.",
                                   get_microtcp_control_to_string(control_segment->header.control), control_segment->header.data_len);
        /* IGNORE check if waiting to receive SYN (server side). */
        if (_required_control != SYN_BIT && control_segment->header.ack_number != _required_ack_number)
                LOG_WARNING_RETURN(RECV_SEGMENT_ERROR, "ACK number mismatch occured. (Got = %d)|(Required = %d)",
                                   control_segment->header.ack_number, _socket->seq_number + 1);

        _socket->peer_win_size = control_segment->header.window;
        LOG_INFO_RETURN(receive_segment_ret_val, "%s segment received.", get_microtcp_control_to_string(_required_control));
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

        DEBUG_SMART_ASSERT(receive_segment_ret_val >= (ssize_t)MICROTCP_HEADER_SIZE);

        LOG_INFO_RETURN(data_segment->header.data_len, "data segment received; seq_number = %u, data_len = %u",
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

        LOG_INFO_RETURN(receive_segment_ret_val, "%s segment received.", get_microtcp_control_to_string(ACK_BIT));
}

static inline ssize_t receive_segment(microtcp_sock_t *_socket, struct sockaddr *const _address, const socklen_t _address_len,
                                      const uint16_t _required_control, const _Bool _block)
{
        const int recvfrom_flags = _block ? 0 : MSG_DONTWAIT;
        ssize_t receive_bytestream_ret_val = receive_bytestream(_socket, _address, _address_len, recvfrom_flags);
        if (receive_bytestream_ret_val <= RECV_SEGMENT_EXCEPTION_THRESHOLD)
                return receive_bytestream_ret_val;

        extract_microtcp_segment(&_socket->segment_receive_buffer, _socket->bytestream_receive_buffer, receive_bytestream_ret_val);
        microtcp_segment_t *segment = _socket->segment_receive_buffer;
        DEBUG_SMART_ASSERT(segment != NULL);

        if (RARE_CASE(segment->header.control & RST_BIT)) /* We test if RST is contained in control field, ACK_BIT might also be contained. (Combinations can singal reasons of why RST was sent). */
                LOG_WARNING_RETURN_CONTROL_MISMATCH(RECV_SEGMENT_RST_RECEIVED, segment->header.control, _required_control);
        if (RARE_CASE(segment->header.control == (WIN_BIT | ACK_BIT)))
                LOG_INFO_RETURN(RECV_SEGMENT_WINACK_RECEIVED, "Peer send WINACK: Requests to find our window size.");
        if (RARE_CASE((segment->header.control == (FIN_BIT | ACK_BIT)) && (_required_control == ACK_BIT)))
                LOG_WARNING_RETURN_CONTROL_MISMATCH(RECV_SEGMENT_FINACK_UNEXPECTED, segment->header.control, _required_control);
        if (RARE_CASE(segment->header.control != _required_control))
                LOG_WARNING_RETURN_CONTROL_MISMATCH(RECV_SEGMENT_ERROR, segment->header.control, _required_control);
        return receive_bytestream_ret_val;
}
#undef LOG_WARNING_RETURN_CONTROL_MISMATCH

static inline ssize_t receive_bytestream(microtcp_sock_t *_socket, struct sockaddr *const _address, socklen_t _address_len, int _recvfrom_flags)
{
        void *const bytestream_buffer = _socket->bytestream_receive_buffer;
        _recvfrom_flags |= MSG_TRUNC; /* Appending MSG_TRUNC flag, too catch large than MicroTCP allowed packets, and discard them. */

        ssize_t recvfrom_ret_val = recvfrom(_socket->sd, bytestream_buffer, MICROTCP_MTU, _recvfrom_flags, _address, &_address_len);
        DEBUG_SMART_ASSERT(_address_len == sizeof(struct sockaddr));
        DEBUG_SMART_ASSERT(recvfrom_ret_val != RECVFROM_SHUTDOWN); /* Underlying protocol is UDP, this should be impossible. */
        if (recvfrom_ret_val == RECVFROM_ERROR && errno == EWOULDBLOCK)
                return RECV_SEGMENT_TIMEOUT;
        if (RARE_CASE(recvfrom_ret_val == RECVFROM_ERROR))
                LOG_ERROR_RETURN(RECV_SEGMENT_FATAL_ERROR, "Receiving segment failed; recvfrom() set errno(%d):%s.", recvfrom_ret_val, errno, strerror(errno));
        if (!is_valid_microtcp_bytestream(bytestream_buffer, recvfrom_ret_val))
                LOG_WARNING_RETURN(RECV_SEGMENT_ERROR, "Received microtcp bytestream is corrupted.");
        update_socket_received_counters(_socket, recvfrom_ret_val);
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
                                           const socklen_t _address_len, uint16_t _control, microtcp_state_t _required_state)
{
        /* Quick argument check. */
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(SEND_SEGMENT_FATAL_ERROR, _socket, _required_state);
        RETURN_ERROR_IF_SOCKADDR_INVALID(SEND_SEGMENT_FATAL_ERROR, _address);
        RETURN_ERROR_IF_SOCKET_ADDRESS_LENGTH_INVALID(SEND_SEGMENT_FATAL_ERROR, _address_len, sizeof(struct sockaddr));

        /* Create handshake segment. */
        const microtcp_payload_t payload = {.raw_bytes = NULL, .size = 0};
        microtcp_segment_t *control_segment = construct_microtcp_segment(_socket, _socket->seq_number, _control, payload);
        DEBUG_SMART_ASSERT(control_segment != NULL); /* If socket is properly initialized, assert should never fail. */

        return send_segment(_socket, _address, _address_len, control_segment);
}

static inline ssize_t send_segment(microtcp_sock_t *_socket, const struct sockaddr *const _address, const socklen_t _address_len, microtcp_segment_t *_segment)
{
        static _Thread_local size_t consecutive_sendto_errors = 0;
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
        {
                LOG_WARNING("Sending %s segment failed; sendto() sent %d bytes, microtcp_segment was %d bytes",
                            segment_type, sendto_ret_val, segment_length);
                consecutive_sendto_errors++;
                if (consecutive_sendto_errors > MAX_CONSECUTIVE_SEND_MISMATCH_ERRORS)
                        LOG_ERROR_RETURN(SEND_SEGMENT_FATAL_ERROR, "Max consecutive send mismatch errors reached.");
                return SEND_SEGMENT_ERROR;
        }
        consecutive_sendto_errors = 0;
        update_socket_sent_counters(_socket, sendto_ret_val);
        LOG_INFO_RETURN(sendto_ret_val, "%s segment sent.", segment_type);
}
