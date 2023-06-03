#include "syscall.h"

#define _syscall0(NUMBER) ({  \
  int retval;	\
  asm volatile (  \
                "int $0x80"	\
                : "=a" (retval)	\
                : "a" (NUMBER)	\
                : "memory"	\
);	\
  retval;	\
})

#define _syscall1(NUMBER, ARG1) ({	\
  int retval;	\
  asm volatile (	\
                "int $0x80"	\
                : "=a" (retval)	\
                : "a" (NUMBER), "b" (ARG1)	\
    : "memory"	\
);	\
  retval;	\
})

#define _syscall2(NUMBER, ARG1, ARG2) ({	\
  int retval;	\
  asm volatile (	\
                "int $0x80"	\
                : "=a" (retval)	\
                : "a" (NUMBER), "b" (ARG1), "c" (ARG2)	\
    : "memory"	\
);	\
  retval;	\
})

#define _syscall3(NUMBER, ARG1, ARG2, ARG3) ({	\
  int retval;	\
  asm volatile (	\
                "int $0x80"	\
                : "=a" (retval)	\
                : "a" (NUMBER), "b" (ARG1), "c" (ARG2), "d" (ARG3)	\
    : "memory"	\
);	\
  retval;	\
})

uint32_t getpid() {
  return _syscall0(SYS_GETPID);
}

void* malloc(uint32_t size) {
  return (void*)_syscall1(SYS_MALLOC, size);
}

void free(void* ptr) {
  _syscall1(SYS_FREE, ptr);
}

int16_t fork() {
  return _syscall0(SYS_FORK);
}

void putchar(char c) {
  _syscall1(SYS_PUTCHAR, c);
}

void clear() {
  _syscall0(SYS_CLEAR);
}

int32_t open(char* pathname, uint8_t flag) {
  return _syscall2(SYS_OPEN, pathname, flag);
}

int32_t close(int32_t fd) {
  return _syscall1(SYS_CLOSE, fd);
}

int32_t write(int32_t fd, const void* buf, uint32_t count) {
  return _syscall3(SYS_WRITE, fd, buf, count);
}

int32_t read(int32_t fd, void* buf, uint32_t count) {
  return _syscall3(SYS_READ, fd, buf, count);
}

int32_t lseek(int32_t fd, int32_t offset, uint8_t whence) {
  return _syscall3(SYS_LSEEK, fd, offset, whence);
}

int32_t unlink(const char* pathname) {
  return _syscall1(SYS_UNLINK, pathname);
}

int32_t mkdir(const char* pathname) {
  return _syscall1(SYS_MKDIR, pathname);
}

struct dir* opendir(const char* name) {
  return (struct dir*)_syscall1(SYS_OPENDIR, name);
}

int32_t closedir(struct dir* dir) {
  return _syscall1(SYS_CLOSEDIR, dir);
}

int32_t rmdir(const char* pathname) {
  return _syscall1(SYS_RMDIR, pathname);
}

struct dir_entry* readdir(struct dir* dir) {
  return (struct dir_entry*)_syscall1(SYS_READDIR, dir);
}

void rewinddir(struct dir* dir) {
  _syscall1(SYS_REWINDDIR, dir);
}

int32_t stat(const char* path, struct stat* buf) {
  return _syscall2(SYS_STAT, path, buf);
}

char* getcwd(char* buf, uint32_t size) {
  return (char*)_syscall2(SYS_GETCWD, buf, size);
}

int32_t chdir(const char* path) {
  return _syscall1(SYS_CHDIR, path);
}

void ps(void) {
  _syscall0(SYS_PS);
}

int execv(const char* pathname, char** argv) {
  return _syscall2(SYS_EXECV, pathname, argv);
}

void exit(int32_t status) {
  _syscall1(SYS_EXIT, status);
}

int16_t wait(int32_t* status) {
  return _syscall1(SYS_WAIT, status);
}

int32_t pipe(int32_t pipefd[2]) {
  return _syscall1(SYS_PIPE, pipefd);
}

void dup2(uint32_t new_fd, uint32_t old_fd) {
  _syscall2(SYS_PIPE, new_fd, old_fd);
}
