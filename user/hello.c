#include "stdio.h"
#include "syscall.h"

int main(int argc, char** argv) {
  printf("hello user world\n");
  execv(argv[0], argv);
  return 0;
}
