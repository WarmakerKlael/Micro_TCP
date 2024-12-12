#ifndef MICROTCP_HELPER_FUNCTIONS_H
#define MICROTCP_HELPER_FUNCTIONS_H

#include "microtcp.h"

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
const char *get_microtcp_state_to_string(mircotcp_state_t _state);

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

#endif /* MICROTCP_HELPER_FUNCTIONS_H */