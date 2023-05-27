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

// create file in given path
// return fd
//    create inode -> create fd -> create dir_e
int32_t file_create(struct dir* parent_dir, char* filename, uint8_t flag) {
   void* io_buf = sys_malloc(1024);
   if (io_buf == NULL) {
      printk("in file_creat: sys_malloc for io_buf failed\n");
      return -1;
   }

   uint8_t rollback_step = 0;

   // alloc inode 
   //	 alloc inode no -> alloc inode -> associate them
   int32_t inode_no = inode_bitmap_alloc(cur_part);
   if (inode_no == -1) {
      printk("in file_creat: allocate inode failed\n");
      return -1;
   }
   struct inode* new_file_inode = (struct inode*)sys_malloc(sizeof(struct inode));
   if (new_file_inode == NULL) {
      printk("file_create: sys_malloc for inode failded\n");
      rollback_step = 1;
      goto rollback;
   }
   inode_init(inode_no, new_file_inode);

   // alloc global fd in file_table
   // then associate global fd with inode
   int fd_idx = get_free_slot_in_global();
   if (fd_idx == -1) {
      printk("exceed max open files\n");
      rollback_step = 2;
      goto rollback;
   }
   file_table[fd_idx].fd_inode = new_file_inode;
   file_table[fd_idx].fd_pos = 0;
   file_table[fd_idx].fd_flag = flag;
   file_table[fd_idx].fd_inode->write_deny = false;

   // alloc dir entry
   struct dir_entry new_dir_entry;
   memset(&new_dir_entry, 0, sizeof(struct dir_entry));
   create_dir_entry(filename, inode_no, FT_REGULAR, &new_dir_entry);

   // write all to disk
   //	 sync dir_e block
   if (!sync_dir_entry(parent_dir, &new_dir_entry, io_buf)) {
      printk("sync dir_entry to disk failed\n");
      rollback_step = 3;
      goto rollback;
   }

   //	 sync dir inode
   memset(io_buf, 0, 1024);
   inode_sync(cur_part, parent_dir->inode, io_buf);

   //	 sync file inode
   memset(io_buf, 0, 1024);
   inode_sync(cur_part, new_file_inode, io_buf);

   //	 sync inode_bitmap
   bitmap_sync(cur_part, inode_no, INODE_BITMAP);

   //	 add inode_tag to list
   list_push(&cur_part->open_inodes, &new_file_inode->inode_tag);
   new_file_inode->i_open_cnts = 1;

   sys_free(io_buf);
   return pcb_fd_install(fd_idx);

   // powerful paradigm
rollback:
   switch (rollback_step) {
      case 3: // free the ft entry
	 memset(&file_table[fd_idx], 0, sizeof(struct file));
      case 2: // free the file inode
	 sys_free(new_file_inode);
      case 1: // free the bit
	 bitmap_set(&cur_part->inode_bitmap, inode_no, 0);
	 break;
   }
   sys_free(io_buf);
   return -1;
}
