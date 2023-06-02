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

   //
   uint8_t local_fd = 3;
   // find a free slot, then map it
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

// create file in given path, return fd
//    this API does't consider filename rep !!!
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
   int fd_i = get_free_slot_in_global();
   if (fd_i == -1) {
      printk("exceed max open files\n");
      rollback_step = 2;
      goto rollback;
   }
   file_table[fd_i].fd_inode = new_file_inode;
   file_table[fd_i].fd_pos = 0;
   file_table[fd_i].fd_flag = flag;
   file_table[fd_i].fd_inode->write_deny = false;

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
   //	 sync pdir's inode (`pid->inode->i_size` changed)
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
   return pcb_fd_install(fd_i);

   // powerful paradigm
rollback:
   switch (rollback_step) {
      case 3: // free the ft entry
	 memset(&file_table[fd_i], 0, sizeof(struct file));
      case 2: // free the file inode
	 sys_free(new_file_inode);
      case 1: // free the bit
	 bitmap_set(&cur_part->inode_bitmap, inode_no, 0);
	 break;
   }
   sys_free(io_buf);
   return -1;
}

// inode_no -> inode, fd
int32_t file_open(uint32_t inode_no, uint8_t flag) {
   // alloc a new global fd
   //	 1 file -> 1 inode -> (possible) n ft entry
   //	 1 global ft entry -> 1 local ft entry
   int fd_i = get_free_slot_in_global();
   if (fd_i == -1) {
      printk("exceed max open files\n");
      return -1;
   }
   file_table[fd_i].fd_inode = inode_open(cur_part, inode_no);
   file_table[fd_i].fd_pos = 0;	 // no keep pos
   file_table[fd_i].fd_flag = flag;
   bool* write_deny = &file_table[fd_i].fd_inode->write_deny;

   // if write, try access exclusively
   if (flag & O_WRONLY || flag & O_RDWR) {
      enum intr_status old_status = intr_disable(); // poor lock
      if (!(*write_deny)) { // no other proc
	 *write_deny = true;
	 intr_set_status(old_status);
      }
      else { // access by another proc
	 intr_set_status(old_status);
	 printk("file can`t be write now, try again later\n");
	 return -1;
      }
   }
   // no write flag (read only or create file)
   // just install global fd into local fd table
   return pcb_fd_install(fd_i);
}

// close file
//    clean it from global file table
//    free the inode if necessary
int32_t file_close(struct file* file) {
   if (file == NULL) {
      return -1;
   }
   file->fd_inode->write_deny = false;
   inode_close(file->fd_inode);
   file->fd_inode = NULL;
   return 0;
}

