#include "print.h"

void test_print() {
  put_str("call put_str in kernel\n");
  put_int(0x123abc);
  put_char('\n');
  put_int(0x23048);
  put_char('\n');
  while(1);
}

int main() {
  test_print();
}
