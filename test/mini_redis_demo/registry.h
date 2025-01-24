#ifndef MINI_REDIS_DEMO_REGISTRY_H
#define MINI_REDIS_DEMO_REGISTRY_H

#include <stddef.h>
#include "status.h"

typedef struct registry registry_t;

registry_t *registry_create(size_t _capacity);
status_t registry_append(registry_t *_registry, const char *const _file_name);
void registry_destroy(registry_t **_registry_address);

#endif /* MINI_REDIS_DEMO_REGISTRY_H */
