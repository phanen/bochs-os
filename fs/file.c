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

// alloc inode, give a inode no
int32_t inode_bitmap_alloc(struct partition* part) {
   int32_t bit_i = bitmap_scan(&part->inode_bitmap, 1);
   if (bit_i == -1) {
      return -1;
   }
   bitmap_set(&part->inode_bitmap, bit_i, 1);
   return bit_i;
}

// alloc blk, give a sec lba
int32_t block_bitmap_alloc(struct partition* part) {
   int32_t bit_i = bitmap_scan(&part->block_bitmap, 1);
   if (bit_i == -1) {
      return -1;
   }
   bitmap_set(&part->block_bitmap, bit_i, 1);
   return (part->sb->data_start_lba + bit_i);
}

// sync the blk contains bitmap[bit_idx]
void bitmap_sync(struct partition* part, uint32_t bit_idx, uint8_t btmp_type) {
   uint32_t off_sec = bit_idx / BITS_PER_SECTOR;
   uint32_t off_size = off_sec * BLOCK_SIZE;
   uint32_t sec_lba;
   uint8_t* bitmap_off;

   switch (btmp_type) {
      case INODE_BITMAP:
	 sec_lba = part->sb->inode_bitmap_lba + off_sec;
	 bitmap_off = part->inode_bitmap.bits + off_size;
      break;

      case BLOCK_BITMAP:
	 sec_lba = part->sb->block_bitmap_lba + off_sec;
	 bitmap_off = part->block_bitmap.bits + off_size;
      break;
   }
   ide_write(part->my_disk, sec_lba, bitmap_off, 1);
}
