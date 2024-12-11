#ifndef SMART_ASSERT_H
#define SMART_ASSERT_H

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#ifndef __FILENAME__
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif /* __FILENAME__ */

#define _TRUE 1
#define _FALSE 0

#define _GET_NTH_COMMA_POSITION(_string, _target_comma_count)                \
        ({                                                                  \
                int string_index = 0;                                       \
                int found_commas = 0;                                       \
                for (; (_string)[string_index] != '\0'; string_index++)     \
                {                                                           \
                        if ((_string)[string_index] == ',')                 \
                                found_commas++;                             \
                        if (found_commas == (_target_comma_count))          \
                                break;                                      \
                }                                                           \
                (found_commas == (_target_comma_count)) ? string_index : 0; \
        })

#define _GET_LEADING_SPACES(_string)                        \
        ({                                                 \
                int offset = 0;                            \
                while (((char *)(_string))[offset] == ' ') \
                        offset++;                          \
                offset;                                    \
        })

#define _PRINT_SMART_ASSERT(_format, ...)                                                      \
        do                                                                                    \
        {                                                                                     \
                fprintf(stderr, "%s:%d: %s: SMART_ASSERT(): ", __FILENAME__, __LINE__, __func__); \
                fprintf(stderr, _format, ##__VA_ARGS__);                                      \
                fprintf(stderr, "\n");                                                        \
        } while (0);

#define SMART_ASSERT(...)                                                                                                                                   \
        do                                                                                                                                                  \
        {                                                                                                                                                   \
                const char *assertion_text = #__VA_ARGS__;                                                                                                  \
                const _Bool assertions[] = {__VA_ARGS__};                                                                                                   \
                const int assertion_count = sizeof(assertions);                                                                                             \
                _Static_assert(sizeof(assertions) != 0, "SMART_ASSERT() empty parameter list. At least one condition required");                            \
                _Bool assert_failure = _FALSE;                                                                                                               \
                for (int assertion_index = 0; assertion_index < assertion_count; assertion_index++)                                                         \
                {                                                                                                                                           \
                        if (assertions[assertion_index] == _TRUE)                                                                                            \
                                continue;                                                                                                                   \
                        assert_failure = _TRUE;                                                                                                              \
                        if (assertion_count == 1)                                                                                                           \
                        {                                                                                                                                   \
                                _PRINT_SMART_ASSERT("Condition `%s` failed.", assertion_text);                                                               \
                                continue;                                                                                                                   \
                        }                                                                                                                                   \
                        if (assertion_index == 0) /* First in list. */                                                                                      \
                        {                                                                                                                                   \
                                int suffix_comma_position = _GET_NTH_COMMA_POSITION(assertion_text, 1);                                                      \
                                _PRINT_SMART_ASSERT("Condition #%d: `%.*s` failed.", assertion_index + 1, suffix_comma_position, assertion_text);            \
                                continue;                                                                                                                   \
                        }                                                                                                                                   \
                        if (assertion_index == assertion_count - 1) /* Last in list. */                                                                     \
                        {                                                                                                                                   \
                                int prefix_comma_position = _GET_NTH_COMMA_POSITION(assertion_text, assertion_index);                                        \
                                int leading_spaces = _GET_LEADING_SPACES(assertion_text + prefix_comma_position + 1);                                        \
                                const char *assertion_address = assertion_text + prefix_comma_position + 1 + leading_spaces; /* +1 to skip comma itself. */ \
                                _PRINT_SMART_ASSERT("Condition %d: `%s` failed.", assertion_index + 1, assertion_address)                                    \
                                continue;                                                                                                                   \
                        }                                                                                                                                   \
                        /*Not first not last... --> Intermidiate */                                                                                         \
                        int prefix_comma_position = _GET_NTH_COMMA_POSITION(assertion_text, assertion_index);                                                \
                        int suffix_comma_position = _GET_NTH_COMMA_POSITION(assertion_text, assertion_index + 1);                                            \
                        int leading_spaces = _GET_LEADING_SPACES(assertion_text + prefix_comma_position + 1);                                                \
                        int condition_string_size = suffix_comma_position - prefix_comma_position - 1 - leading_spaces;                                     \
                        const char *assertion_address = assertion_text + prefix_comma_position + 1 + leading_spaces;                                        \
                        _PRINT_SMART_ASSERT("Condition %d: `%.*s` failed.", assertion_index + 1, condition_string_size, assertion_address);                  \
                }                                                                                                                                           \
                if (assert_failure == _TRUE)                                                                                                                 \
                        abort();                                                                                                                            \
        } while (0)

/* Clear global namespace. */
// #undef _TRUE
// #undef _FALSE
// #undef _GET_NTH_COMMA_POSITION
// #undef _GET_LEADING_SPACES
// #undef _PRINT_SMART_ASSERT

#endif /* SMART_ASSERT_H */
