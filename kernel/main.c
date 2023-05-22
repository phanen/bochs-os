#include "print.h"
#include "init.h"
#include "debug.h"
#include "memory.h"
#include "thread.h"
#include "interrupt.h"
#include "console.h"
#include "ioqueue.h"
#include "keyboard.h"

// void test_print();

void k_thread_a(void*);
void k_thread_b(void*);

int main() {

  put_str("you are in kernel now\n");

  init_all();

  // // test intr, set eflags.IF = 1 to enable
  // asm volatile("sti");
  // ASSERT(1 == 2);

  // // test malloc
  // void* addr = get_kernel_pages(3);
  // put_str("\n get_kernel_pages start vaddr is ");
  // put_int((uint32_t)addr);
  // put_str("\n");

  thread_create("k_thread_a", 31, k_thread_a, "argA ");
  thread_create("k_thread_b", 31, k_thread_b, "argB ");

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
    enum intr_status old_status = intr_disable();
    if (!ioq_empty(&kbd_buf)) {
      console_put_str(para);
      char byte = ioq_getchar(&kbd_buf);
      console_put_char(byte);
    }
    intr_set_status(old_status);
  };
}

void k_thread_b(void* arg) {

  char* para = arg;
  while (1) {
    enum intr_status old_status = intr_disable();
    if (!ioq_empty(&kbd_buf)) {
      console_put_str(para);
      char byte = ioq_getchar(&kbd_buf);
      console_put_char(byte);
    }
    intr_set_status(old_status);
  };
}
