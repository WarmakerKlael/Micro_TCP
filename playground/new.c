#include <sys/cdefs.h>
#include <stdint.h>
#include <stdio.h>

/* Function to determine if `_ack1` is newer than `_ack2` */
static __always_inline uint32_t get_most_recent_ack(uint32_t _ack1, uint32_t _ack2)
{
        return (int32_t)(_ack1 - _ack2) > 0 ? _ack1 : _ack2;
}

int main()
{
        printf("newer == %u\n", get_most_recent_ack( 0, UINT32_MAX));
}