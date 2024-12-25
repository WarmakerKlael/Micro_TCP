#include "logging/microtcp_fsm_logger.h"
#include <stdarg.h>
#include <stdio.h>
#include "microtcp_defines.h"
#include "microtcp_helper_macros.h"
#include "microtcp_logging_colors.h"

extern FILE *microtcp_log_stream;

/* TODO: write DOC */
static const char *get_colored_fsm_log_tag_string(enum fsm_log_tag _fsm_log_tag);

void log_fsm_message_thread_safe(enum fsm_log_tag _fsm_log_tag, const char *_format_message, ...)
{
	va_list args;
	va_start(args, _format_message);

	const char *colored_string_fsm_log_tag = get_colored_fsm_log_tag_string(_fsm_log_tag);
	fprintf(microtcp_log_stream, LOG_DEFAULT_COLOR); /* Set DEFAULT logging color. */
	fprintf(microtcp_log_stream, "[%s]: ", colored_string_fsm_log_tag);
	fprintf(microtcp_log_stream, LOG_MESSAGE_COLOR); /* Set logging message color. */
	vfprintf(microtcp_log_stream, _format_message, args);
	fprintf(microtcp_log_stream, "%s\n", LOG_DEFAULT_COLOR);
	fflush(microtcp_log_stream); /* We flush even though fprintf ends with '\n', in case stdout is fully-buffered. (file, pipe). */

	va_end(args);
}

// clang-format off
static const char *get_colored_fsm_log_tag_string(enum fsm_log_tag _fsm_log_tag)
{
	switch (_fsm_log_tag)
	{
	case FSM_CONNECT: 	return LOG_FSM_CONNECT_COLOR 	PROJECT_NAME " " STRINGIFY(FSM_CONNECT) 	LOG_DEFAULT_COLOR;
	case FSM_ACCEPT: 	return LOG_FSM_ACCEPT_COLOR 	PROJECT_NAME " " STRINGIFY(FSM_ACCEPT) 		LOG_DEFAULT_COLOR;
	case FSM_SHUTDOWN:	return LOG_FSM_SHUTDOWN_COLOR 	PROJECT_NAME " " STRINGIFY(FSM_SHUTDOWN) 	LOG_DEFAULT_COLOR;
	case FSM_RECV:		return LOG_FSM_RECV_COLOR 	PROJECT_NAME " " STRINGIFY(FSM_RECV) 		LOG_DEFAULT_COLOR;
	case FSM_SEND:		return LOG_FSM_SEND_COLOR 	PROJECT_NAME " " STRINGIFY(FSM_SEND) 		LOG_DEFAULT_COLOR;
	default:		return LOG_DEFAULT_COLOR 	PROJECT_NAME " " "??FSM??" 			LOG_DEFAULT_COLOR;
	}
}
// clang-format on