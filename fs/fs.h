#ifndef __FS_FS_H
#define __FS_FS_H

#include "stdint.h"
#include "ide.h"

#define MAX_FILES_PER_PART 4096	    // limit
#define BITS_PER_SECTOR 4096
#define SECTOR_SIZE 512
#define BLOCK_SIZE SECTOR_SIZE	    // block = sector

enum file_types {
   FT_UNKNOWN,
   FT_REGULAR,
   FT_DIRECTORY
};
void fs_init(void);

#endif