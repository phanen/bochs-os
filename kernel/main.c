#include "print.h"
#include "init.h"
#include "debug.h"

void test_print();

int main() {

  // test_print();
  put_str("you are in kernel now\n");

  init_all();
  // set eflags.IF = 1 to enable intrrupt
  asm volatile("sti");

  ASSERT(1 == 2);

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
