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

#include "shell.h"

void init();

void loadelf2fs(uint32_t sec_base, uint32_t file_size, char* filename);

int main() {

   put_str("you are in kernel now\n");
   init_all();
   cls_screen();

   loadelf2fs(300, 13888, "/hello");
   loadelf2fs(400, 18028, "/fork-exec");

   console_put_str("[phanium@bochs /]$ ");
   // shell_run();
   while (1);
   return 0;
}

void loadelf2fs(uint32_t sec_base, uint32_t file_size, char* filename) {

   uint32_t sec_cnt = DIV_ROUND_UP(file_size, SECTOR_SIZE);
   struct disk* sda = &channels[0].devices[0];

   void* buf = sys_malloc(file_size);
   ide_read(sda, sec_base, buf, sec_cnt);

   int32_t fd = sys_open(filename, O_CREAT|O_RDWR);
   if (fd == -1) {
      printk("open error");
      return;
   }

   if(sys_write(fd, buf, file_size) == -1) {
      printk("file write error!\n");
      while(1);
   }
}

void init(void) {
   pid_t ret_pid = fork();
   if(ret_pid) {
      while (1);
   } else {
      shell_run();
   }
   while(1);
}
