#include "stdio.h"
#include "syscall.h"
#include "string.h"
#include "assert.h"

int main(int argc, char** argv) {

  int i = 0;
  while(i < argc) {
    printf("argv[%d] is %s\n", i, argv[i]);
    i++;
  }

  int pid = fork();

  if (pid) {
    int delay = 900000;
    while(delay--);
    printf("\n      I'm father prog, my pid:%d, I will show process list\n", getpid());
    ps();
  }
  else {
    char abs_path[512] = {0};

    assert(argv[1]);
    printf("\n      I'm child prog, my pid:%d, I will exec %s right now\n", getpid(), argv[1]);
    if (argv[1][0] != '/') {
      getcwd(abs_path, 512);
      strcat(abs_path, "/");
      strcat(abs_path, argv[1]);
      execv(abs_path, argv);
    }
    else {
      execv(argv[1], argv);
    }
  }

  while(1);
  return 0;
}
