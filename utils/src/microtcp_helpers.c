#include "microtcp_helpers.h"
#include "microtcp_macro_functions.h"
// clang-format off
const char *get_microtcp_state_to_string(mircotcp_state_t _state)
{
        switch (_state)
        {
        case INVALID:           return STRINGIFY(INVALID);
        case CLOSED:            return STRINGIFY(CLOSED);
        case BOUND:             return STRINGIFY(BOUND);
        case LISTEN:            return STRINGIFY(LISTEN);
        case ESTABLISHED:       return STRINGIFY(ESTABLISHED);
        case CLOSING_BY_PEER:   return STRINGIFY(CLOSING_BY_PEER);
        case CLOSING_BY_HOST:   return STRINGIFY(CLOSING_BY_HOST);
        default:                return "??MICROTCP_STATE??";
        }
}
//clang-format on