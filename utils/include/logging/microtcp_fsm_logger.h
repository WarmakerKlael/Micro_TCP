#ifndef MICROTCP_FSM_LOGGER_H
#define MICROTCP_FSM_LOGGER_H

enum fsm_log_tag
{
	FSM_CONNECT,
	FSM_ACCEPT,
	FSM_SHUTDOWN,
	FSM_RECV,
	FSM_SEND
};

void log_fsm_message_thread_safe(enum fsm_log_tag _fsm_log_tag, const char *_format_message, ...);

/* In MicroTCP these are not performance critical, thus are always visible. (Expect is logging is completely disabled (through MicroTCP's settings). */
#define LOG_FSM_CONNECT(_format_message, ...) log_fsm_message_thread_safe(FSM_CONNECT, _format_message, ##__VA_ARGS__)
#define LOG_FSM_ACCEPT(_format_message, ...) log_fsm_message_thread_safe(FSM_ACCEPT, _format_message, ##__VA_ARGS__)
#define LOG_FSM_SHUTDOWN(_format_message, ...) log_fsm_message_thread_safe(FSM_SHUTDOWN, _format_message, ##__VA_ARGS__)

/* Enable logging for RECV and SEND FSM only in VERBOSE_MODE for performance reasons */
#ifdef VERBOSE_MODE
#define LOG_FSM_RECV(_format_message, ...) log_fsm_message_thread_safe(FSM_RECV, _format_message, ##__VA_ARGS__)
#define LOG_FSM_SEND(_format_message, ...) log_fsm_message_thread_safe(FSM_SEND, _format_message, ##__VA_ARGS__)
#else /* ifndef VERBOSE_MODE */
#define LOG_FSM_RECV(_format_message, ...)
#define LOG_FSM_SEND(_format_message, ...)
#endif /* VERBOSE_MODE */

#endif /* MICROTCP_FSM_LOGGER_H */
