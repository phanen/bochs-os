#ifndef __FS_DIR_H
#define __FS_DIR_H

#include "stdint.h"
#include "inode.h"
#include "ide.h"
#include "global.h"
#include "fs.h"

#define MAX_FILE_NAME_LEN  16

struct dir {
   struct inode* inode;
   uint32_t dir_pos;	  // offset in dir
   uint8_t dir_buf[512];  // date cache
};

struct dir_entry {
   char filename[MAX_FILE_NAME_LEN];  // name of regular file or dir
   uint32_t i_no;		      // map name to inode number (id)
   enum file_types f_type;	      // type is in dir
};

#endif
