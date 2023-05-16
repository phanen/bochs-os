#include "print.h"
#include "init.h"
#include "debug.h"
#include "memory.h"
#include "thread.h"

void test_print();

void k_thread_a(void*);

int main() {

  put_str("you are in kernel now\n");

  // // test print
  // test_print();

  init_all();

  // // test intr
  // set eflags.IF = 1 to enable intrrupt
  // asm volatile("sti");

  // // test ASSERT
  // ASSERT(1 == 2);

  // // test malloc
  // void* addr = get_kernel_pages(3);
  // put_str("\n get_kernel_pages start vaddr is ");
  // put_int((uint32_t)addr);
  // put_str("\n");

  // // test thread
  // thread_start("k_thread_a", 31, k_thread_a, "argA ");

  while(1);
  return 0;
}

void test_print() {

  put_str("call put_str in kernel\n");
  put_int(0x123abc);
  put_char('\n');
  put_int(0x23048);
  put_char('\n');
  // while(1);
  // return;
}

void k_thread_a(void* arg) {

  char* para = arg;
  while (1) { put_str(para); };
}
