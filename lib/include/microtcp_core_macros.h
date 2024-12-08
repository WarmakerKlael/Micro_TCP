#ifndef MICROTCP_CORE_MACROS_H
#define MICROTCP_CORE_MACROS_H

#include "logging/logger.h"
#include "microtcp_common_macros.h"
#include "microtcp_helper_functions.h"

/* Internal MACRO defines. */
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

/* Error values returned internally to microtcp_accept(), if any of its stages fail. */
#define MICROTCP_RECV_SYN_TIMEOUT 0
#define MICROTCP_RECV_SYN_ERROR -1
#define MICROTCP_RECV_SYN_FATAL_ERROR -2

/* Directly used in: microtcp_socket() */
#define FUNCTION_MICROTCP_SOCKET_PARAMETER_IS_VALID(_given_parameter,               \
                                                    _expected_parameter)            \
        ((_given_parameter == _expected_parameter)                                  \
             ? TRUE                                                                 \
             : (LOG_ERROR("Only %s %d (%s) is supported for socket; %d was given.", \
                          #_given_parameter,                                        \
                          _expected_parameter,                                      \
                          #_expected_parameter,                                     \
                          _given_parameter),                                        \
                FALSE))

/* Directly used in: microtcp_bind() & microtcp_connect() &  create_microtcp_segment() */
#define RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(_failure_return_value, _socket, _required_state)                                       \
        do                                                                                                                             \
        {                                                                                                                              \
                if (_socket == NULL)                                                                                                   \
                        LOG_ERROR_RETURN(_failure_return_value, "NULL pointer passed to variable '%s'.", STRINGIFY(_socket));          \
                                                                                                                                       \
                if (_required_state != ANY && _socket->state != _required_state)                                                       \
                        LOG_ERROR_RETURN(_failure_return_value, "Expected socket in %s state; (state = %s).",                          \
                                         get_microtcp_state_to_string(_required_state), get_microtcp_state_to_string(_socket->state)); \
                                                                                                                                       \
                if (_socket->sd < 0)                                                                                                   \
                        LOG_ERROR_RETURN(_failure_return_value, "Invalid MicroTCP socket descriptor; (sd = %d).", _socket->sd);        \
        } while (0)

/* Directly used in: microtcp_bind() & microtcp_connect() */
#define RETURN_ERROR_IF_SOCKADDR_INVALID(_failure_return_value, _address)                                                      \
        do                                                                                                                     \
        {                                                                                                                      \
                if (_address == NULL)                                                                                          \
                        LOG_ERROR_RETURN(_failure_return_value, "NULL pointer passed to variable '%s'.", STRINGIFY(_address)); \
        } while (0)

/* Directly used in: microtcp_bind() & microtcp_connect() */
#define RETURN_ERROR_IF_SOCKET_ADDRESS_LENGTH_INVALID(_failure_return_value, _address_len, _expected_address_len)                                        \
        do                                                                                                                                               \
        {                                                                                                                                                \
                if (_address_len != _expected_address_len)                                                                                               \
                        LOG_ERROR_RETURN(_failure_return_value, "Address length mismatch: (got %d, expected %zu)", _address_len, _expected_address_len); \
        } while (0)

/* Directly used in: create_microtcp_segment() */
#define RETURN_ERROR_IF_MICROTCP_PAYLOAD_INVALID(_failure_return_value, _microtcp_payload)                                              \
        do                                                                                                                              \
        {                                                                                                                               \
                if (_microtcp_payload.size > MICROTCP_MSS)                                                                              \
                        LOG_ERROR_RETURN(_failure_return_value, "Tried to create %s with %d bytes of payload; Limit is %s = %d",        \
                                         STRINGIFY(microtcp_segment_t), _microtcp_payload.size, STRINGIFY(MICROTCP_MSS), MICROTCP_MSS); \
                                                                                                                                        \
                if ((_microtcp_payload.size == 0) != (_microtcp_payload.raw_bytes == NULL))                                             \
                        LOG_ERROR_RETURN(_failure_return_value, "Payload field mismatch: size is %d, but raw_bytes is %s.",             \
                                         _microtcp_payload.size, (_microtcp_payload.raw_bytes ? "not NULL" : "NULL"));                  \
        } while (0);

#endif /* MICROTCP_CORE_MACROS_H */