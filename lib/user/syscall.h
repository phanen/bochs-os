#ifndef __LIB_USER_SYSCALL_H
#define __LIB_USER_SYSCALL_H

#include "stdint.h"

enum SYSCALL_NR {
  SYS_GETPID,
  SYS_WRITE,
  SYS_MALLOC,
  SYS_FREE,
};

// write -> int 0x80
//  -> idt (build in c)
//  -> syscall_handler (build in asm)
//  -> syscall_table (id = eax)
//  -> sys_write

uint32_t getpid();
uint32_t write(char* str);
void* malloc(uint32_t size);
void free(void* ptr);

#endif