// bytes will always append to file (no matter where the fd_pos is)
// return bytes_written
int32_t file_write(struct file* file, const void* buf, uint32_t count) {

   if ((file->fd_inode->i_size + count) > (BLOCK_SIZE * FLATTEN_PTRS))	{
      // 512*140=71680
      printk("exceed max file_size %d bytes, write file failed\n",
	     (BLOCK_SIZE * FLATTEN_PTRS));
      return -1;
   }

   uint8_t* io_buf = sys_malloc(BLOCK_SIZE);
   if (io_buf == NULL) {
      printk("file_write: sys_malloc for io_buf failed\n");
      return -1;
   }

   uint32_t* all_blocks = (uint32_t*)sys_malloc(FLATTEN_PTRS * sizeof(uint32_t));
   if (all_blocks == NULL) {
      printk("file_write: sys_malloc for all_blocks failed\n");
      return -1;
   }

   const uint8_t* src = buf;	    // remain part to be written
   uint32_t bytes_written = 0;	    // been written
   uint32_t size_left = count;	    // to be written

   int32_t block_lba = -1;
   uint32_t block_bitmap_idx = 0;

   // if write a empty file, then alloc a block first
   if (file->fd_inode->i_sectors[0] == 0) {
      block_lba = block_bitmap_alloc(cur_part);
      if (block_lba == -1) {
	 printk("file_write: block_bitmap_alloc failed\n");
	 return -1;
      }
      file->fd_inode->i_sectors[0] = block_lba;
      block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
      ASSERT(block_bitmap_idx != 0);
      bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
   }

   // current size
   uint32_t file_has_used_blocks = file->fd_inode->i_size / BLOCK_SIZE + 1;
   // future size
   uint32_t file_will_use_blocks = (file->fd_inode->i_size + count) / BLOCK_SIZE + 1;
   ASSERT(file_will_use_blocks <= 140);
   // diff
   uint32_t add_blocks = file_will_use_blocks - file_has_used_blocks;

   uint32_t blk_i;
   int32_t indirect_block_table;

   if (add_blocks == 0) {
      if (file_has_used_blocks <= 12) {	// direct only
	 blk_i = file_has_used_blocks - 1;
	 all_blocks[blk_i] = file->fd_inode->i_sectors[blk_i];
	 // other ptr all are NULL
      }
      else { // need indirect
	 ASSERT(file->fd_inode->i_sectors[12] != 0);
	 indirect_block_table = file->fd_inode->i_sectors[12];
	 ide_read(cur_part->my_disk, indirect_block_table, all_blocks + 12, 1);
      }
   }
   else {
      // need alloc new direct block only
      if (file_will_use_blocks <= 12) {
	 // get current block ptr
	 blk_i = file_has_used_blocks - 1;
	 ASSERT(file->fd_inode->i_sectors[blk_i] != 0);
	 all_blocks[blk_i] = file->fd_inode->i_sectors[blk_i];

	 // alloc new block ptr, and get it

	 for (blk_i = file_has_used_blocks; blk_i < file_will_use_blocks; blk_i++) {
	    block_lba = block_bitmap_alloc(cur_part);
	    if (block_lba == -1) {
	       printk("file_write: block_bitmap_alloc for situation 1 failed\n");
	       return -1;
	    }

	    //	     never: used block size < alloc blocks size
	    //	     we will set ptr to 0 when file delete
	    ASSERT(file->fd_inode->i_sectors[blk_i] == 0);
	    file->fd_inode->i_sectors[blk_i] = all_blocks[blk_i] = block_lba;
	    block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
	    bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
	 }
      }
      // need alloc new indirect table block first
      else if (file_will_use_blocks > 12 && file_has_used_blocks <= 12) {
	 blk_i = file_has_used_blocks - 1;
	 all_blocks[blk_i] = file->fd_inode->i_sectors[blk_i];

	 // alloc indirect table block
	 block_lba = block_bitmap_alloc(cur_part);
	 if (block_lba == -1) {
	    printk("file_write: block_bitmap_alloc for situation 2 failed\n");
	    return -1;
	 }
	 ASSERT(file->fd_inode->i_sectors[12] == 0);
	 indirect_block_table = file->fd_inode->i_sectors[12] = block_lba;


	 for (blk_i = file_has_used_blocks; blk_i < file_will_use_blocks; blk_i++) {
	    block_lba = block_bitmap_alloc(cur_part);
	    if (block_lba == -1) {
	       printk("file_write: block_bitmap_alloc for situation 2 failed\n");
	       return -1;
	    }

	    if (blk_i < 12) {
	       ASSERT(file->fd_inode->i_sectors[blk_i] == 0);
	       file->fd_inode->i_sectors[blk_i] = block_lba;
	       all_blocks[blk_i] = block_lba;
	    }
	    else {
	       all_blocks[blk_i] = block_lba;
	    }
	    block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
	    bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
	 }
	 // sync indirect table
	 ide_write(cur_part->my_disk, indirect_block_table, all_blocks + 12, 1);
      }
	 // need to alloc indirect blocks only
      else if (file_has_used_blocks > 12) {
	 ASSERT(file->fd_inode->i_sectors[12] != 0);
	 indirect_block_table = file->fd_inode->i_sectors[12];
	 ide_read(cur_part->my_disk, indirect_block_table, all_blocks + 12, 1);

	 // alloc
	 blk_i = file_has_used_blocks;
	 for (blk_i = file_has_used_blocks; blk_i < file_will_use_blocks; blk_i++) {
	    block_lba = block_bitmap_alloc(cur_part);
	    if (block_lba == -1) {
	       printk("file_write: block_bitmap_alloc for situation 3 failed\n");
	       return -1;
	    }
	    all_blocks[blk_i] = block_lba;
	    block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
	    bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
	 }
	 // sync indirec table
	 ide_write(cur_part->my_disk, indirect_block_table, all_blocks + 12, 1);
      }
   }

   uint32_t sec_idx;
   uint32_t sec_lba;
   uint32_t sec_off_bytes;
   uint32_t sec_left_bytes;
   uint32_t chunk_size;
   bool first_write_block = true; // need sync
   // ptr map has been build in all_blocks
   // just write it
   file->fd_pos = file->fd_inode->i_size - 1; // set pos to append
   while (bytes_written < count) {
      memset(io_buf, 0, BLOCK_SIZE);

      // calculate start point
      //    dividable since 2rd loop
      sec_idx = file->fd_inode->i_size / BLOCK_SIZE;
      sec_lba = all_blocks[sec_idx];
      sec_off_bytes = file->fd_inode->i_size % BLOCK_SIZE;
      sec_left_bytes = BLOCK_SIZE - sec_off_bytes; // in a block

      // calculate size to write
      chunk_size = size_left < sec_left_bytes ? size_left : sec_left_bytes;
      if (first_write_block) {
	 ide_read(cur_part->my_disk, sec_lba, io_buf, 1);
	 first_write_block = false;
      }
      memcpy(io_buf + sec_off_bytes, src, chunk_size);
      ide_write(cur_part->my_disk, sec_lba, io_buf, 1);
      // printk("file write at lba 0x%x\n", sec_lba); // debug

      // update
      src += chunk_size;   // remain pointer
      file->fd_inode->i_size += chunk_size;  // update file size
      file->fd_pos += chunk_size; // emm...
      bytes_written += chunk_size;
      size_left -= chunk_size;
   }

   inode_sync(cur_part, file->fd_inode, io_buf);
   sys_free(all_blocks);
   sys_free(io_buf);
   return bytes_written;
}

