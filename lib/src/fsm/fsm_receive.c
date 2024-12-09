#include "state_machines/state_machines.h"
#include "state_machines_common.h"
#include "microtcp_core.h"
#include "logging/logger.h"

/* Very ealy version, made just for connection termination. */
typedef enum
{
        INITIAL_STATE, /* Start state. */
        RECEIVED_FINACK_STATE,
        /* RECEIVED_RST, */
        /* RECEIVED_SYN, */
        EXIT_FAILURE_STATE
} receive_internal_states;

typedef struct
{
} fsm_context_t;

// clang-format off
static const char *convert_state_to_string(receive_internal_states _state)
{
        switch (_state)
        {
        case INITIAL_STATE:             return STRINGIFY(INITIAL_STATE);
        case RECEIVED_FINACK_STATE:     return STRINGIFY(RECEIVED_FINACK_STATE);
        case EXIT_FAILURE_STATE:        return STRINGIFY(EXIT_FAILURE_STATE);
        default:                        return "??RECEIVE_STATE??";
        }
}
// clang-format on

static receive_internal_states execute_initial_state(microtcp_sock_t *const _socket, struct sockaddr *const _address,
                                                     socklen_t _address_len, fsm_context_t *_context)
{

        ;
}

int receive_fsm(microtcp_sock_t *const _socket, struct sockaddr *_address, socklen_t _address_len)
{
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(MICROTCP_SHUTDOWN_FAILURE, _socket, ESTABLISHED);
        RETURN_ERROR_IF_SOCKADDR_INVALID(MICROTCP_SHUTDOWN_FAILURE, _address);
        RETURN_ERROR_IF_SOCKET_ADDRESS_LENGTH_INVALID(MICROTCP_SHUTDOWN_FAILURE, _address_len, sizeof(*_address));

        fsm_context_t context = {0};
        receive_internal_states current_state = INITIAL_STATE;
        while (TRUE)
        {
                switch (current_state)
                {
                case INITIAL_STATE:
                case RECEIVED_FINACK_STATE:
                case EXIT_FAILURE_STATE:
                }
        }
}