#ifndef MICROTCP_SETTINGS_PROMPTS_H
#define MICROTCP_SETTINGS_PROMPTS_H

void prompt_set_assembly_buffer_length(void);
void prompt_set_ack_timeout(void);
void prompt_set_connect_retries(void);
void prompt_set_accept_retries();
void prompt_set_shutdown_retries();
void prompt_set_shutdown_time_wait_period();

#endif /* MICROTCP_SETTINGS_PROMPTS_H */