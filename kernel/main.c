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

void k_thread_a(void*);
void k_thread_b(void*);
void k_thread_c(void*);
void k_thread_d(void*);

void u_prog_a(void);
void u_prog_b(void);
void u_prog_c(void);
void u_prog_d(void);

int bss_var_a = 0, bss_var_b = 0;

int main() {

  put_str("you are in kernel now\n");
  init_all();

  // process_execute(u_prog_a, "user_prog_a");
  // process_execute(u_prog_b, "user_prog_b");
  // process_execute(u_prog_c, "user_prog_c");
  // process_execute(u_prog_d, "user_prog_d");
  // intr_enable();
  // thread_create("k_thread_a", 31, k_thread_a, "thread_a");
  // thread_create("k_thread_b", 31, k_thread_b, "thread_b");
  // thread_create("k_thread_c", 31, k_thread_c, "thread_c");
  // thread_create("k_thread_d", 31, k_thread_d, "thread_d");
  // console_put_str(" main_pid:0x");
  // console_put_int(sys_getpid());
  // console_put_char('\n');

  // void* addr = sys_malloc(33);
  // printk("%s, pid:%d addr:0x%x %c", "main", sys_getpid(), (int)addr, '\n');
  // process_execute(u_prog_a, "u_prog_a");
  // process_execute(u_prog_b, "u_prog_b");
  // thread_create("k_thread_a", 31, k_thread_a, "I am thread_a");
  // thread_create("k_thread_b", 31, k_thread_b, "I am thread_b");

  // sys_open("/file1", O_CREAT);
  uint32_t fd = sys_open("/file1", O_RDWR);
  printf("fd:%d\n", fd);
  sys_write(fd, "hello,world\n", 12);
  sys_close(fd);
  printf("%d closed now\n", fd);  while(1);
  return 0;
}

void k_thread_c(void* arg) {     
  char* para = arg;
  void* addr1;
  void* addr2;
  void* addr3;
  void* addr4;
  void* addr5;
  void* addr6;
  void* addr7;
  console_put_str("  thread_a start\n");
  int max = 10;
  while (max-- > 0) {
    int size = 128;
    addr1 = sys_malloc(size); 
    size *= 2; 
    addr2 = sys_malloc(size); 
    size *= 2; 
    addr3 = sys_malloc(size);
    sys_free(addr1);
    addr4 = sys_malloc(size);
    size *= 2; size *= 2; size *= 2; size *= 2; 
    size *= 2; size *= 2; size *= 2; 
    addr5 = sys_malloc(size);
    addr6 = sys_malloc(size);
    sys_free(addr5);
    size *= 2; 
    addr7 = sys_malloc(size);
    sys_free(addr6);
    sys_free(addr7);
    sys_free(addr2);
    sys_free(addr3);
    sys_free(addr4);
  }
  console_put_str(" thread_a end\n");
  while(1);
}


void k_thread_d(void* arg) {     
  char* para = arg;
  void* addr1;
  void* addr2;
  void* addr3;
  void* addr4;
  void* addr5;
  void* addr6;
  void* addr7;
  void* addr8;
  void* addr9;
  int max = 10;
  console_put_str(" thread_b start\n");
  while (max-- > 0) {
    int size = 9;
    addr1 = sys_malloc(size);
    size *= 2; 
    addr2 = sys_malloc(size);
    size *= 2; 
    sys_free(addr2);
    addr3 = sys_malloc(size);
    sys_free(addr1);
    addr4 = sys_malloc(size);
    addr5 = sys_malloc(size);
    addr6 = sys_malloc(size);
    sys_free(addr5);
    size *= 2; 
    addr7 = sys_malloc(size);
    sys_free(addr6);
    sys_free(addr7);
    sys_free(addr3);
    sys_free(addr4);

    size *= 2; size *= 2; size *= 2; 
    addr1 = sys_malloc(size);
    addr2 = sys_malloc(size);
    addr3 = sys_malloc(size);
    addr4 = sys_malloc(size);
    addr5 = sys_malloc(size);
    addr6 = sys_malloc(size);
    addr7 = sys_malloc(size);
    addr8 = sys_malloc(size);
    addr9 = sys_malloc(size);
    sys_free(addr1);
    sys_free(addr2);
    sys_free(addr3);
    sys_free(addr4);
    sys_free(addr5);
    sys_free(addr6);
    sys_free(addr7);
    sys_free(addr8);
    sys_free(addr9);
  }
  console_put_str(" thread_b end\n");
  while(1);
}

void k_thread_a(void* arg) {
  char* para = arg;
  void* addr = sys_malloc(31);
  printk("%s, pid:%d addr:0x%x %c", para, sys_getpid(), (int)addr, '\n');
  while (1) {
  }
}

void k_thread_b(void* arg) {

  char* para = arg;
  void* addr = sys_malloc(63);
  printk("%s, pid:%d addr:0x%x %c", para, sys_getpid(), (int)addr, '\n');
  while (1) {
  }
}

void u_prog_a(void) {
  char* name = "uprog_a";
  printf("%s, pid:%d %c", name, getpid(),'\n');
  while(1) {
    bss_var_a = getpid();
  }

}

void u_prog_b(void) {
  char* name = "uprog_b";
  printf("%s, pid:%d %c", name, getpid(),'\n');
  while(1) {
    bss_var_b = getpid();
  }
}

void u_prog_c(void) {
  void* addr1 = malloc(256);
  void* addr2 = malloc(255);
  void* addr3 = malloc(254);
  printf(" prog_a malloc addr:0x%x,0x%x,0x%x\n", (int)addr1, (int)addr2, (int)addr3);

  int cpu_delay = 1000;
  while(cpu_delay-- > 0);
  free(addr1);
  free(addr2);
  free(addr3);
  while(1);
}

void u_prog_d(void) {
  void* addr1 = malloc(256);
  void* addr2 = malloc(255);
  void* addr3 = malloc(254);
  printf(" prog_b malloc addr:0x%x,0x%x,0x%x\n", (int)addr1, (int)addr2, (int)addr3);

  int cpu_delay = 1000;
  while(cpu_delay-- > 0);
  free(addr1);
  free(addr2);
  free(addr3);
  while(1);
}