// read bytes starting from fd_pos
// return bytes_written
int32_t file_read(struct file* file, void* buf, uint32_t count) {
   uint8_t* buf_dst = (uint8_t*)buf;
   uint32_t size = count, size_left = size;

   // bound check
   if ((file->fd_pos + count) > file->fd_inode->i_size)	{
      size = file->fd_inode->i_size - file->fd_pos;
      size_left = size;
      if (size == 0) {
	 return -1;
      }
   }

   uint8_t* io_buf = sys_malloc(BLOCK_SIZE);
   if (io_buf == NULL) {
      printk("file_read: sys_malloc for io_buf failed\n");
   }
   uint32_t* all_blocks = (uint32_t*)sys_malloc(FLATTEN_PTRS * sizeof(uint32_t));
   if (all_blocks == NULL) {
      printk("file_read: sys_malloc for all_blocks failed\n");
      return -1;
   }

   uint32_t block_read_start_idx = file->fd_pos / BLOCK_SIZE;
   uint32_t block_read_end_idx = (file->fd_pos + size) / BLOCK_SIZE;
   uint32_t read_blocks = block_read_start_idx - block_read_end_idx;
   // TODO: 0 - 138?? 139??
   ASSERT(block_read_start_idx < 139 && block_read_end_idx < 139);

   int32_t indirect_block_table;
   uint32_t block_idx;

   if (read_blocks == 0) {
      ASSERT(block_read_end_idx == block_read_start_idx);
      if (block_read_end_idx < 12) {
	 block_idx = block_read_end_idx;
	 all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
      }
      else {
	 indirect_block_table = file->fd_inode->i_sectors[12];
	 ide_read(cur_part->my_disk, indirect_block_table, all_blocks + 12, 1);
      }
   }
      // need alloc
   else {
      // read only direct
      if (block_read_end_idx < 12) {
	 block_idx = block_read_start_idx;
	 while (block_idx <= block_read_end_idx) {
	    all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
	    block_idx++;
	 }
      }
	 // read direct and need load tab
      else if (block_read_start_idx < 12 && block_read_end_idx >= 12) {
	 block_idx = block_read_start_idx;
	 while (block_idx < 12) {
	    all_blocks[block_idx] = file->fd_inode->i_sectors[block_idx];
	    block_idx++;
	 }
	 ASSERT(file->fd_inode->i_sectors[12] != 0);

	 indirect_block_table = file->fd_inode->i_sectors[12];
	 ide_read(cur_part->my_disk, indirect_block_table, all_blocks + 12, 1);
      }
	 // need load tab
      else {
	 ASSERT(file->fd_inode->i_sectors[12] != 0);
	 indirect_block_table = file->fd_inode->i_sectors[12];
	 ide_read(cur_part->my_disk, indirect_block_table, all_blocks + 12, 1);
      }
   }

   uint32_t sec_idx, sec_lba, sec_off_bytes, sec_left_bytes, chunk_size;
   uint32_t bytes_read = 0;
   while (bytes_read < size) {
      sec_idx = file->fd_pos / BLOCK_SIZE;
      sec_lba = all_blocks[sec_idx];
      sec_off_bytes = file->fd_pos % BLOCK_SIZE;
      sec_left_bytes = BLOCK_SIZE - sec_off_bytes;
      chunk_size = size_left < sec_left_bytes ? size_left : sec_left_bytes;

      memset(io_buf, 0, BLOCK_SIZE);
      ide_read(cur_part->my_disk, sec_lba, io_buf, 1);
      memcpy(buf_dst, io_buf + sec_off_bytes, chunk_size);

      buf_dst += chunk_size;
      file->fd_pos += chunk_size;
      bytes_read += chunk_size;
      size_left -= chunk_size;
   }
   sys_free(all_blocks);
   sys_free(io_buf);
   return bytes_read;
}
