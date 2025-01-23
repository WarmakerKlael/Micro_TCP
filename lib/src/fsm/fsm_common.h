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

#include <time.h> // for time_t
struct timeval;

void subtract_timeval(struct timeval *_subtrahend, struct timeval _minuend);
time_t timeval_to_us(struct timeval _timeval);
struct timeval us_to_timeval(time_t _us);
time_t elapsed_time_us(struct timeval _start_time);

#endif /* STATE_MACHINES_COMMON_H */
