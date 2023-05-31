#include "shell.h"
#include "builtin_cmd.h"
#include "stdint.h"
#include "fs.h"
#include "file.h"
#include "syscall.h"
#include "stdio.h"
#include "global.h"
#include "assert.h"
#include "string.h"

// NOTE: this is a shell in userland
//    we don't use any api begin with `FUC_`

#define CMD_LEN           MAX_PATH_LEN
#define CWD_CACHE_LEN     64

#define MAX_ARG_NR        16

static char cmd_line[CMD_LEN] = {0};
char cwd_cache[CWD_CACHE_LEN] = {0};

char final_path[MAX_PATH_LEN] = {0};

char* argv[MAX_ARG_NR];
int32_t argc = -1;

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

      // c-l
      case 'l' - 'a':
        *pos = 0;
        clear();
        print_prompt();
        printf("%s", buf);
        break;

      // c-u
      case 'u' - 'a':
        while (buf != pos) {
          putchar('\b');
          *(pos--) = 0;
        }
        break;

      default:
        putchar(*pos);
        pos++;
    }
  }
  printf("readline: cmdline overflow, max num of char is 128\n");
}

// tokenize cmd_str into argv
//      warn: in-place
static int32_t cmd_parse(char* cmd_str, char** argv, char split) {

  assert(cmd_str != NULL);

  int32_t arg_idx = 0;
  while (arg_idx < MAX_ARG_NR) {
    argv[arg_idx] = NULL;
    arg_idx++;
  }

  char* next = cmd_str;
  int32_t argc = 0;
  while(*next) {

    // ignore splitors
    while(*next == split) {
      next++;
    }

    // end with splitors "ls dir2 "
    if (*next == 0) {
      break;
    }

    // current slot out of bound
    if (argc == MAX_ARG_NR) {
      return -1;
    }

    // find a token, trunc it
    argv[argc] = next;
    while (*next && *next != split) {
      next++;
    }
    if (*next) {
      *next++ = 0;
    }

    argc++;
  }

  return argc;
}

void shell_run() {

  cwd_cache[0] = '/';

  while (1) {

    print_prompt();

    memset(final_path, 0, MAX_PATH_LEN);
    memset(cmd_line, 0, CMD_LEN);

    // read one line input
    readline(cmd_line, CMD_LEN);

    // no input
    if (cmd_line[0] == 0) {
      continue;
    }

    // tokenize
    argc = -1;
    argc = cmd_parse(cmd_line, argv, ' ');
    if (argc == -1) {
      printf("num of arguments exceed %d\n", MAX_ARG_NR);
      continue;
    }

    // foreach token
    for (int32_t i = 0; i < argc; i++) {
      make_clear_abs_path(argv[i], final_path);
      printf("%s -> %s\n", argv[i], final_path);
    }

    // unimplement
  }
  panic("my_shell: should not be here");
}
