#include "stdint.h"

typedef uint16_t pid_t;

extern uint8_t pid_bitmap_bits[128];


void pid_pool_init();
pid_t allocate_pid();
void release_pid(pid_t pid);
