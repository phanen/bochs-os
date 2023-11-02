#ifndef __FS_INODE_H
#define __FS_INODE_H

#include "stdint.h"
#include "list.h"
#include "ide.h"
#include "fs.h"

#define DIRECT_PTRS 12
#define INDIRE_PTRS 1
#define TOTAL_PTRS (DIRECT_PTRS + INDIRE_PTRS)

// num of blk ptrs 12+128
#define TWO_PTRS (INDIRE_PTRS * SECTOR_SIZE / sizeof(uint32_t))
#define FLATTEN_PTRS (DIRECT_PTRS + TWO_PTRS)

struct inode {
	uint32_t i_no; // self-reference
	uint32_t i_size; // file size

	uint32_t i_open_cnts; // cur file open counters
	bool write_deny; // one writer only

	// i_sectors[0-11] direct, i_sectors[12] indirect
	uint32_t i_sectors[TOTAL_PTRS];
	struct list_elem inode_tag;
};

struct inode *inode_open(struct partition *part, uint32_t inode_no);
void inode_close(struct inode *inode);
void inode_sync(struct partition *part, struct inode *inode, void *io_buf);
void inode_init(uint32_t inode_no, struct inode *new_inode);

// erase bitmap (inode's and its blocks')
void inode_release(struct partition *part, uint32_t inode_no);

#endif
