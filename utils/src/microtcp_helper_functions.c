#include <string.h>
#include "microtcp_common_macros.h"
#include "microtcp_helper_functions.h"

// clang-format off
const char *get_microtcp_state_to_string(mircotcp_state_t _state)
{
#define STATE_SEPARATOR "|"
        enum constants
        {
                SEPARATOR_SIZE = 2,
                TERM_NULL_SIZE = 1,
                STRING_BUFFER_SIZE = sizeof(STRINGIFY(INVALID)) - TERM_NULL_SIZE + SEPARATOR_SIZE +
                                     sizeof(STRINGIFY(CLOSED)) - TERM_NULL_SIZE + SEPARATOR_SIZE +
                                     sizeof(STRINGIFY(LISTEN)) - TERM_NULL_SIZE + SEPARATOR_SIZE +
                                     sizeof(STRINGIFY(ESTABLISHED)) - TERM_NULL_SIZE + SEPARATOR_SIZE +
                                     sizeof(STRINGIFY(CLOSING_BY_HOST)) - TERM_NULL_SIZE + SEPARATOR_SIZE +
                                     sizeof(STRINGIFY(CLOSING_BY_PEER)) + SEPARATOR_SIZE
        };

        static char string_buffer[STRING_BUFFER_SIZE] = {0};
        memset(string_buffer, 0, STRING_BUFFER_SIZE);

        if (_state & INVALID)           strcat(string_buffer, STRINGIFY(INVALID) STATE_SEPARATOR);
        if (_state & CLOSED)            strcat(string_buffer, STRINGIFY(CLOSED) STATE_SEPARATOR);
        if (_state & LISTEN)            strcat(string_buffer, STRINGIFY(LISTEN) STATE_SEPARATOR);
        if (_state & ESTABLISHED)       strcat(string_buffer, STRINGIFY(ESTABLISHED) STATE_SEPARATOR);
        if (_state & CLOSING_BY_HOST)   strcat(string_buffer, STRINGIFY(CLOSING_BY_HOST) STATE_SEPARATOR);
        if (_state & CLOSING_BY_PEER)   strcat(string_buffer, STRINGIFY(CLOSING_BY_PEER) STATE_SEPARATOR);

        size_t string_length = strlen(string_buffer);
        if (string_length > 0) string_buffer[string_length - 1] = '\0'; /* Remove trailing - */

        return string_length > 0 ? string_buffer : "??MICROTCP_STATE??";
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