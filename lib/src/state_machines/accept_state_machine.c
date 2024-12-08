#include "microtcp_core_utils.h"
#include "state_machines/state_machines.h"
#include "logging/logger.h"
typedef enum
{
        START_STATE,
        SYN_RECEIVED_STATE,
        SYNACK_SENT_STATE,
        ACK_RECEIVED_STATE,
        EXIT_FAILURE_STATE
} connect_internal_states;

typedef struct
{
        ssize_t recv_syn_ret_val;
        ssize_t send_synack_ret_val;
        ssize_t recv_ack_ret_val;
} state_machine_context_t;

// clang-format off
static const char *get_connect_state_to_string(connect_internal_states _state)
{
        switch (_state)
        {
        case START_STATE:               return STRINGIFY(START_STATE);
        case SYN_RECEIVED_STATE:        return STRINGIFY(SYN_RECEIVED_STATE);
        case SYNACK_SENT_STATE:         return STRINGIFY(SYNACK_SENT_STATE);
        case ACK_RECEIVED_STATE:        return STRINGIFY(ACK_RECEIVED_STATE);
        case EXIT_FAILURE_STATE:        return STRINGIFY(EXIT_FAILURE_STATE);
        default:                        return "??ACCEPT_STATE??";
        }
}
// clang-format on