#ifndef MICROTCP_COMMON_MACROS_H
#define MICROTCP_COMMON_MACROS_H

#define STRINGIFY(_var_name) #_var_name
#define STRINGIFY_EXPANDED(_var_name) STRINGIFY(_var_name)

#define MIN(x, y) ((x) <= (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))

#endif /* MICROTCP_COMMON_MACROS_H */