#include "shell.h"
#include "stdint.h"
#include "fs.h"
#include "file.h"
#include "syscall.h"
#include "stdio.h"
#include "global.h"
#include "assert.h"
#include "string.h"

#define CMD_LEN           128
#define CWD_CACHE_LEN     64

static char cmd_line[CMD_LEN] = {0};
char cwd_cache[CWD_CACHE_LEN] = {0};

void print_prompt(void) {
  printf("[phanium@bochs %s]$ ", cwd_cache);
}

// read from stdin
//    parse it into cmdline buf and do echo
static void readline(char* buf, int32_t count) {

  assert(buf != NULL && count > 0);
  char* pos = buf;

  // read input
  while (read(stdin_no, pos, 1) != -1 && (pos - buf) < count) {
    // parse and each
    switch (*pos) {
      case '\n':
      case '\r':
        *pos = 0;
        putchar('\n');
        return;

      case '\b':
        if (buf[0] != '\b') { // or (pos != buf)
          --pos;
          putchar('\b');
        }
        break;

      default:
        putchar(*pos);
        pos++;
    }
  }
  printf("readline: cmdline overflow, max num of char is 128\n");
}

void shell_run() {

  cwd_cache[0] = '/';

  while (1) {

    print_prompt(); 

    // read input
    memset(cmd_line, 0, CMD_LEN);
    readline(cmd_line, CMD_LEN);
    if (cmd_line[0] == 0) { // CRLF only
      continue;
    }

    // unimplement, parse cmd_line then exec
    //
  }
  panic("my_shell: should not be here");
}
