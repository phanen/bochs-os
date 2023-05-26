#ifndef __FS_FILE_H
#define __FS_FILE_H

#include "stdint.h"
#include "ide.h"
#include "dir.h"
#include "global.h"

// max file num, also max file open num
#define MAX_FILE_OPEN 32

struct file {
   uint32_t fd_pos;  // 0, size-1
   uint32_t fd_flag;
   struct inode* fd_inode;
};

extern struct file file_table[MAX_FILE_OPEN];

enum std_fd {
   stdin_no,
   stdout_no,
   stderr_no
};

int32_t get_free_slot_in_global(void);

int32_t pcb_fd_install(int32_t globa_fd);

#endif
