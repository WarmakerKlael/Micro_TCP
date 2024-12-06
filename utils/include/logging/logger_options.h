#ifndef LOGGER_OPTIONS_H
#define LOGGER_OPTIONS_H

/* Logger Option getters(): */

/* Logger Option Getters: */
_Bool logger_is_enabled(void);
_Bool logger_is_info_enabled(void);
_Bool logger_is_warning_enabled(void);
_Bool logger_is_error_enabled(void);
_Bool logger_is_allocator_enabled(void);

/* Logger Option Setters: */
void logger_set_enabled(_Bool _enabled);
void logger_set_info_enabled(_Bool _enabled);
void logger_set_warning_enabled(_Bool _enabled);
void logger_set_error_enabled(_Bool _enabled);
void logger_set_allocator_enabled(_Bool _enabled);

#endif /* LOGGER_OPTIONS_H */