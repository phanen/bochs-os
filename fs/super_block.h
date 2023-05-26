#ifndef __FS_SUPER_BLOCK_H
#define __FS_SUPER_BLOCK_H

#include "stdint.h"

struct super_block {
   uint32_t magic;		// like type
   uint32_t sec_cnt;		// all secs in current partition
   uint32_t inode_cnt;		// all inodes in current partition
   uint32_t part_lba_base;

   uint32_t block_bitmap_lba;	// lba base
   uint32_t block_bitmap_sects; // cnt

   uint32_t inode_bitmap_lba;
   uint32_t inode_bitmap_sects;

   uint32_t inode_table_lba;
   uint32_t inode_table_sects;

   uint32_t data_start_lba;     // partition = meta + data
   uint32_t root_inode_no;	// where the dream start from
   uint32_t dir_entry_size;

   uint8_t  pad[460];		// pad to 512B (1sec)
} __attribute__ ((packed));

#endif
