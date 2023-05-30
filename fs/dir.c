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

// find file(dir_e) by name in current dir
//    return when first match name (no matter what file type)
//    `dir_e` should be alloc before
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

// write p_de entry into parent_dir
//    sync the block (and inode, if alloc new bloc)
// FIXME: this implement is buggy
bool sync_dir_entry(struct dir* parent_dir, struct dir_entry* p_de, void* io_buf) {

   struct inode* dir_inode = parent_dir->inode;
   uint32_t dir_size = dir_inode->i_size;
   uint32_t dir_entry_size = cur_part->sb->dir_entry_size;

   ASSERT(dir_size % dir_entry_size == 0);

   uint32_t dir_entrys_per_sec = (SECTOR_SIZE / dir_entry_size);
   int32_t blk_lba = -1;

   // flatten ptrs (140 * 4 < PAGE_SIZE though...)
   uint32_t all_blocks[FLATTEN_PTRS] = {0};
   for (uint8_t blk_i = 0; blk_i < DIRECT_PTRS; blk_i++) {
      all_blocks[blk_i] = dir_inode->i_sectors[blk_i];
   }
   struct dir_entry* dir_e = (struct dir_entry*)io_buf;

   // find free dir_entry slot
   uint8_t blk_i = 0;
   bool has_flatten = false;
   while (blk_i < FLATTEN_PTRS) {
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
	 else if (blk_i == DIRECT_PTRS && !has_flatten) {
	    // we use pre-alloc block as indirect tab
	    dir_inode->i_sectors[DIRECT_PTRS] = blk_lba;

	    // then alloc a new block
	    blk_lba = -1;
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

	    // build a indirect tab in mem in-place
	    // then update the indirect tab into disk
	    all_blocks[DIRECT_PTRS] = blk_lba;
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

      // no-free direct block and the indirec block has been alloc
      // then load indirect block
      if (blk_i == DIRECT_PTRS) {
	 ide_read(cur_part->my_disk, dir_inode->i_sectors[DIRECT_PTRS],
	   all_blocks + DIRECT_PTRS, 1);
	 has_flatten = true;
	 continue;
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
      blk_i++;
   }
   // no free entry
   printk("directory is full!\n");
   return false;
}

// delete entry in pdir with inode_no
bool delete_dir_entry(struct partition* part, struct dir* pdir, uint32_t inode_no, void* io_buf) {

   struct inode* dir_inode = pdir->inode;

   // paradigm: build a flatten ptrs array
   uint32_t blk_i = 0, all_blocks[FLATTEN_PTRS] = {0};
   while (blk_i < DIRECT_PTRS) {
      all_blocks[blk_i] = dir_inode->i_sectors[blk_i];
      blk_i++;
   }
   if (dir_inode->i_sectors[DIRECT_PTRS]) {
      ide_read(part->my_disk,
	       dir_inode->i_sectors[DIRECT_PTRS], all_blocks + DIRECT_PTRS, 1);
   }

   // dir_e will not cross secs
   uint32_t dir_entry_size = part->sb->dir_entry_size;
   uint32_t dir_entrys_per_sec = (SECTOR_SIZE / dir_entry_size);
   struct dir_entry* dir_e = (struct dir_entry*)io_buf; // dir buf
   struct dir_entry* dir_entry_found = NULL;

   uint8_t dir_entry_idx, dir_entry_cnt;
   // cnt tell us if we can delete the block

   bool is_dir_first_block = false;

   blk_i = 0;
   while (blk_i < FLATTEN_PTRS) {
      is_dir_first_block = false;
      if (all_blocks[blk_i] == 0) {
	 blk_i++;
	 continue;
      }

      memset(io_buf, 0, SECTOR_SIZE);
      ide_read(part->my_disk, all_blocks[blk_i], io_buf, 1);

      dir_entry_idx = dir_entry_cnt = 0;
      // foreach entry
      while (dir_entry_idx < dir_entrys_per_sec) {
	 // exist
	 if ((dir_e + dir_entry_idx)->f_type != FT_UNKNOWN) {
	    if (!strcmp((dir_e + dir_entry_idx)->filename, ".")) {
	       // this dir contain '.', then it must be the first blk of entry
	       // emm... why not use blk_i == 0 ?
	       is_dir_first_block = true;
	    }
	    else if (strcmp((dir_e + dir_entry_idx)->filename, ".") &&
		  strcmp((dir_e + dir_entry_idx)->filename, "..")) {
	       dir_entry_cnt++;
	       if ((dir_e + dir_entry_idx)->i_no == inode_no) {
		  // never twice found
		  ASSERT(dir_entry_found == NULL);
		  dir_entry_found = dir_e + dir_entry_idx;
		  // gono, to get `cnt`
	       }
	    }
	 }
	 dir_entry_idx++;
      }

      if (dir_entry_found == NULL) {
	 blk_i++;
	 continue;
      }

      // found
      ASSERT(dir_entry_cnt >= 1);


      // delete the block
      //    if it's the first block(contain '.' '..'),
      //    we can not delete it even if it's empty
      if (dir_entry_cnt == 1 && !is_dir_first_block) {
	 // erase this block (bitmap)
	 uint32_t block_bitmap_idx = all_blocks[blk_i] - part->sb->data_start_lba;
	 bitmap_set(&part->block_bitmap, block_bitmap_idx, 0);
	 bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);

	 // direct blk
	 if (blk_i < DIRECT_PTRS) {
	    dir_inode->i_sectors[blk_i] = 0;
	 }
	    // indirect blk
	 else {
	    // num of indirect blks
	    uint32_t indirect_blocks = 0;
	    uint32_t indirect_block_idx = DIRECT_PTRS;
	    while (indirect_block_idx < FLATTEN_PTRS) {
	       if (all_blocks[indirect_block_idx] != 0) {
		  indirect_blocks++;
	       }
	    }
	    // at lease cur blk
	    ASSERT(indirect_blocks >= 1);

	    // erase only this indirect ptr in tab
	    if (indirect_blocks > 1) {
	       all_blocks[blk_i] = 0;
	       ide_write(part->my_disk,
		  dir_inode->i_sectors[DIRECT_PTRS], all_blocks + DIRECT_PTRS, 1);
	    }
	    // erase the whole indirect tab
	    else {
	       block_bitmap_idx = dir_inode->i_sectors[DIRECT_PTRS] - part->sb->data_start_lba;
	       bitmap_set(&part->block_bitmap, block_bitmap_idx, 0);
	       bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);
	       dir_inode->i_sectors[DIRECT_PTRS] = 0;
	    }
	 }
      }
	 // delete the entry
      else {
	 memset(dir_entry_found, 0, dir_entry_size);
	 ide_write(part->my_disk, all_blocks[blk_i], io_buf, 1);
      }

      // udpate inode
      ASSERT(dir_inode->i_size >= dir_entry_size);
      dir_inode->i_size -= dir_entry_size;
      memset(io_buf, 0, SECTOR_SIZE * 2);
      // `inode_sync` may cross secs
      inode_sync(part, dir_inode, io_buf);
      return true;
   }
   // no such inode(file), 
   //	 you may need check usage of `search_file`
   return false;
}
