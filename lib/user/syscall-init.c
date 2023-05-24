#include "syscall-init.h" // ring0
#include "syscall.h" // ring3
#include "stdint.h"
#include "print.h"
#include "thread.h"
#include "string.h"
#include "console.h"

#define syscall_nr 32
typedef void* syscall;
syscall syscall_table[syscall_nr];

uint32_t sys_getpid() {
  return running_thread()->pid;
}

uint32_t sys_write(char* str) {
  console_put_str(str);
  return strlen(str);
}

void syscall_init() {
  put_str("syscall_init start\n");
  syscall_table[SYS_GETPID] = sys_getpid;
  syscall_table[SYS_GETPID] = sys_write;
  put_str("syscall_init done\n");
}
