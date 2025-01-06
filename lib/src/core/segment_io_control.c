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
#include "segment_io_internal.h"

/* Declarations of static functions implemented in this file: */
/**
 * @returns the number of bytes, validly send into the socket.
 * This implies that a packet was validly send into the socket.
 */
static ssize_t send_control_segment(microtcp_sock_t *const _socket, const struct sockaddr *const _address,
                                    const socklen_t _address_len, uint16_t _control, mircotcp_state_t _required_state);
static ssize_t receive_control_segment(microtcp_sock_t *const _socket, struct sockaddr *const _address, socklen_t _address_len,
                                       uint32_t _required_ack_number, uint16_t _required_control, const mircotcp_state_t _required_state);
static __always_inline uint32_t get_most_recent_ack(uint32_t _ack1, uint32_t _ack2);

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

ssize_t receive_ack_control_segment_async(microtcp_sock_t *const _socket, _Bool _block)
{
        const uint16_t _required_control = ACK_BIT;
        const ssize_t expected_segment_size = sizeof(microtcp_header_t);
        void *const bytestream_buffer = _socket->bytestream_receive_buffer;
        const int recvfrom_flags = MSG_TRUNC | (_block ? 0 : MSG_DONTWAIT);
        socklen_t peer_address_length = sizeof(*_socket->peer_address);

        ssize_t recvfrom_ret_val = recvfrom(_socket->sd, bytestream_buffer, MICROTCP_MTU, recvfrom_flags, _socket->peer_address, &peer_address_length);
        if (recvfrom_ret_val == RECVFROM_ERROR && errno == EWOULDBLOCK)
                return RECV_SEGMENT_TIMEOUT;
        if (RARE_CASE(recvfrom_ret_val == RECVFROM_SHUTDOWN))
                LOG_ERROR_RETURN(RECV_SEGMENT_FATAL_ERROR, "recvfrom returned 0, which points a closed connection; but underlying protocol is UDP, so this should not happen.");
        if (RARE_CASE(recvfrom_ret_val == RECVFROM_ERROR))
                LOG_ERROR_RETURN(RECV_SEGMENT_FATAL_ERROR, "recvfrom returned %d, errno(%d):%s.", recvfrom_ret_val, errno, strerror(errno));
        if (recvfrom_ret_val != expected_segment_size)
                LOG_WARNING_RETURN(RECV_SEGMENT_ERROR, "Received bytestream size is (%zd bytes) different than %s (%zu bytes).",
                                   recvfrom_ret_val, STRINGIFY(expected_segment_size), expected_segment_size);
        if (!is_valid_microtcp_bytestream(bytestream_buffer, recvfrom_ret_val))
                LOG_WARNING_RETURN(RECV_SEGMENT_ERROR, "Received microtcp bytestream is corrupted.");

        extract_microtcp_segment(&(_socket->segment_receive_buffer), bytestream_buffer, recvfrom_ret_val);
        microtcp_segment_t *control_segment = _socket->segment_receive_buffer;
        if (control_segment == NULL)
                LOG_ERROR_RETURN(RECV_SEGMENT_FATAL_ERROR, "Extracting %s segment resulted to a NULL pointer.", get_microtcp_control_to_string(_required_control));
        if (control_segment->header.control & RST_BIT) /* We test if RST is contained in control field, ACK_BIT might also be contained. (Combinations can singal reasons of why RST was sent). */
                LOG_WARNING_RETURN(RECV_SEGMENT_RST_BIT, "Control-field: Received = `%s`; Expected = `%s`.",
                                   get_microtcp_control_to_string(control_segment->header.control), get_microtcp_control_to_string(_required_control));
        if ((_required_control & SYN_BIT) && !(control_segment->header.control & SYN_BIT))
                LOG_WARNING_RETURN(RECV_SEGMENT_NOT_SYN_BIT, "Control-field: Received = `%s`; Expected = `%s`.",
                                   get_microtcp_control_to_string(control_segment->header.control), get_microtcp_control_to_string(_required_control));
        if ((control_segment->header.control == (FIN_BIT | ACK_BIT)) && (_required_control == ACK_BIT))
                LOG_WARNING_RETURN(RECV_SEGMENT_UNEXPECTED_FINACK, "Control-field: Received = `%s`; Expected = `%s`.",
                                   get_microtcp_control_to_string(control_segment->header.control), get_microtcp_control_to_string(_required_control));
        if (control_segment->header.data_len != 0)
                LOG_WARNING_RETURN(RECV_SEGMENT_CARRIES_DATA, "Received segment %s contains %d bytes of payload.",
                                   get_microtcp_control_to_string(control_segment->header.control), control_segment->header.data_len);
        if (control_segment->header.control != _required_control)
                LOG_WARNING_RETURN(RECV_SEGMENT_ERROR, "Control-field: Received = `%s`; Expected = `%s`.",
                                   get_microtcp_control_to_string(control_segment->header.control), get_microtcp_control_to_string(_required_control));

        _socket->ack_number = get_most_recent_ack(_socket->ack_number, control_segment->header.seq_number + 1);
        LOG_INFO_RETURN(recvfrom_ret_val, "%s segment received.", get_microtcp_control_to_string(_required_control));
}

