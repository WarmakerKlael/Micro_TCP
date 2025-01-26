#include "settings/microtcp_settings_prompts.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include "cli_color.h"
#include "microtcp.h"
#include "microtcp_helper_functions.h"
#include "microtcp_helper_macros.h"
#include "microtcp_settings_common.h"
#include "settings/microtcp_settings.h"
#include "smart_assert.h"
#include "microtcp_prompt_util.h"

void prompt_set_rrb_length(void)
{
        const char *prompt = "Specify the Receive-Ring-Buffer size (uint32_t, power of 2, in bytes, default: " STRINGIFY_EXPANDED(MICROTCP_RECVBUF_LEN) "): ";

        long rrb_size = 0;
        while (1)
        {
                PROMPT_WITH_READLINE(prompt, "%ld", &rrb_size);
                if (rrb_size > 0 && rrb_size < UINT32_MAX && IS_POWER_OF_2(rrb_size))
                        break;
                clear_line();
        }
        set_microtcp_bytestream_rrb_size(rrb_size);
}

void prompt_set_ack_timeout(void)
{
        const char *prompt = "Specify the ACK timeout interval (Default: " STRINGIFY_EXPANDED(DEFAULT_MICROTCP_ACK_TIMEOUT_SEC) " seconds " STRINGIFY_EXPANDED(DEFAULT_MICROTCP_ACK_TIMEOUT_USEC) " microseconds): ";
        struct timeval ack_timeout_interval = {0};
        do
        {
                PROMPT_WITH_READLINE(prompt, "%ld%ld", &ack_timeout_interval.tv_sec, &ack_timeout_interval.tv_usec);
                if (ack_timeout_interval.tv_sec < 0 || ack_timeout_interval.tv_usec < 0)
                        clear_line();
        } while (ack_timeout_interval.tv_sec < 0 || ack_timeout_interval.tv_usec < 0);
        set_microtcp_ack_timeout(ack_timeout_interval);
}

void prompt_set_connect_retries(void)
{
        const char *prompt = "Specify the maximum number of connection retries on RST (Default: " STRINGIFY_EXPANDED(DEFAULT_CONNECT_RST_RETRIES) "): ";
        long retries = 0;
        do
        {
                PROMPT_WITH_READLINE(prompt, "%ld", &retries);
                if (retries < 0)
                        clear_line();
        } while (retries < 0);
        set_connect_rst_retries(retries);
}

void prompt_set_accept_retries(void)
{
        const char *prompt = "Specify the maximum SYN|ACK retries during handshake (Default: " STRINGIFY_EXPANDED(LINUX_DEFAULT_ACCEPT_TIMEOUTS) "): ";
        long retries = 0;
        do
        {
                PROMPT_WITH_READLINE(prompt, "%ld", &retries);
                if (retries < 0)
                        clear_line();
        } while (retries < 0);
        set_accept_synack_retries(retries);
}

void prompt_set_shutdown_retries(void)
{
        const char *prompt = "Specify the maximum FIN|ACK retries during shutdown (Default: " STRINGIFY_EXPANDED(TCP_RETRIES2) "): ";
        long retries = 0;
        do
        {
                PROMPT_WITH_READLINE(prompt, "%ld", &retries);
                if (retries < 0)
                        clear_line();
        } while (retries < 0);
        set_shutdown_finack_retries(retries);
}

void prompt_set_shutdown_time_wait_period(void)
{
        const char *prompt = "Specify the TIME-WAIT period (Default: " STRINGIFY_EXPANDED(DEFAULT_SHUTDOWN_TIME_WAIT_SEC) " seconds " STRINGIFY_EXPANDED(DEFAULT_SHUTDOWN_TIME_WAIT_USEC) " microseconds): ";
        struct timeval time_wait_period = {0};
        do
        {
                PROMPT_WITH_READLINE(prompt, "%ld%ld", &time_wait_period.tv_sec, &time_wait_period.tv_usec);
                if (time_wait_period.tv_sec < 0 || time_wait_period.tv_usec < 0)
                        clear_line();
        } while (time_wait_period.tv_sec < 0 || time_wait_period.tv_usec < 0);
        set_shutdown_time_wait_period(time_wait_period);
}

void configure_microtcp_settings(void)
{
        prompt_set_rrb_length();
        prompt_set_ack_timeout();
        prompt_set_connect_retries();
        prompt_set_accept_retries();
        prompt_set_shutdown_retries();
        prompt_set_shutdown_time_wait_period();
}
