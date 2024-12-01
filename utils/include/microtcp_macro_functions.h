#ifndef MICROTCP_MACRO_FUNCTIONS_H
#define MICROTCP_MACRO_FUNCTIONS_H

#include <string.h>

#include "logging/logger.h"
#include "microtcp_defines.h"

#define STRINGIFY(_var_name) #_var_name
#define __FILENAME__ (strstr(__FILE__, PROJECT_NAME) ? strstr(__FILE__, PROJECT_NAME) : __FILE__)

/* #define CHECK_MICROTCP_SOCKET_PARAMETERS_OLD(_new_socket_name,                                    \
                                         _given_parameter,                                    \
                                         _expected_parameter)                                 \
        do                                                                                    \
        {                                                                                     \
                if ((_given_parameter) != (_expected_parameter))                              \
                {                                                                             \
                        PRINT_ERROR("Only %s %d (%s) is supported for socket; %d was given.", \
                                    #_given_parameter,                                        \
                                    _expected_parameter,                                      \
                                    #_expected_parameter,                                     \
                                    _given_parameter);                                        \
                }                                                                             \
        } while (0) */

#define MICROTCP_SOCKET_PARAMETER_IS_VALID(_given_parameter,                            \
                                         _expected_parameter)                         \
        ((_given_parameter == _expected_parameter)                                      \
             ? TRUE                                                                   \
             : (PRINT_ERROR("Only %s %d (%s) is supported for socket; %d was given.", \
                            #_given_parameter,                                        \
                            _expected_parameter,                                      \
                            #_expected_parameter,                                     \
                            _given_parameter),                                        \
                FALSE))

#endif /* MICROTCP_MACRO_FUNCTIONS_H */