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

void init();

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

  // // sys_open("/file1", O_CREAT);
  //
  // // create
  // uint32_t fd = sys_open("/file1", O_CREAT);
  // printf("create fd:%d\n", fd);
  // sys_close(fd);
  //
  // // write
  // fd = sys_open("/file1", O_RDWR);
  // printf("write fd:%d\n", fd);
  // sys_write(fd, "hello,world\n", 12);
  // sys_write(fd, "hello,world\n", 12);
  // sys_close(fd);
  // printf("%d closed now\n", fd);
  //
  // // read
  // fd = sys_open("/file1", O_RDWR);
  // printf("read fd:%d\n", fd);
  // printf("open /file1, fd:%d\n", fd);
  //
  // char buf[64] = {0};
  // int read_bytes = sys_read(fd, buf, 18);
  // // hello,world\nhello
  // printf("1_ read %d bytes:\n%s\n", read_bytes, buf);
  //
  // memset(buf, 0, 64);
  // read_bytes = sys_read(fd, buf, 6);
  // // world\n
  // printf("2_ read %d bytes:\n%s", read_bytes, buf);
  //
  // memset(buf, 0, 64);
  // // nothing
  // read_bytes = sys_read(fd, buf, 6);
  // printf("3_ read %d bytes:\n%s", read_bytes, buf);
  //
  // // reset fd_pos by close and reopen
  // // printf("________  close file1 and reopen  ________\n");
  // // sys_close(fd);
  //
  // // reset fd_pos by lseek
  // sys_lseek(fd, 0, SEEK_SET);
  // memset(buf, 0, 64);
  // read_bytes = sys_read(fd, buf, 24);
  // printf("4_ read %d bytes:\n%s", read_bytes, buf);
  //
  // sys_close(fd);

  printf("/file1 delete %s!\n", sys_unlink("/file1") == 0 ? "done" : "fail");

  while(1);
  return 0;
}


void init(void) {
   pid_t ret_pid = fork();
   if(ret_pid) {
      printf("i am father, my pid is %d, child pid is %d\n", getpid(), ret_pid);
   } else {
      printf("i am child, my pid is %d, ret pid is %d\n", getpid(), ret_pid);
   }
   while(1);
}
