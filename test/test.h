#ifndef __TEST_H
#define __TEST_H

#include "print.h"
#include "init.h"
// #include "debug.h"
#include "thread.h"
#include "interrupt.h"
#include "console.h"
// #include "ioqueue.h"
// #include "keyboard.h"
#include "process.h"
#include "syscall.h"
#include "syscall-init.h"

#include "stdio.h"
#include "stdio-kernel.h"
#include "memory.h"
#include "fs.h"
#include "string.h"
#include "dir.h"

void k_thread_a(void *);
void k_thread_b(void *);
void k_thread_c(void *);
void k_thread_d(void *);

void u_prog_a(void);
void u_prog_b(void);
void u_prog_c(void);
void u_prog_d(void);

int bss_var_a = 0, bss_var_b = 0;

void test_file();
void test_dir();
void test_api_dir();

void test_cwd();
void test_stat();

void test_main();

#endif
