#include <string.h>
#include "microtcp_common_macros.h"
#include "microtcp_helper_functions.h"
// clang-format off
const char *get_microtcp_state_to_string(mircotcp_state_t _state)
{
        switch (_state)
        {
        case INVALID:           return STRINGIFY(INVALID);
        case CLOSED:            return STRINGIFY(CLOSED);
        case LISTEN:            return STRINGIFY(LISTEN);
        case ESTABLISHED:       return STRINGIFY(ESTABLISHED);
        case CLOSING_BY_PEER:   return STRINGIFY(CLOSING_BY_PEER);
        case CLOSING_BY_HOST:   return STRINGIFY(CLOSING_BY_HOST);
        default:                return "??MICROTCP_STATE??";
        }
}

const char *get_microtcp_control_to_string(uint16_t _control)
{
        enum constants
        {
                BITS_PER_BYTE = 8,
                CONTROL_BITS = sizeof(uint16_t) * BITS_PER_BYTE,
                MAX_STRING_SIZE_PER_CONTROL_BIT = 4, /* 3 letters + 1 separator (|). */
                STRING_BUFFER_SIZE = CONTROL_BITS * MAX_STRING_SIZE_PER_CONTROL_BIT
        };

        static char string_buffer[STRING_BUFFER_SIZE] = {0};
        memset(string_buffer, 0, STRING_BUFFER_SIZE);

        if (_control & SYN_BIT) strcat(string_buffer, "SYN|");
        if (_control & FIN_BIT) strcat(string_buffer, "FIN|");
        if (_control & RST_BIT) strcat(string_buffer, "RST|");
        if (_control & ACK_BIT) strcat(string_buffer, "ACK|");

        size_t string_length = strlen(string_buffer);
        if (string_length > 0) string_buffer[string_length - 1] = '\0'; /* Remove trailing | */

        return string_length > 0 ? string_buffer : "??NO_CONTROL??";
}
// clang-format on