#ifndef MICROTCP_HELPER_FUNCTIONS_H
#define MICROTCP_HELPER_FUNCTIONS_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h> // for time_t
#include "microtcp.h"
struct timeval;

/**
 * @brief  Maps a `microtcp_state_t` value to its corresponding string.
 *
 * @param _state The microTCP state to convert. Valid values: `INVALID`,
 * `CLOSED`, `LISTEN`, `ESTABLISHED`, `CLOSING_BY_PEER`,
 * `CLOSING_BY_HOST`.
 *
 * @return A string representing the given state. If `_state` is unrecognized,
 * returns `"??MICROTCP_STATE??"`.
 */
const char *get_microtcp_state_to_string(microtcp_state_t _state);

/**
 * @brief Converts a microTCP control flag(s) field to its string representation.
 *
 * @param _control A bitmask of control flags to convert. Recognized flags:
 *                 `SYN_BIT`, `FIN_BIT`, `RST_BIT`, `ACK_BIT`.
 *
 * @return A string representing the active control flags, separated by `|`.
 *         If no flags are set, returns `"??NO_CONTROL??"`.
 */
const char *get_microtcp_control_to_string(uint16_t _control);

void normalize_timeval(struct timeval *_tv);

void subtract_timeval(struct timeval *_subtrahend, struct timeval _minuend);
time_t timeval_to_us(struct timeval _timeval);
struct timeval us_to_timeval(time_t _us);
time_t elapsed_time_us(struct timeval _start_time);
struct timeval get_current_timeval(void);

#endif /* MICROTCP_HELPER_FUNCTIONS_H */
