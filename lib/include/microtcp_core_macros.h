#ifndef MICROTCP_CORE_MACROS_H
#define MICROTCP_CORE_MACROS_H

#include "logging/microtcp_logger.h"
#include "microtcp_helper_macros.h"
#include "microtcp_helper_functions.h"
#include "smart_assert.h"

/* Directly used in: microtcp_socket() */
#define RETURN_ERROR_IF_FUNCTION_PARAMETER_MICROTCP_SOCKET_INVALID(_failure_return_value,               \
                                                                   _given_parameter,                    \
                                                                   _expected_parameter)                 \
        do                                                                                              \
        {                                                                                               \
                if (_given_parameter == _expected_parameter)                                            \
                        break;                                                                          \
                LOG_ERROR("Failing to proceed. Only %s %d (%s) is supported for socket; %d was given.", \
                          #_given_parameter,                                                            \
                          (_expected_parameter),                                                        \
                          #_expected_parameter,                                                         \
                          (_given_parameter));                                                          \
                return (_failure_return_value);                                                         \
        } while (0)

/* Directly used in: microtcp_bind() & microtcp_connect() &  construct_microtcp_segment() */
#define RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(_failure_return_value, _socket, _allowed_states)                                                \
        do                                                                                                                                      \
        {                                                                                                                                       \
                SMART_ASSERT((_socket) != NULL);                                                                                                \
                                                                                                                                                \
                if (!((_socket)->state & (_allowed_states)))                                                                                    \
                {                                                                                                                               \
                        /* Separated these... because they cause problems with static buffer. */                                                \
                        LOG_ERROR("Expected socket in state(s): %s.", get_microtcp_state_to_string(_allowed_states));                           \
                        LOG_ERROR_RETURN((_failure_return_value), "Current socket->state = %s.", get_microtcp_state_to_string(_socket->state)); \
                }                                                                                                                               \
                                                                                                                                                \
                if ((_socket)->sd < 0)                                                                                                          \
                        LOG_ERROR_RETURN((_failure_return_value), "Invalid MicroTCP socket descriptor; (sd = %d).", (_socket)->sd);             \
        } while (0)

/* Directly used in: microtcp_bind() & microtcp_connect() */
#define RETURN_ERROR_IF_SOCKADDR_INVALID(_failure_return_value, _address) \
        do                                                                \
        {                                                                 \
                SMART_ASSERT((_address) != NULL);                         \
        } while (0)

/* Directly used in: microtcp_bind() & microtcp_connect() */
#define RETURN_ERROR_IF_SOCKET_ADDRESS_LENGTH_INVALID(_failure_return_value, _address_len, _expected_address_len)                                              \
        do                                                                                                                                                     \
        {                                                                                                                                                      \
                if ((_address_len) != (_expected_address_len))                                                                                                 \
                        LOG_ERROR_RETURN((_failure_return_value), "Address length mismatch: (got %d, expected %zu)", (_address_len), (_expected_address_len)); \
        } while (0)

/* Directly used in: construct_microtcp_segment() */
#define RETURN_ERROR_IF_MICROTCP_PAYLOAD_INVALID(_failure_return_value, _microtcp_payload)                                               \
        do                                                                                                                               \
        {                                                                                                                                \
                if ((_microtcp_payload).size + sizeof(microtcp_header_t) > MICROTCP_MSS)                                                 \
                        LOG_ERROR_RETURN((_failure_return_value), "Tried to create %s with %d bytes of payload; Limit is %s = %d",       \
                                         STRINGIFY(microtcp_segment_t), (_microtcp_payload).size,                                        \
                                         STRINGIFY(MICROTCP_MSS - sizeof(microtcp_header_t)), MICROTCP_MSS - sizeof(microtcp_header_t)); \
                                                                                                                                         \
                if ((_microtcp_payload.size == 0) != ((_microtcp_payload).raw_bytes == NULL))                                            \
                        LOG_ERROR_RETURN((_failure_return_value), "Payload field mismatch: size is %d, but raw_bytes is %s.",            \
                                         (_microtcp_payload).size, ((_microtcp_payload).raw_bytes ? "not NULL" : "NULL"));               \
        } while (0);

#endif /* MICROTCP_CORE_MACROS_H */