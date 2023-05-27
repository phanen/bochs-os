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
   for (; blk_i < DIRECT_PTRS; blk_i++) {
      all_blocks[blk_i] = pdir->inode->i_sectors[blk_i];
   }
   blk_i = 0;
   if (pdir->inode->i_sectors[DIRECT_PTRS]) { // indirect ptr
      ide_read(part->my_disk, pdir->inode->i_sectors[DIRECT_PTRS],
	       all_blocks + DIRECT_PTRS, 1);
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
      // TODO: avoid iterating all entries...
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

// write p_de entry into parent_dir(inode)
bool sync_dir_entry(struct dir* parent_dir, struct dir_entry* p_de, void* io_buf) {

   // FIXME: no cur_part
   struct inode* dir_inode = parent_dir->inode;
   uint32_t dir_size = dir_inode->i_size;
   uint32_t dir_entry_size = cur_part->sb->dir_entry_size;

   ASSERT(dir_size % dir_entry_size == 0);

   uint32_t dir_entrys_per_sec = (SECTOR_SIZE / dir_entry_size);
   int32_t blk_lba = -1;

   // flatten ptrs
   uint32_t all_blocks[FLATTEN_PTRS] = {0};
   for (uint8_t blk_i = 0; blk_i < DIRECT_PTRS; blk_i++) {
      all_blocks[blk_i] = dir_inode->i_sectors[blk_i];
   }
   struct dir_entry* dir_e = (struct dir_entry*)io_buf;

   // find free slot
   for (uint8_t blk_i = 0; blk_i < FLATTEN_PTRS; blk_i++) {
      int32_t blk_bm_i = -1;

      // find a free block ptr
      //    1. alloc a free block
      //    2. different cases
      //    3. write to disk
      if (all_blocks[blk_i] == 0) {

	 // alloc a blk, and sync blk bitmap
	 //	  race condition? may sync delay?
	 blk_lba = block_bitmap_alloc(cur_part);
	 if (blk_lba == -1) {
	    printk("alloc block bitmap for sync_dir_entry failed\n");
	    return false;
	 }
	 blk_bm_i = blk_lba - cur_part->sb->data_start_lba;
	 ASSERT(blk_bm_i != -1);
	 bitmap_sync(cur_part, blk_bm_i, BLOCK_BITMAP);
	 blk_bm_i = -1;

	 // for free direct ptr
	 if (blk_i < DIRECT_PTRS) {
	    dir_inode->i_sectors[blk_i] = blk_lba;
	    all_blocks[blk_i] = blk_lba;
	 }
	 // for free indirect ptr
	 else if (blk_i == DIRECT_PTRS) {
	    dir_inode->i_sectors[DIRECT_PTRS] = blk_lba;
	    // blk_lba = -1;
	    blk_lba = block_bitmap_alloc(cur_part);
	    if (blk_lba == -1) { // rollback
	       // free the indirect block
	       blk_bm_i = dir_inode->i_sectors[DIRECT_PTRS] - cur_part->sb->data_start_lba;
	       bitmap_set(&cur_part->block_bitmap, blk_bm_i, 0);
	       dir_inode->i_sectors[DIRECT_PTRS] = 0;
	       printk("alloc block bitmap for sync_dir_entry failed\n");
	       return false;
	    }
	    blk_bm_i = blk_lba - cur_part->sb->data_start_lba;
	    ASSERT(blk_bm_i != -1);
	    bitmap_sync(cur_part, blk_bm_i, BLOCK_BITMAP);
	    
	    // assign the flat ptr
	    all_blocks[DIRECT_PTRS] = blk_lba;

	    // write indirect tab to disk
	    //	     all_blocks + DIRECT_PTRS is the base of tab
	    ide_write(cur_part->my_disk, dir_inode->i_sectors[DIRECT_PTRS],
	       all_blocks + DIRECT_PTRS, 1);
	 }
	 // 2-direct
	 else {
	    all_blocks[blk_i] = blk_lba;
	    ide_write(cur_part->my_disk, dir_inode->i_sectors[DIRECT_PTRS],
	       all_blocks + DIRECT_PTRS, 1);
	 }
	 // write entry -> mem block -> disk block
	 memset(io_buf, 0, 512);
	 memcpy(io_buf, p_de, dir_entry_size);
	 ide_write(cur_part->my_disk, all_blocks[blk_i], io_buf, 1);
	 dir_inode->i_size += dir_entry_size;
	 return true;
      }

      // find a non-free block ptr (try to find empty entry in it)
      ide_read(cur_part->my_disk, all_blocks[blk_i], io_buf, 1);
      for (uint8_t dir_entry_i = 0; dir_entry_i < dir_entrys_per_sec; dir_entry_i++) {
	 if ((dir_e + dir_entry_i)->f_type == FT_UNKNOWN) { // empty
	    memcpy(dir_e + dir_entry_i, p_de, dir_entry_size);
	    ide_write(cur_part->my_disk, all_blocks[blk_i], io_buf, 1);
	    dir_inode->i_size += dir_entry_size;
	    return true;
	 } 
      }
   }
   // no free entry
   printk("directory is full!\n");
   return false;
}
