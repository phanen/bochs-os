#include "stdint.h"
#include "fs.h"
#include "file.h"
#include "syscall.h"
#include "stdio.h"
#include "global.h"
#include "assert.h"
#include "string.h"

#include "shell.h"
#include "builtin_cmd.h"
#include "path_parse.h"
#include "wait_exit.h"

// NOTE: shell aims to be ported into userland
//    do not use syslevel api, (e.g. begin with `sys_`)

extern char* name_buf;

#define CMD_LEN           MAX_PATH_LEN
#define MAX_ARG_NR        16

static char cmd_line[CMD_LEN] = {0};
char cwd_cache[MAX_PATH_LEN] = {0};

// cache the full_path (for some builtin_cmd)
char final_path[MAX_PATH_LEN] = {0};

char* argv[MAX_ARG_NR];
int32_t argc = -1;

static void print_prompt(void) {
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
static int32_t cmd_parse(char* cmd_str, char split) {

  assert(cmd_str != NULL);

  int32_t i = 0;
  while (i < MAX_ARG_NR) {
    argv[i] = NULL;
    i++;
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

int32_t builtin_run() {

  memset(final_path, 0, MAX_PATH_LEN);

  if (!strcmp("ls", argv[0])) {
    builtin_ls(argc, argv);
  }
  else if (!strcmp("cd", argv[0])) {
    if (!builtin_cd(argc, argv)) {
      memset(cwd_cache, 0, MAX_PATH_LEN);
      strcpy(cwd_cache, final_path);
    }
  }
  else if (!strcmp("pwd", argv[0])) {
    builtin_pwd(argc, argv);
  }
  else if (!strcmp("ps", argv[0])) {
    builtin_ps(argc, argv);
  }
  else if (!strcmp("clear", argv[0])) {
    builtin_clear(argc, argv);
  }
  else if (!strcmp("mkdir", argv[0])) {
    builtin_mkdir(argc, argv);
  }
  else if (!strcmp("rmdir", argv[0])) {
    builtin_rmdir(argc, argv);
  }
  else if (!strcmp("rm", argv[0])) {
    builtin_rm(argc, argv);
  }
  else {
    // printf("external command\n");
    return -2;
  }
  return 0;
}

void external_run() {

  int32_t pid = fork();

  if (!pid) {
    make_clear_abs_path(argv[0], final_path);
    argv[0] = final_path;

    struct stat file_stat;
    memset(&file_stat, 0, sizeof(struct stat));
    if (stat(argv[0], &file_stat) == -1) {
      printf("external_run: cannot access %s: No such file or directory\n", argv[0]);
      exit(-1);
    }
    execv(argv[0], argv);
  }

  int32_t status, child_pid;
  child_pid = wait(&status);
  printf("shell recieve %d, (status: %d)\n", child_pid, status);
}

int32_t cmd_run () {
  // at lease argv[0] is not null
  int32_t builtin_ok = builtin_run();
  if (builtin_ok != -2) {
    return builtin_ok;
  }

  external_run();
  return 0;
}

int32_t pipe_run() {

  char* pipe_sym = strchr(cmd_line, '|');
  if (!pipe_sym) { // no a pipe cmd
    return -2;
  }

  int32_t fd[2] = {-1};
  pipe(fd);

  // write to pipe
  dup2(fd[1], 1);

  // trunc left part
  char* each_cmd = cmd_line;
  pipe_sym = strchr(each_cmd, '|');
  *pipe_sym = 0;
  argc = cmd_parse(each_cmd, ' ');
  cmd_run();

  // read from pipe
  dup2(fd[0], 0);

  // from the first right part
  each_cmd = pipe_sym + 1;
  while ((pipe_sym = strchr(each_cmd, '|'))) {
    *pipe_sym = 0;
    argc = cmd_parse(each_cmd, ' ');
    cmd_run();
    each_cmd = pipe_sym + 1;
  }

  // resume stdout
  dup2(1, 1);

  argc = cmd_parse(each_cmd, ' ');
  cmd_run();

  // resume stdin
  dup2(0, 0);

  close(fd[0]);
  close(fd[1]);
  return 0;
}

void shell_run() {

  cwd_cache[0] = '/';
  cwd_cache[1] = 0;

  while (1) {

    print_prompt();
    // printf("shell pid is %d, name is %s", getpid(), name_buf);

    // read one line input
    memset(cmd_line, 0, CMD_LEN);
    readline(cmd_line, CMD_LEN);

    // no input
    if (!*cmd_line) {
      continue;
    }

    int32_t pipe_ok = pipe_run();
    if (pipe_ok != -2) {
      continue;
    } // else, no pipe sym

    // tokenize into argv
    for (uint32_t i = 0; i < MAX_ARG_NR; i++) {
      argv[i] = NULL;
    }
    argc = cmd_parse(cmd_line, ' ');
    if (argc == -1) {
      printf("num of arguments exceed %d\n", MAX_ARG_NR);
      continue;
    }

    cmd_run();
  }
  panic("shell_run: should not be here");
}
