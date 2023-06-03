#ifndef __LIB_USER_SYSCALL_H
#define __LIB_USER_SYSCALL_H

#include "stdint.h"
// #include "thread.h"
#include "fs.h"

enum syscall_nr {
  SYS_GETPID,
  SYS_MALLOC,
  SYS_FREE,
  SYS_FORK,
  SYS_PUTCHAR,
  SYS_CLEAR,

  SYS_OPEN,
  SYS_CLOSE,
  SYS_WRITE,
  SYS_READ,
  SYS_LSEEK,
  SYS_UNLINK,

  SYS_MKDIR,
  SYS_OPENDIR,
  SYS_CLOSEDIR,
  SYS_RMDIR,
  SYS_READDIR,
  SYS_REWINDDIR,
  SYS_STAT,
  SYS_GETCWD,
  SYS_CHDIR,
  SYS_PS,
  SYS_EXECV,
  SYS_WAIT,
  SYS_EXIT,
  SYS_PIPE,
  SYS_DUP2,
};

// write -> int 0x80
//       -> syscall_handler -> syscall_table (index = eax)
//       -> sys_write

uint32_t getpid();
void* malloc(uint32_t size);
void free(void* ptr);
// pid_t fork();
int16_t fork();
void putchar(char c);
void clear();

int32_t open(char* pathname, uint8_t flag);
int32_t close(int32_t fd);
int32_t write(int32_t fd, const void* buf, uint32_t count);
int32_t read(int32_t fd, void* buf, uint32_t count);
int32_t lseek(int32_t fd, int32_t offset, uint8_t whence);
int32_t unlink(const char* pathname);

int32_t mkdir(const char* pathname);
struct dir* opendir(const char* name);
int32_t closedir(struct dir* dir);
int32_t rmdir(const char* pathname);
struct dir_entry* readdir(struct dir* dir);
void rewinddir(struct dir* dir);
int32_t stat(const char* path, struct stat* buf);
char* getcwd(char* buf, uint32_t size);
int32_t chdir(const char* path);
void ps(void);

int execv(const char* pathname, char** argv);

void exit(int32_t status);
int16_t wait(int32_t* status);

int32_t pipe(int32_t pipefd[2]);

void dup2(uint32_t new_fd, uint32_t old_fd);

#endif
