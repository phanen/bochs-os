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
#include "memory.h"

void k_thread_a(void*);
void k_thread_b(void*);
void u_prog_a(void);
void u_prog_b(void);
int bss_var_a = 0, bss_var_b = 0;

int main() {

  put_str("you are in kernel now\n");
  init_all();

  process_execute(u_prog_a, "user_prog_a");
  process_execute(u_prog_b, "user_prog_b");

  intr_enable();

  thread_create("k_thread_a", 31, k_thread_a, "thread_a");
  thread_create("k_thread_b", 31, k_thread_b, "thread_b");

  // console_put_str(" main_pid:0x");
  // console_put_int(sys_getpid());
  // console_put_char('\n');

  void* addr = sys_malloc(33);
  printf("%s, pid:%d addr:0x%x %c", "main", sys_getpid(), (int)addr, '\n');

  while(1) {
    // console_put_str("Main ");
  }
  return 0;
}

void k_thread_a(void* arg) {
  char* para = arg;
  void* addr = sys_malloc(33);
  printf("%s, pid:%d addr:0x%x %c", para, sys_getpid(), (int)addr, '\n');
  while (1) {
  }
}

void k_thread_b(void* arg) {

  char* para = arg;
  void* addr = sys_malloc(63);
  printf("%s, pid:%d addr:0x%x %c", para, sys_getpid(), (int)addr, '\n');
  while (1) {
  }
}

void u_prog_a(void) {
  char* name = "uprog_a";
  printf("%s, pid:%d %c", name, getpid(),'\n');
  while(1) {
    bss_var_a = getpid();
  }

}

void u_prog_b(void) {
  char* name = "uprog_b";
  printf("%s, pid:%d %c", name, getpid(),'\n');
  while(1) {
    bss_var_b = getpid();
  }
}