/**
 * @returns the number of bytes, it validly received.
 * This also implies that a packet was correctly received.
 */
static inline ssize_t send_control_segment(microtcp_sock_t *const _socket, const struct sockaddr *const _address,
                                           const socklen_t _address_len, uint16_t _required_control, mircotcp_state_t _required_state)
{
        /* Quick argument check. */
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(SEND_SEGMENT_FATAL_ERROR, _socket, _required_state);
        RETURN_ERROR_IF_SOCKADDR_INVALID(SEND_SEGMENT_FATAL_ERROR, _address);
        RETURN_ERROR_IF_SOCKET_ADDRESS_LENGTH_INVALID(SEND_SEGMENT_FATAL_ERROR, _address_len, sizeof(struct sockaddr));

        /* Create handshake segment. */
        microtcp_segment_t *control_segment = construct_microtcp_segment(_socket, _socket->seq_number, _required_control, (microtcp_payload_t){.raw_bytes = NULL, .size = 0});
        DEBUG_SMART_ASSERT(control_segment != NULL); /* If socket is properly initialized, assert should never fail. */

        /* Convert it to bytestream. */
        void *bytestream_buffer = serialize_microtcp_segment(_socket, control_segment);
        DEBUG_SMART_ASSERT(bytestream_buffer != NULL); /* If socket is properly initialized, assert should never fail. */

        /* Send handshake segment to server with UDP's sendto(). */
        const ssize_t segment_length = ((sizeof(control_segment->header)) + control_segment->header.data_len);
        const ssize_t sendto_ret_val = sendto(_socket->sd, bytestream_buffer, segment_length, NO_SENDTO_FLAGS, _address, _address_len);

        /* Log operation's outcome. */
        if (RARE_CASE(sendto_ret_val == SENDTO_ERROR))
                LOG_ERROR_RETURN(SEND_SEGMENT_FATAL_ERROR, "Sending %s segment failed. sendto() errno(%d):%s.",
                                 get_microtcp_state_to_string(_required_control), errno, strerror(errno));
        if (RARE_CASE(sendto_ret_val != segment_length))
                LOG_WARNING_RETURN(SEND_SEGMENT_ERROR, "Failed sending %s segment. sendto() sent %d bytes, but was asked to sent %d bytes",
                                   get_microtcp_state_to_string(_required_control), sendto_ret_val, segment_length);
        LOG_INFO_RETURN(sendto_ret_val, "%s segment sent.", get_microtcp_control_to_string(_required_control));
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

        /* All uontrol segments contain no data. */
        ssize_t expected_segment_size = sizeof(microtcp_header_t);
        void *const bytestream_buffer = _socket->bytestream_receive_buffer;

        ssize_t recvfrom_ret_val = recvfrom(_socket->sd, bytestream_buffer, MICROTCP_MTU, MSG_TRUNC, _address, &_address_len);
        if (recvfrom_ret_val == RECVFROM_ERROR && errno == EWOULDBLOCK)
                return RECV_SEGMENT_TIMEOUT;
        DEBUG_SMART_ASSERT(recvfrom_ret_val != RECVFROM_SHUTDOWN); /* Underlying protocol is UDP, this should be impossible. */
        if (RARE_CASE(recvfrom_ret_val == RECVFROM_ERROR))
                LOG_ERROR_RETURN(RECV_SEGMENT_FATAL_ERROR, "recvfrom returned %d, errno(%d):%s.", recvfrom_ret_val, errno, strerror(errno));
        if (recvfrom_ret_val != expected_segment_size)
                LOG_WARNING_RETURN(RECV_SEGMENT_ERROR, "Received bytestream size is (%zd bytes) different than %s (%zu bytes).",
                                   recvfrom_ret_val, STRINGIFY(expected_segment_size), expected_segment_size);
        if (!is_valid_microtcp_bytestream(bytestream_buffer, recvfrom_ret_val))
                LOG_WARNING_RETURN(RECV_SEGMENT_ERROR, "Received microtcp bytestream is corrupted.");

        extract_microtcp_segment(&(_socket->segment_receive_buffer), bytestream_buffer, recvfrom_ret_val);
        microtcp_segment_t *control_segment = _socket->segment_receive_buffer;
        if (RARE_CASE(control_segment == NULL))
                LOG_ERROR_RETURN(RECV_SEGMENT_FATAL_ERROR, "Extracting %s segment resulted to a NULL pointer.", get_microtcp_control_to_string(_required_control));
        if (control_segment->header.control & RST_BIT) /* We test if RST is contained in control field, ACK_BIT might also be contained. (Combinations can singal reasons of why RST was sent). */
                LOG_WARNING_RETURN(RECV_SEGMENT_RST_BIT, "Control-field: Received = `%s`; Expected = `%s`.",
                                   get_microtcp_control_to_string(control_segment->header.control), get_microtcp_control_to_string(_required_control));
        if ((_required_control & SYN_BIT) && !(control_segment->header.control & SYN_BIT))
                LOG_WARNING_RETURN(RECV_SEGMENT_NOT_SYN_BIT, "Control-field: Received = `%s`; Expected = `%s`.",
                                   get_microtcp_control_to_string(control_segment->header.control), get_microtcp_control_to_string(_required_control));
        if ((control_segment->header.control == (FIN_BIT | ACK_BIT)) && (_required_control == ACK_BIT))
                LOG_WARNING_RETURN(RECV_SEGMENT_UNEXPECTED_FINACK, "Control-field: Received = `%s`; Expected = `%s`.",
                                   get_microtcp_control_to_string(control_segment->header.control), get_microtcp_control_to_string(_required_control));
        if (control_segment->header.data_len != 0)
                LOG_WARNING_RETURN(RECV_SEGMENT_CARRIES_DATA, "Received segment %s contains %d bytes of payload.",
                                   get_microtcp_control_to_string(control_segment->header.control), control_segment->header.data_len);
        if (control_segment->header.control != _required_control)
                LOG_WARNING_RETURN(RECV_SEGMENT_ERROR, "Control-field: Received = `%s`; Expected = `%s`.",
                                   get_microtcp_control_to_string(control_segment->header.control), get_microtcp_control_to_string(_required_control));

        /* Ignore check if waiting to receive SYN (server side). */
        if (_required_control != SYN_BIT && control_segment->header.ack_number != _required_ack_number)
                LOG_ERROR_RETURN(RECV_SEGMENT_ERROR, "Received segment %s and ACK number mismatch occured. (Got = %d)|(Required = %d)",
                                 get_microtcp_control_to_string(_required_control), control_segment->header.ack_number, _socket->seq_number + 1);

        if (RARE_CASE(_required_control & SYN_BIT)) /* It is rare_case as it only happen in the beggining of the connection. */
                _socket->ack_number = control_segment->header.seq_number + SYN_SEQ_NUMBER_INCREMENT;
        else
                _socket->ack_number = get_most_recent_ack(_socket->ack_number, control_segment->header.seq_number + 1);

        _socket->peer_win_size = control_segment->header.window;
        LOG_INFO_RETURN(recvfrom_ret_val, "%s segment received.", get_microtcp_control_to_string(_required_control));
}

/* Function to determine if `_ack1` is newer than `_ack2` */
static __always_inline uint32_t get_most_recent_ack(const uint32_t _ack1, const uint32_t _ack2)
{
        return (int32_t)(_ack1 - _ack2) > 0 ? _ack1 : _ack2;
}
