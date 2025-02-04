#include "fsm/microtcp_fsm.h"
#include <stddef.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include "core/segment_io.h"
#include "core/misc.h"
#include "core/resource_allocation.h"
#include "core/socket_stats_updater.h"
#include "core/segment_processing.h"
#include "fsm_common.h"
#include "logging/microtcp_fsm_logger.h"
#include "logging/microtcp_logger.h"
#include "microtcp.h"
#include "microtcp_core_macros.h"
#include "microtcp_defines.h"
#include "microtcp_helper_macros.h"
#include "settings/microtcp_settings.h"

typedef enum
{
        CONNECTION_ESTABLISHED_SUBSTATE = ESTABLISHED, /* Initial substate. FSM entry point. */
        FIN_WAIT_1_SUBSTATE,
        FIN_DOUBLE_SUBSTATE,
        FIN_WAIT_2_RECV_SUBSTATE, /* Sub state. */
        FIN_WAIT_2_SEND_SUBSTATE, /* Sub state. */
        TIME_WAIT_SUBSTATE,
        CLOSED_1_SUBSTATE,
        CLOSED_2_SUBSTATE = CLOSED, /* Terminal substate(success). FSM exit point. */
        EXIT_FAILURE_SUBSTATE = -1  /* Terminal substate (failure). FSM exit point. */
} shutdown_active_fsm_substates_t;

typedef enum
{
        NO_ERROR,
        PEER_FINACK_RETRIES_EXHAUSTED, /* Sent FIN to peer, ack never received. */
        PEER_FIN_TIMEOUT,              /* Peer sent ACK, never sent FIN. */
        DOUBLE_FIN,
        RST_EXPECTED_ACK,          /* RST after host sent FIN|ACK and expected peer's ACK. */
        RST_EXPECTED_FINACK,       /* RST after host received peer's ACK and expected peer's FIN|ACK. */
        RST_EXPECTED_TIMEOUT,      /* RST after host send ACK, and expected TIME_WAIT timer to run off . */
        FATAL_ERROR,               /* Set when fatal errors occur on host's side. */
        SOCKET_TIMEOUT_SETUP_ERROR /* Set when host is unable to set socket's timer for the TIME_WAIT period. */
} shutdown_active_fsm_errno_t;

typedef struct
{
        ssize_t send_finack_ret_val;
        ssize_t recv_ack_ret_val;
        ssize_t recv_finack_ret_val;
        ssize_t send_ack_ret_val;

        ssize_t finack_retries_counter;
        struct timeval finack_wait_time_timer;
        struct timeval recvfrom_timeout;
        shutdown_active_fsm_errno_t errno;
} fsm_context_t;

/* ----------------------------------------- LOCAL HELPER FUNCTIONS ----------------------------------------- */
static inline shutdown_active_fsm_substates_t handle_fatal_error(fsm_context_t *_context);
static const char *convert_substate_to_string(shutdown_active_fsm_substates_t _state);
static void log_errno_status(shutdown_active_fsm_errno_t _errno);

static shutdown_active_fsm_substates_t execute_connection_established_substate(microtcp_sock_t *const _socket, struct sockaddr *const _address,
                                                                               socklen_t _address_len, fsm_context_t *_context)
{
        _context->send_finack_ret_val = send_finack_control_segment(_socket, _address, _address_len);
        switch (_context->send_finack_ret_val)
        {
        case SEND_SEGMENT_FATAL_ERROR:
                return handle_fatal_error(_context);

        case SEND_SEGMENT_ERROR:
                return CONNECTION_ESTABLISHED_SUBSTATE;

        default:
                return FIN_WAIT_1_SUBSTATE;
        }
}

/**
 * @brief Purpose of macro is to remove boiler-plate code from
 * function fsm's substate functions.
 */
#define RETURN_EXIT_FAILURE_SUBSTATE_IF_FINACK_RETRIES_EXHAUSTED(_context)                                         \
        do                                                                                                         \
        {                                                                                                          \
                if ((_context)->finack_retries_counter == 0) /* Exhausted attempts to resend FIN|ACK. */           \
                {                                                                                                  \
                        (_context)->errno = PEER_FINACK_RETRIES_EXHAUSTED;                                         \
                        return EXIT_FAILURE_SUBSTATE;                                                              \
                }                                                                                                  \
                (_context)->finack_retries_counter--; /* Decrement retry counter and attempt to resend FIN|ACK. */ \
        } while (0)

