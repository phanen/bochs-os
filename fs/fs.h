#ifndef __FS_FS_H
#define __FS_FS_H

#include "stdint.h"
#include "ide.h"

#define MAX_FILES_PER_PART 4096	    // limit
#define BITS_PER_SECTOR 4096
#define SECTOR_SIZE 512
#define BLOCK_SIZE SECTOR_SIZE	    // block = sector

#define MAX_PATH_LEN 512

enum file_types {
   FT_UNKNOWN,
   FT_REGULAR,
   FT_DIRECTORY
};

struct path_search_record {
   char searched_path[MAX_PATH_LEN]; // full path searched
   struct dir* parent_dir;
   enum file_types file_type;
};

void fs_init(void);

int32_t path_depth_cnt(char* pathname);
#endif
