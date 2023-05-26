#include "file.h"
#include "fs.h"
#include "super_block.h"
#include "inode.h"
#include "stdio-kernel.h"
#include "memory.h"
#include "debug.h"
#include "interrupt.h"
#include "string.h"
#include "thread.h"
#include "global.h"

struct file file_table[MAX_FILE_OPEN];

// get free slot in global file table
//	 linear search
int32_t get_free_slot_in_global(void) {
   uint32_t fd = 3;
   for (; fd < MAX_FILE_OPEN; fd++) {
      if (file_table[fd].fd_inode == NULL) {
	 break;
      }
   }
   if (fd == MAX_FILE_OPEN) {
      printk("exceed max open files\n");
      return -1;
   }
   return fd;
}

// map local_fd to global_fd (in pcb fd table)
int32_t pcb_fd_install(int32_t globa_fd) {
   struct task_struct* cur = running_thread();
   uint8_t local_fd = 3;
   for (; local_fd < MAX_FILES_OPEN_PER_PROC; local_fd++) {
      if (cur->fd_table[local_fd] == -1) {   //  -1 -> free
	 cur->fd_table[local_fd] = globa_fd;
	 break;
      }
   }
   if (local_fd == MAX_FILES_OPEN_PER_PROC) {
      printk("exceed max open files_per_proc\n");
      return -1;
   }
   return local_fd;
}