static shutdown_active_fsm_substates_t execute_fin_wait_1_substate(microtcp_sock_t *const _socket, struct sockaddr *const _address,
                                                                   socklen_t _address_len, fsm_context_t *_context)
{
        const uint32_t required_ack_number = _socket->seq_number + FIN_SEQ_NUMBER_INCREMENT;
        _context->recv_ack_ret_val = receive_ack_control_segment(_socket, _address, _address_len, required_ack_number);
        switch (_context->recv_ack_ret_val)
        {
        case RECV_SEGMENT_FATAL_ERROR:
                return handle_fatal_error(_context);
        /* Actions on the following two cases are the same. */
        case RECV_SEGMENT_FINACK_UNEXPECTED:
                return FIN_DOUBLE_SUBSTATE;
        case RECV_SEGMENT_CARRIES_DATA:
                return FIN_WAIT_1_SUBSTATE;
        case RECV_SEGMENT_ERROR:
        case RECV_SEGMENT_TIMEOUT:
                update_socket_lost_counters(_socket, _context->send_finack_ret_val);
                RETURN_EXIT_FAILURE_SUBSTATE_IF_FINACK_RETRIES_EXHAUSTED(_context);
                return CONNECTION_ESTABLISHED_SUBSTATE;
        case RECV_SEGMENT_RST_RECEIVED:
                _context->errno = RST_EXPECTED_ACK;
                return CLOSED_1_SUBSTATE;
        default:
                _socket->seq_number = required_ack_number;
                return FIN_WAIT_2_RECV_SUBSTATE;
        }
}

