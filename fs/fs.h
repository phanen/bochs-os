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

// file open flags (bitmap)
enum oflags {
   O_RDONLY,	  // read only
   O_WRONLY,	  // write only
   O_RDWR,	  // read/write
   O_CREAT = 4	  // create
};

enum whence {
   SEEK_SET = 1,
   SEEK_CUR,
   SEEK_END
};

extern struct partition* cur_part;

void fs_init(void);

int32_t path_depth_cnt(char* pathname);

int32_t sys_open(const char* pathname, uint8_t flags);
int32_t sys_close(int32_t fd);

int32_t sys_write(int32_t fd, const void* buf, uint32_t count);
int32_t sys_read(int32_t fd, void* buf, uint32_t count);

#endif
