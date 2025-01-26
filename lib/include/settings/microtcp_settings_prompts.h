#ifndef MICROTCP_SETTINGS_PROMPTS_H
#define MICROTCP_SETTINGS_PROMPTS_H

void prompt_set_rrb_size(void);
void prompt_set_microtcp_ack_timeout(void);
void prompt_set_connect_retries(void);
void prompt_set_accept_retries(void);
void prompt_set_shutdown_retries(void);
void prompt_set_shutdown_time_wait_period(void);

void configure_microtcp_settings(void);

#endif /* MICROTCP_SETTINGS_PROMPTS_H */
