#ifndef __FS_FS_H
#define __FS_FS_H

#include "stdint.h"
// #include "ide.h"

#define MAX_FILES_PER_PART 4096 // limit
#define BITS_PER_SECTOR 4096
#define SECTOR_SIZE 512
#define BLOCK_SIZE SECTOR_SIZE // block = sector

#define MAX_PATH_LEN 512

enum file_types { FT_UNKNOWN, FT_REGULAR, FT_DIRECTORY };

struct path_search_record {
	char searched_path[MAX_PATH_LEN]; // full path searched
	struct dir *parent_dir;
	enum file_types file_type;
};

// file open flags (bitmap)
enum oflags {
	O_RDONLY, // read only
	O_WRONLY, // write only
	O_RDWR, // read/write
	O_CREAT = 4 // create
};

enum whence { SEEK_SET = 1, SEEK_CUR, SEEK_END };

// file or file system status
struct stat {
	uint32_t st_ino;
	uint32_t st_size;
	enum file_types st_filetype;
};

extern struct partition *cur_part;

void fs_init(void);

char *path_parse(char *pathname, char *name_store);
int32_t path_depth_cnt(char *pathname);

uint32_t fd_local2global(uint32_t local_fd);

int32_t sys_open(const char *pathname, uint8_t flags);
int32_t sys_close(int32_t fd);

// r/w both change fd_pos, but
//    write will start from end of file?
//    read will start from fd_pos?
int32_t sys_write(int32_t fd, const void *buf, uint32_t count);
int32_t sys_read(int32_t fd, void *buf, uint32_t count);

// fd_pos manipulator
int32_t sys_lseek(int32_t fd, int32_t offset, uint8_t whence);

int32_t sys_unlink(const char *pathname);

int32_t sys_mkdir(const char *pathname);

struct dir *sys_opendir(const char *pathname);

int32_t sys_closedir(struct dir *dir);

struct dir_entry *sys_readdir(struct dir *dir);
void sys_rewinddir(struct dir *dir);

int32_t sys_rmdir(const char *pathname);

char *sys_getcwd(char *buf, uint32_t size);
int32_t sys_chdir(const char *path);

int32_t sys_stat(const char *path, struct stat *buf);

#endif
