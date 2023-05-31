#ifndef __USERPROG_FORK_H
#define __USERPROG_FORK_H

#include "thread.h"

// only for user (not used for kernel)
//    need stack switch?
pid_t sys_fork(void);

#endif
