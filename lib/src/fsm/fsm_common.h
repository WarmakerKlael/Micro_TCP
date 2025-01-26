#ifndef STATE_MACHINES_COMMON_H
#define STATE_MACHINES_COMMON_H

#include "logging/microtcp_logger.h"

#define FSM_DEFAULT_CASE_HANDLER(_convert_substate_to_string_func, _current_substate, _next_substate) \
        do                                                                                            \
        {                                                                                             \
                LOG_ERROR("%s() entered an `undefined` substate = %s",                                \
                          __func__, (_convert_substate_to_string_func)((_current_substate)));         \
                (_current_substate) = _next_substate;                                                 \
        } while (0)

#endif /* STATE_MACHINES_COMMON_H */
