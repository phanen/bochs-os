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
//  -> idt (c) -> intrientry in intr_entry_table (asm)
//  -> idt_table (asm) -> syscall_handler (asm)
//  -> syscall_table (index = eax) -> sys_write

uint32_t getpid();
uint32_t write(char* str);
void* malloc(uint32_t size);
void free(void* ptr);

#endif