#define IS_VALID_CNTRL_TYPE(_cntrl_type)                                                                            \
        ((sizeof(#_cntrl_type) == sizeof("ack") &&                                                                  \
          #_cntrl_type[0] == 'a' && #_cntrl_type[1] == 'c' && #_cntrl_type[2] == 'k' && #_cntrl_type[3] == '\0') || \
         (sizeof(#_cntrl_type) == sizeof("finack") &&                                                               \
          #_cntrl_type[0] == 'f' && #_cntrl_type[1] == 'i' && #_cntrl_type[2] == 'n' && #_cntrl_type[3] == 'a' &&   \
          #_cntrl_type[4] == 'c' && #_cntrl_type[5] == 'k' && #_cntrl_type[6] == '\0'))

/**
 * @brief Purpose of macro is to remove boiler-plate code from
 * function `execute_fin_double_substate`.
 * @note Support only sending ack and finack.
 */
#define TRY_SEND_CTRL_SEG_OR_RETURN_SUBSTATE(_socket, _address, _address_len, _context, _cntrl_type)                                    \
        do                                                                                                                              \
        {                                                                                                                               \
                /* _Static_assert(IS_VALID_CNTRL_TYPE(_cntrl_type), "Invalid _cntrl_type: must be 'ack' or 'finack'"); */               \
                                                                                                                                        \
                (_context)->send_##_cntrl_type##_ret_val = send_##_cntrl_type##_control_segment((_socket), (_address), (_address_len)); \
                                                                                                                                        \
                if ((_context)->send_##_cntrl_type##_ret_val == SEND_SEGMENT_FATAL_ERROR)                                               \
                        return handle_fatal_error(_context);                                                                            \
                if ((_context)->send_##_cntrl_type##_ret_val == SEND_SEGMENT_ERROR)                                                     \
                        return FIN_DOUBLE_SUBSTATE;                                                                                     \
                                                                                                                                        \
        } while (0)

static shutdown_active_fsm_substates_t execute_fin_double_substate(microtcp_sock_t *const _socket, struct sockaddr *const _address,
                                                                   socklen_t _address_len, fsm_context_t *_context)
{
        _context->errno = DOUBLE_FIN;
        /* TODO: check if FINACK is with correct seq and ACK. (if it is SYNCED). */
        _socket->ack_number = _socket->segment_receive_buffer->header.seq_number + FIN_SEQ_NUMBER_INCREMENT;
        _socket->peer_win_size = _socket->segment_build_buffer->header.window;

        TRY_SEND_CTRL_SEG_OR_RETURN_SUBSTATE(_socket, _address, _address_len, _context, ack);
        const uint32_t required_ack_number = _socket->seq_number + SYN_SEQ_NUMBER_INCREMENT;

        _context->recv_ack_ret_val = receive_ack_control_segment(_socket, _address, _address_len, required_ack_number);

        switch (_context->recv_ack_ret_val)
        {
        case RECV_SEGMENT_FATAL_ERROR:
                return handle_fatal_error(_context);
        case RECV_SEGMENT_CARRIES_DATA:
                return FIN_DOUBLE_SUBSTATE;
        case RECV_SEGMENT_FINACK_UNEXPECTED:
                update_socket_lost_counters(_socket, _context->send_ack_ret_val);
                TRY_SEND_CTRL_SEG_OR_RETURN_SUBSTATE(_socket, _address, _address_len, _context, ack);
                return FIN_DOUBLE_SUBSTATE;
        case RECV_SEGMENT_ERROR:
        case RECV_SEGMENT_TIMEOUT:
                update_socket_lost_counters(_socket, _context->send_finack_ret_val);
                RETURN_EXIT_FAILURE_SUBSTATE_IF_FINACK_RETRIES_EXHAUSTED(_context);
                TRY_SEND_CTRL_SEG_OR_RETURN_SUBSTATE(_socket, _address, _address_len, _context, finack);
                return FIN_DOUBLE_SUBSTATE;
        case RECV_SEGMENT_RST_RECEIVED:
                _context->errno = RST_EXPECTED_ACK;
                return CLOSED_1_SUBSTATE;
        default: /* Received ACK for our FIN|ACK. */
                _socket->seq_number = required_ack_number;
                return TIME_WAIT_SUBSTATE;
        }
}

#undef TRY_SEND_CTRL_SEG_OR_RETURN_SUBSTATE
#undef RETURN_EXIT_FAILURE_SUBSTATE_IF_FINACK_RETRIES_EXHAUSTED

static shutdown_active_fsm_substates_t execute_fin_wait_2_recv_substate(microtcp_sock_t *const _socket, struct sockaddr *const _address,
                                                                        socklen_t _address_len, fsm_context_t *_context)
{
        _context->recv_finack_ret_val = receive_finack_control_segment(_socket, _address, _address_len, _socket->seq_number);
        switch (_context->recv_finack_ret_val)
        {
        case RECV_SEGMENT_FATAL_ERROR:
                return handle_fatal_error(_context);
        case RECV_SEGMENT_CARRIES_DATA:
                return FIN_WAIT_2_RECV_SUBSTATE;

        /* Actions on the following two cases are the same. */
        case RECV_SEGMENT_ERROR:
        case RECV_SEGMENT_TIMEOUT:
                if (timeval_to_usec(_context->finack_wait_time_timer) > 0) /* Retry receiving FIN|ACK until the timeout expires. */
                {
                        subtract_timeval(&(_context->finack_wait_time_timer), _context->recvfrom_timeout);
                        return FIN_WAIT_2_RECV_SUBSTATE;
                }
                send_rstack_control_segment(_socket, _address, _address_len); /* Final attempt to notify peer for closure. */
                _context->errno = PEER_FIN_TIMEOUT;
                return CLOSED_1_SUBSTATE;

        case RECV_SEGMENT_RST_RECEIVED:
                _context->errno = RST_EXPECTED_FINACK;
                return CLOSED_1_SUBSTATE;

        default:
                _socket->ack_number = _socket->segment_receive_buffer->header.seq_number + FIN_SEQ_NUMBER_INCREMENT;
                return FIN_WAIT_2_SEND_SUBSTATE;
        }
}

static shutdown_active_fsm_substates_t execute_fin_wait_2_send_substate(microtcp_sock_t *const _socket, struct sockaddr *const _address,
                                                                        socklen_t _address_len, fsm_context_t *_context)
{
        _context->send_ack_ret_val = send_ack_control_segment(_socket, _address, _address_len);
        switch (_context->send_ack_ret_val)
        {
        case SEND_SEGMENT_FATAL_ERROR:
                return handle_fatal_error(_context);

        case SEND_SEGMENT_ERROR:
                return FIN_WAIT_2_SEND_SUBSTATE; /* Retry sending ACK. Peer re-sending FIN-ACK is acceptable until ACK succeeds. */

        default:
                return TIME_WAIT_SUBSTATE;
        }
}

static shutdown_active_fsm_substates_t execute_time_wait_substate(microtcp_sock_t *const _socket, struct sockaddr *const _address,
                                                                  socklen_t _address_len, fsm_context_t *_context)
{
        time_t timewait_period_us = timeval_to_usec(get_shutdown_time_wait_period());
        struct timeval starting_time;
        gettimeofday(&starting_time, NULL);

        while (elapsed_time_usec(starting_time) < timewait_period_us)
        {
                _context->recv_finack_ret_val = receive_finack_control_segment(_socket, _address, _address_len, _socket->seq_number);
                switch (_context->recv_finack_ret_val)
                {
                case RECV_SEGMENT_FATAL_ERROR:
                        return handle_fatal_error(_context);

                case RECV_SEGMENT_RST_RECEIVED:
                        _context->errno = RST_EXPECTED_TIMEOUT;
                        return CLOSED_1_SUBSTATE;

                case RECV_SEGMENT_TIMEOUT: /* Timeout: Do nothing; wait for the next segment or timer expiration. */
                case RECV_SEGMENT_ERROR:   /* Heard junk, ignore. If peer sent an ack and was corrupted, it will resend it, after its timeout. */
                case RECV_SEGMENT_CARRIES_DATA:
                        break;

                default: /* Heard FIN|ACK */
                        update_socket_lost_counters(_socket, _context->send_ack_ret_val);
                        _context->send_ack_ret_val = send_ack_control_segment(_socket, _address, _address_len);
                        if (_context->send_ack_ret_val == SEND_SEGMENT_FATAL_ERROR)
                                return handle_fatal_error(_context);
                        break;
                }
        }
        return CLOSED_1_SUBSTATE;
}

static shutdown_active_fsm_substates_t execute_closed_1_substate(microtcp_sock_t *const _socket, struct sockaddr *const _address,
                                                                 socklen_t _address_len, fsm_context_t *_context)
{
        release_and_reset_connection_resources(_socket, CLOSED);
        return CLOSED_2_SUBSTATE;
}

int microtcp_shutdown_active_fsm(microtcp_sock_t *const _socket, struct sockaddr *_address, socklen_t _address_len)
{
        RETURN_ERROR_IF_MICROTCP_SOCKET_INVALID(MICROTCP_SHUTDOWN_FAILURE, _socket, ESTABLISHED);                  /* Validate socket state. */
        RETURN_ERROR_IF_SOCKADDR_INVALID(MICROTCP_SHUTDOWN_FAILURE, _address);                                     /* Validate address structure. */
        RETURN_ERROR_IF_SOCKET_ADDRESS_LENGTH_INVALID(MICROTCP_SHUTDOWN_FAILURE, _address_len, sizeof(*_address)); /* Validate address length. */

        /* Initialize FSM's context. */
        fsm_context_t context = {.finack_retries_counter = get_shutdown_finack_retries(),
                                 .finack_wait_time_timer = get_shutdown_time_wait_period(),
                                 .recvfrom_timeout = get_socket_recvfrom_timeout(_socket),
                                 .errno = NO_ERROR};

        /* If we are in shutdown_active()'s FSM, that means that host (local) called shutdown not peer. */
        _socket->state = CLOSING_BY_HOST;

        shutdown_active_fsm_substates_t current_substate = CONNECTION_ESTABLISHED_SUBSTATE; /* Initial state in shutdown_active. */
        while (true)
        {
                LOG_FSM_SHUTDOWN("Entering %s", convert_substate_to_string(current_substate));
                switch (current_substate)
                {
                case CONNECTION_ESTABLISHED_SUBSTATE:
                        current_substate = execute_connection_established_substate(_socket, _address, _address_len, &context);
                        break;
                case FIN_WAIT_1_SUBSTATE:
                        current_substate = execute_fin_wait_1_substate(_socket, _address, _address_len, &context);
                        break;
                case FIN_DOUBLE_SUBSTATE:
                        current_substate = execute_fin_double_substate(_socket, _address, _address_len, &context);
                        break;
                case FIN_WAIT_2_RECV_SUBSTATE:
                        current_substate = execute_fin_wait_2_recv_substate(_socket, _address, _address_len, &context);
                        break;
                case FIN_WAIT_2_SEND_SUBSTATE:
                        current_substate = execute_fin_wait_2_send_substate(_socket, _address, _address_len, &context);
                        break;
                case TIME_WAIT_SUBSTATE:
                        current_substate = execute_time_wait_substate(_socket, _address, _address_len, &context);
                        break;
                case CLOSED_1_SUBSTATE:
                        current_substate = execute_closed_1_substate(_socket, _address, _address_len, &context);
                        break;
                case CLOSED_2_SUBSTATE:
                        log_errno_status(context.errno);
                        return MICROTCP_SHUTDOWN_SUCCESS;
                case EXIT_FAILURE_SUBSTATE:
                        _socket->state = ESTABLISHED; /* shutdown_active failed, thus connection remains ESTABLISHED. */
                        log_errno_status(context.errno);
                        return MICROTCP_SHUTDOWN_FAILURE;
                default:
                        FSM_DEFAULT_CASE_HANDLER(convert_substate_to_string, current_substate, EXIT_FAILURE_SUBSTATE);
                        break;
                }
        }
}

/* ----------------------------------------- LOCAL HELPER FUNCTIONS ----------------------------------------- */
// clang-format off
static const char *convert_substate_to_string(shutdown_active_fsm_substates_t _state)
{
        switch (_state)
        {
        case CONNECTION_ESTABLISHED_SUBSTATE:   return STRINGIFY(CONNECTION_ESTABLISHED_SUBSTATE);
        case FIN_WAIT_1_SUBSTATE:               return STRINGIFY(FIN_WAIT_1_SUBSTATE);
        case FIN_DOUBLE_SUBSTATE:               return STRINGIFY(FIN_DOUBLE_SUBSTATE);
        case FIN_WAIT_2_RECV_SUBSTATE:          return STRINGIFY(FIN_WAIT_2_RECV_SUBSTATE);
        case FIN_WAIT_2_SEND_SUBSTATE:          return STRINGIFY(FIN_WAIT_2_SEND_SUBSTATE);
        case TIME_WAIT_SUBSTATE:                return STRINGIFY(TIME_WAIT_SUBSTATE);
        case CLOSED_1_SUBSTATE:                 return STRINGIFY(CLOSED_1_SUBSTATE);
        case CLOSED_2_SUBSTATE:                 return STRINGIFY(CLOSED_2_SUBSTATE);
        case EXIT_FAILURE_SUBSTATE:             return STRINGIFY(EXIT_FAILURE_SUBSTATE);
        default:                                return "??SHUTDOWN_ACTIVE_SUBSTATE??";
        }
}
// clang-format on

static void log_errno_status(shutdown_active_fsm_errno_t _errno)
{
        switch (_errno)
        {
        case NO_ERROR:
                LOG_INFO("shutdown_active() FSM: closed connection gracefully.");
                break;
        case PEER_FINACK_RETRIES_EXHAUSTED:
                LOG_ERROR("shutdown_active() FSM: peer's `FIN|ACK` retries exhausted.");
                break;
        case PEER_FIN_TIMEOUT:
                LOG_WARNING("shutdown_active() FSM: timed out waiting for `FIN` from peer.");
                break;
        case DOUBLE_FIN:
                LOG_WARNING("shutdown_active() FSM: received `FIN` instread of `ACK`; Simultaneous shutdown_active().");
                break;
        case RST_EXPECTED_ACK:
                LOG_WARNING("shutdown_active() FSM: received `RST` while expecting `ACK` from peer.");
                break;
        case RST_EXPECTED_FINACK:
                LOG_WARNING("shutdown_active() FSM: received `RST` while expecting `FIN|ACK` from peer.");
                break;
        case RST_EXPECTED_TIMEOUT:
                LOG_WARNING("shutdown_active() FSM: received `RST` while expecting connection to timeout.");
                break;
        case FATAL_ERROR:
                LOG_ERROR("shutdown_active() FSM: encountered a fatal error on the host's side.");
                break;
        case SOCKET_TIMEOUT_SETUP_ERROR:
                LOG_WARNING("shutdown_active() FSM: failed to set socket's timeout for `recvfrom()`.");
                break;
        default:
                LOG_ERROR("shutdown_active() FSM: encountered an `unknown` error code: %d.", _errno); /* Log unknown errors for debugging purposes. */
                break;
        }
}

static inline shutdown_active_fsm_substates_t handle_fatal_error(fsm_context_t *_context)
{
        _context->errno = FATAL_ERROR;
        return EXIT_FAILURE_SUBSTATE;
}
