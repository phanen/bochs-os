#include "print.h"
#include "init.h"
// #include "debug.h"
// #include "memory.h"
#include "thread.h"
#include "interrupt.h"
#include "console.h"
// #include "ioqueue.h"
// #include "keyboard.h"
#include "process.h"

void k_thread_a(void*);
void k_thread_b(void*);
void u_prog_a(void);
void u_prog_b(void);
int bss_var_a = 0, bss_var_b = 0;

int main() {

  put_str("you are in kernel now\n");
  init_all();

  thread_create("k_thread_a", 31, k_thread_a, "argA ");
  thread_create("k_thread_b", 31, k_thread_b, "argB ");

  process_execute(u_prog_a, "user_prog_a");
  process_execute(u_prog_b, "user_prog_b");

  intr_enable();
  while(1) {
    // console_put_str("Main ");
  }
  return 0;
}

void k_thread_a(void* arg) {

  char* para = arg;
  while (1) {
    // disable intr?
    // since you disable intr....
    // both the `console_put_str` and `ioq_getchar` don't need to be thread-safe
    // why bother you use them?

    console_put_str(" v_a:0x");
    console_put_int(bss_var_a);
    console_put_str("\n");
  }
}

void k_thread_b(void* arg) {

  char* para = arg;
  while (1) {
    console_put_str(" v_b:0x");
    console_put_int(bss_var_b);
    console_put_str("\n");
    // enum intr_status old_status = intr_disable();
    // if (!ioq_empty(&kbd_buf)) {
    //   console_put_str(para);
    //   char byte = ioq_getchar(&kbd_buf);
    //   console_put_char(byte);
    // }
    // intr_set_status(old_status);
  };
}

void u_prog_a(void) {
  while(1) {
    bss_var_a++;
  }

}

void u_prog_b(void) {
  while(1) {
    bss_var_b++;
  }
}
