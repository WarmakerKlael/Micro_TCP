#include "fsm/microtcp_fsm.h"
#include "fsm_common.h"
#include "microtcp_core.h"
#include "logging/microtcp_logger.h"

/* Very ealy version, made just for connection termination. */
typedef enum
{
        RECEIVING_SUBSTATE, /* Start state. */
        RECEIVED_FINACK_SUBSTATE,
        RECEIVING_REMAINING_PACKETS_SUBSTATE, /* Until not gap in received data. */
        RECEIVED_REMAINING_PACKETS_SUBSTATE,


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
        case SEND_SYN_STATE:             return STRINGIFY(SEND_SYN_STATE);
        case RECEIVED_FINACK_STATE:     return STRINGIFY(RECEIVED_FINACK_STATE);
        case EXIT_FAILURE_STATE:        return STRINGIFY(EXIT_FAILURE_STATE);
        default:                        return "??RECEIVE_STATE??";
        }
}
// clang-format on

static receive_internal_states execute_receiving_state(microtcp_sock_t *const _socket, struct sockaddr *const _address,
                                                     socklen_t _address_len, fsm_context_t *_context)
{
        // TBI
        return EXIT_FAILURE_STATE;

}

int receive_fsm(microtcp_sock_t *const _socket, struct sockaddr *_address, socklen_t _address_len)
{
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(MICROTCP_SHUTDOWN_FAILURE, _socket, ESTABLISHED);
        RETURN_ERROR_IF_SOCKADDR_INVALID(MICROTCP_SHUTDOWN_FAILURE, _address);
        RETURN_ERROR_IF_SOCKET_ADDRESS_LENGTH_INVALID(MICROTCP_SHUTDOWN_FAILURE, _address_len, sizeof(*_address));

        fsm_context_t context;
        receive_internal_states current_state = SEND_SYN_STATE;
        while (TRUE)
        {
                switch (current_state)
                {
                case SEND_SYN_STATE:
                case RECEIVED_FINACK_STATE:
                case EXIT_FAILURE_STATE:
                }
        }
}