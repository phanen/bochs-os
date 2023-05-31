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

#include "shell.h"

void init();

int main() {

   put_str("you are in kernel now\n");
   init_all();
   cls_screen();

   console_put_str("[phanium@bochs /]$ ");
   // shell_run();
   while (1);
   return 0;
}


void init(void) {
   pid_t ret_pid = fork();
   if(ret_pid) {
      while (1);
   } else {
      shell_run();
   }
   while(1);
}
