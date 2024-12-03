#ifndef MICROTCP_MACRO_FUNCTIONS_H
#define MICROTCP_MACRO_FUNCTIONS_H

#include <string.h>

#include "logging/logger.h"
#include "microtcp_defines.h"

#define STRINGIFY(_var_name) #_var_name
#define __FILENAME__ (strstr(__FILE__, PROJECT_NAME) ? strstr(__FILE__, PROJECT_NAME) : __FILE__)

#define FUNCTION_MICROTCP_SOCKET_PARAMETER_IS_VALID(_given_parameter,              \
                                                    _expected_parameter)           \
     ((_given_parameter == _expected_parameter)                                    \
          ? TRUE                                                                   \
          : (PRINT_ERROR("Only %s %d (%s) is supported for socket; %d was given.", \
                         #_given_parameter,                                        \
                         _expected_parameter,                                      \
                         #_expected_parameter,                                     \
                         _given_parameter),                                        \
             FALSE))

#define RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(_failure_return_value, _socket, _required_state)                                \
     do                                                                                                                         \
     {                                                                                                                          \
          if (_socket == NULL)                                                                                                  \
               PRINT_ERROR_RETURN(_failure_return_value, "NULL pointer passed to variable '%s'.", STRINGIFY(_socket));          \
          if (_socket->state != _required_state)                                                                                \
               PRINT_ERROR_RETURN(_failure_return_value, "Expected socket in %s state; (state = %s).",                          \
                                  get_microtcp_state_to_string(_required_state), get_microtcp_state_to_string(_socket->state)); \
          if (_socket->sd < 0)                                                                                                  \
               PRINT_ERROR_RETURN(_failure_return_value, "Invalid MicroTCP socket descriptor; (sd = %d).", _socket->sd);        \
     } while (0)

#define RETURN_ERROR_IF_SOCKADDR_INVALID(_failure_return_value, _address)                                               \
     do                                                                                                                 \
     {                                                                                                                  \
          if (_address == NULL)                                                                                         \
               PRINT_ERROR_RETURN(_failure_return_value, "NULL pointer passed to variable '%s'.", STRINGIFY(_address)); \
     } while (0)

#define RETURN_ERROR_IF_SOCKET_ADDRESS_LENGTH_INVALID(_failure_return_value, _address_len, _expected_address_len)                                 \
     do                                                                                                                                           \
     {                                                                                                                                            \
          if (_address_len != _expected_address_len)                                                                                              \
               PRINT_ERROR_RETURN(_failure_return_value, "Address length mismatch: (got %d, expected %zu)", _address_len, _expected_address_len); \
     } while (0)

#endif /* MICROTCP_MACRO_FUNCTIONS_H */