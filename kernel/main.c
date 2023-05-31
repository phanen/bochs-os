#include "print.h"
#include "init.h"
// #include "debug.h"
#include "thread.h"
#include "interrupt.h"
#include "console.h"
// #include "ioqueue.h"
// #include "keyboard.h"
#include "process.h"
#include "syscall.h"
#include "syscall-init.h"

#include "stdio.h"
#include "stdio-kernel.h"
#include "memory.h"
#include "fs.h"
#include "string.h"
#include "dir.h"

void init();

int main() {

  put_str("you are in kernel now\n");
  init_all();

  while (1);
  return 0;
}


void init(void) {
   uint32_t ret_pid = fork();
   if(ret_pid) {
      printf("i am father, my pid is %d, child pid is %d\n", getpid(), ret_pid);
   } else {
      printf("i am child, my pid is %d, ret pid is %d\n", getpid(), ret_pid);
   }
   while(1);
}
