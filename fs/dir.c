#include "dir.h"
#include "stdint.h"
#include "inode.h"
#include "file.h"
#include "fs.h"
#include "stdio-kernel.h"
#include "global.h"
#include "debug.h"
#include "memory.h"
#include "string.h"
#include "interrupt.h"
#include "super_block.h"

// num of blk ptrs 12+128
#define FLATTEN_PTRS 140

struct dir root_dir;

void open_root_dir(struct partition* part) {
   root_dir.inode = inode_open(part, part->sb->root_inode_no);
   root_dir.dir_pos = 0;
}

// dir inode_no -> dir inode
struct dir* dir_open(struct partition* part, uint32_t inode_no) {
   struct dir* pdir = (struct dir*)sys_malloc(sizeof(struct dir));
   pdir->inode = inode_open(part, inode_no);
   pdir->dir_pos = 0;
   return pdir;
}

void dir_close(struct dir* dir) {
   // never close root dir:
   //	 root_dir in stack, cannot free
   //	 root_dir <1M, cannot be free
   //	 root_dir should be open, anyway
   if (dir == &root_dir) {
      return;
   }
   inode_close(dir->inode);
   sys_free(dir);
}

// filename dir -> dir entry
bool search_dir_entry(struct partition* part, struct dir* pdir, \
		      const char* name, struct dir_entry* dir_e) {
   // buf to get all flatten ptrs
   uint32_t* all_blocks = (uint32_t*)sys_malloc(FLATTEN_PTRS * sizeof(uint32_t));
   if (all_blocks == NULL) {
      printk("search_dir_entry: sys_malloc for all_blocks failed");
      return false;
   }

   // get all flatten ptrs
   uint32_t blk_i = 0;
   for (; blk_i < 12; blk_i++) {
      all_blocks[blk_i] = pdir->inode->i_sectors[blk_i];
   }
   blk_i = 0;
   if (pdir->inode->i_sectors[12] != 0) {
      ide_read(part->my_disk, pdir->inode->i_sectors[12], all_blocks + 12, 1);
   }

   // blk buf
   uint8_t* buf = (uint8_t*)sys_malloc(SECTOR_SIZE);
   uint32_t dir_entry_size = part->sb->dir_entry_size;
   //	 ensure dir entry never cross secs
   uint32_t dir_entry_cnt = SECTOR_SIZE / dir_entry_size;

   // foreach blk
   for (; blk_i < FLATTEN_PTRS; blk_i++) {
      if (all_blocks[blk_i] == 0) { // NULL ptr
	 continue;
      }
      ide_read(part->my_disk, all_blocks[blk_i], buf, 1);

      struct dir_entry* p_de = (struct dir_entry*)buf;
      uint32_t dir_entry_i = 0;
      // FIXME: dir_entry_cnt?
      for (; dir_entry_i < dir_entry_cnt; p_de++, dir_entry_i++) {
	 if (!strcmp(p_de->filename, name)) {
	    memcpy(dir_e, p_de, dir_entry_size);
	    sys_free(buf);
	    sys_free(all_blocks);
	    return true;
	 }
      }
      memset(buf, 0, SECTOR_SIZE);
   }
   sys_free(buf);
   sys_free(all_blocks);
   return false;
}

void create_dir_entry(char* filename, uint32_t inode_no, uint8_t file_type, struct dir_entry* p_de) {
   ASSERT(strlen(filename) <=  MAX_FILE_NAME_LEN);
   memcpy(p_de->filename, filename, strlen(filename));
   p_de->i_no = inode_no;
   p_de->f_type = file_type;
}
