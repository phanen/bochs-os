#ifndef __FS_INODE_H
#define __FS_INODE_H

#include "stdint.h"
#include "list.h"

struct inode {
   uint32_t i_no;
   uint32_t i_size;
   //	 for file: size
   //	 for dir:  sum of entry size

   uint32_t i_open_cnts;   // cur file open counters
   bool write_deny;	   // one writer only

   // i_sectors[0-11] direct, i_sectors[12] indirect
   uint32_t i_sectors[13];
   struct list_elem inode_tag;
};

#endif
