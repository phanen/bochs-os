#ifndef __FS_INODE_H
#define __FS_INODE_H

#include "stdint.h"
#include "list.h"
#include "ide.h"

struct inode {
   uint32_t i_no; // self-reference
   uint32_t i_size; // file size

   uint32_t i_open_cnts;   // cur file open counters
   bool write_deny;	   // one writer only

   // i_sectors[0-11] direct, i_sectors[12] indirect
   uint32_t i_sectors[13];
   struct list_elem inode_tag;
};

struct inode* inode_open(struct partition* part, uint32_t inode_no);
void inode_close(struct inode* inode);
void inode_sync(struct partition* part, struct inode* inode, void* io_buf);
void inode_init(uint32_t inode_no, struct inode* new_inode);

#endif
