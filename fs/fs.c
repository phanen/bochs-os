#include "fs.h"
#include "super_block.h"
#include "inode.h"
#include "dir.h"
#include "stdint.h"
#include "stdio-kernel.h"
#include "list.h"
#include "string.h"
#include "ide.h"
#include "global.h"
#include "debug.h"
#include "memory.h"
#include "file.h"
#include "console.h"
#include "ioqueue.h"
#include "keyboard.h"

struct partition* cur_part;

// find a part named `part_name` in `partition_list`
//  then init `cur_part` (using current info in `cur_part` to get more info)
//      read out super block/block bitmap/inode bitmap
static bool mount_partition(struct list_elem* pelem, int arg) {

    char* part_name = (char*)arg;
    struct partition* part = elem2entry(struct partition, part_tag, pelem);

    if (!strcmp(part->name, part_name)) {
        cur_part = part;
        struct disk* hd = cur_part->my_disk;

        // read super_block form disk to mem
        //  frist to sb_buf, then to cur_part->sb
        struct super_block* sb_buf = (struct super_block*)sys_malloc(SECTOR_SIZE);
        cur_part->sb = (struct super_block*)sys_malloc(sizeof(struct super_block));
        if (cur_part->sb == NULL) {
            PANIC("alloc memory failed!");
        }
        memset(sb_buf, 0, SECTOR_SIZE);
        ide_read(hd, cur_part->start_lba + 1, sb_buf, 1);
        memcpy(cur_part->sb, sb_buf, sizeof(struct super_block));

        // load disk block bitmap to mem
        cur_part->block_bitmap.bits = (uint8_t*)sys_malloc(sb_buf->block_bitmap_sects * SECTOR_SIZE);
        if (cur_part->block_bitmap.bits == NULL) {
            PANIC("alloc memory failed!");
        }
        cur_part->block_bitmap.btmp_bytes_len = sb_buf->block_bitmap_sects * SECTOR_SIZE;
        ide_read(hd, sb_buf->block_bitmap_lba, cur_part->block_bitmap.bits, sb_buf->block_bitmap_sects);

        // load disk inode bitmap to mem
        cur_part->inode_bitmap.bits = (uint8_t*)sys_malloc(sb_buf->inode_bitmap_sects * SECTOR_SIZE);
        if (cur_part->inode_bitmap.bits == NULL) {
            PANIC("alloc memory failed!");
        }
        cur_part->inode_bitmap.btmp_bytes_len = sb_buf->inode_bitmap_sects * SECTOR_SIZE;
        ide_read(hd, sb_buf->inode_bitmap_lba, cur_part->inode_bitmap.bits, sb_buf->inode_bitmap_sects);

        list_init(&cur_part->open_inodes);
        printk("mount %s done!\n", part->name);

        return true; // stop list_traversal
    }
    return false; // list_traversal next
}

// init the fs meta info in partition (install fs)
static void partition_format(struct partition* part) {
    // block = sector

    // set each sector size
    uint32_t boot_sector_sects = 1;
    uint32_t super_block_sects = 1;
    uint32_t inode_bitmap_sects = DIV_ROUND_UP(MAX_FILES_PER_PART, BITS_PER_SECTOR);
    uint32_t inode_table_sects = DIV_ROUND_UP(((sizeof(struct inode) * MAX_FILES_PER_PART)), SECTOR_SIZE);

    uint32_t used_sects = boot_sector_sects + super_block_sects + inode_bitmap_sects + inode_table_sects;
    uint32_t free_sects = part->sec_cnt - used_sects;

    // calc the blk bitmap secs
    //  first take all remain as blk
    uint32_t block_bitmap_sects;
    block_bitmap_sects = DIV_ROUND_UP(free_sects, BITS_PER_SECTOR);
    //  get the bitmap length, also blk secs
    uint32_t block_bitmap_bit_len = free_sects - block_bitmap_sects;
    //  update bitmap size to fit remain blk secs
    //  TODO: still lose some space
    block_bitmap_sects = DIV_ROUND_UP(block_bitmap_bit_len, BITS_PER_SECTOR);

    // prepare super_block
    struct super_block sb;
    sb.magic = 0x19491001;
    sb.sec_cnt = part->sec_cnt;
    sb.inode_cnt = MAX_FILES_PER_PART;
    sb.part_lba_base = part->start_lba;

    //  boot -> super -> b_bm -> i_bm -> i_tab -> data
    sb.block_bitmap_lba = sb.part_lba_base + 2;	// boot -> super -> blk
    sb.block_bitmap_sects = block_bitmap_sects;

    sb.inode_bitmap_lba = sb.block_bitmap_lba + sb.block_bitmap_sects;
    sb.inode_bitmap_sects = inode_bitmap_sects;

    sb.inode_table_lba = sb.inode_bitmap_lba + sb.inode_bitmap_sects;
    sb.inode_table_sects = inode_table_sects;

    sb.data_start_lba = sb.inode_table_lba + sb.inode_table_sects;

    sb.root_inode_no = 0;
    sb.dir_entry_size = sizeof(struct dir_entry);

    printk("%s info:\n", part->name);
    printk("    magic:0x%x\n"\
           "    part_lba_base:0x%x\n"\
           "    all_sectors:0x%x"\
           "    inode_cnt:0x%x\n"\
           "    block_bitmap_lba:0x%x\n"\
           "    block_bitmap_sectors:0x%x\n"\
           "    inode_bitmap_lba:0x%x\n"\
           "    inode_bitmap_sectors:0x%x\n"\
           "    inode_table_lba:0x%x\n"\
           "    inode_table_sectors:0x%x\n"\
           "    data_start_lba:0x%x\n",
           sb.magic,
           sb.part_lba_base,
           sb.sec_cnt,
           sb.inode_cnt,
           sb.block_bitmap_lba,
           sb.block_bitmap_sects,
           sb.inode_bitmap_lba,
           sb.inode_bitmap_sects,
           sb.inode_table_lba,
           sb.inode_table_sects,
           sb.data_start_lba);

    struct disk* hd = part->my_disk;

    // install super_block (preserve the boot sec)
    ide_write(hd, part->start_lba + boot_sector_sects, &sb, super_block_sects);
    printk("    super_block_lba:0x%x\n", part->start_lba + 1);

    // prepare a buffer (based on the max size of meta secs)
    uint32_t buf_size = (sb.block_bitmap_sects >= sb.inode_bitmap_sects ? sb.block_bitmap_sects : sb.inode_bitmap_sects);
    buf_size = (buf_size >= sb.inode_table_sects ? buf_size : sb.inode_table_sects) * SECTOR_SIZE;
    uint8_t* buf = (uint8_t*)sys_malloc(buf_size);

    // install block_bitmap
    //      blk0 preserve for root dir
    buf[0] |= 0x01;
    uint32_t block_bitmap_last_byte = block_bitmap_bit_len / 8; // where the last byte begin
    uint8_t  block_bitmap_last_bit  = block_bitmap_bit_len % 8; // the remain bits in last byte
    //      unused byte size in last sec of bitmap (override last few bit)
    uint32_t last_size = SECTOR_SIZE - (block_bitmap_last_byte % SECTOR_SIZE);
    memset(&buf[block_bitmap_last_byte], 0xff, last_size);
    //      reset override bits
    for (uint8_t bit_i = 0; bit_i <= block_bitmap_last_bit; bit_i++) {
        buf[block_bitmap_last_byte] &= ~(1 << bit_i);
    }
    ide_write(hd, sb.block_bitmap_lba, buf, sb.block_bitmap_sects);

    // install inode_bitmap
    memset(buf, 0, buf_size);
    buf[0] |= 0x1;  // preserve for root dir
    //      we know sb.inode_bitmap_sects=1...
    ide_write(hd, sb.inode_bitmap_lba, buf, sb.inode_bitmap_sects);

    // install inode_table
    memset(buf, 0, buf_size);
    struct inode* i = (struct inode*)buf;
    //      the 0th entry(inode of rootdir)
    i->i_size = sb.dir_entry_size * 2;  // . ..
    i->i_no = 0; // self-reference, never mind
    i->i_sectors[0] = sb.data_start_lba; // blk ptr (sec = blk)
    ide_write(hd, sb.inode_table_lba, buf, sb.inode_table_sects);

    // install root dir
    memset(buf, 0, buf_size);
    //      cur dir
    struct dir_entry* p_de = (struct dir_entry*)buf;
    memcpy(p_de->filename, ".", 1);
    p_de->i_no = 0;
    p_de->f_type = FT_DIRECTORY;
    p_de++;
    //      parent dir
    memcpy(p_de->filename, "..", 2);
    p_de->i_no = 0; // circle it
    p_de->f_type = FT_DIRECTORY;
    //      write the first data sec
    ide_write(hd, sb.data_start_lba, buf, 1);
    printk("    root_dir_lba:0x%x\n", sb.data_start_lba);

    printk("%s format done\n", part->name);
    sys_free(buf);
}

// get the top name
// return next subpathname
static char* path_parse(char* pathname, char* name_store) {
    if (pathname[0] == '/') {
        // for ///a
        while(*(++pathname) == '/');
    }

    // read to null or next /
    while (*pathname != '/' && *pathname != 0) {
        *name_store++ = *pathname++;
    }

    // end with null -> pathname has no more content
    if (pathname[0] == 0) {
        return NULL;
    }

    // else, return ptr to the next start point of parsing
    return pathname;
}

// get depth of the path
//	 / -> 0, /a -> 1, /a/ -> 1
// i.e. count name split by pattern `/+`
int32_t path_depth_cnt(char* pathname) {
    ASSERT(pathname != NULL);

    char* p = pathname;
    char name[MAX_FILE_NAME_LEN];
    uint32_t depth = 0;

    // count the time name is not empty
    p = path_parse(p, name);
    while (name[0]) {
        depth++;
        memset(name, 0, MAX_FILE_NAME_LEN);
        if (p) {
            p  = path_parse(p, name);
        }
    }
    return depth;
}

// pathname -> path_search_record and inode
static int search_file(const char* pathname, struct path_search_record* searched_record) {
    // pattern can be easily parsed as root dir
    //      but pattern like `/.//..//../.` need to be parse later
    if (!strcmp(pathname, "/") || !strcmp(pathname, "/.") || !strcmp(pathname, "/..")) {
        searched_record->parent_dir = &root_dir;
        searched_record->file_type = FT_DIRECTORY;
        searched_record->searched_path[0] = 0;
        return 0;
    }

    // now the pattern must be '/xxx'
    uint32_t path_len = strlen(pathname);
    ASSERT(pathname[0] == '/' && path_len > 1 && path_len < MAX_PATH_LEN);

    char* sub_path = (char*)pathname;
    struct dir* parent_dir = &root_dir;
    struct dir_entry dir_e;
    char name[MAX_FILE_NAME_LEN] = {0};

    searched_record->parent_dir = parent_dir;
    searched_record->file_type = FT_UNKNOWN;
    uint32_t parent_inode_no = 0;

    sub_path = path_parse(sub_path, name);
    // exit only when parse `/` last time
    while (name[0]) {
        ASSERT(strlen(searched_record->searched_path) < 512);
        strcat(searched_record->searched_path, "/");
        strcat(searched_record->searched_path, name);

        // find in current path
        if (search_dir_entry(cur_part, parent_dir, name, &dir_e)) {
            // parse next layer
            memset(name, 0, MAX_FILE_NAME_LEN);
            if (sub_path) {
                sub_path = path_parse(sub_path, name);
            }

            if (FT_DIRECTORY == dir_e.f_type) {
                parent_inode_no = parent_dir->inode->i_no;
                dir_close(parent_dir);
                parent_dir = dir_open(cur_part, dir_e.i_no);
                searched_record->parent_dir = parent_dir;
                continue;
            }
            else if (FT_REGULAR == dir_e.f_type) {
                // FIXME: ?want a dir but give regular file first
                //          then stop here?
                // or DESIGN: never allow create two files with the same name
                searched_record->file_type = FT_REGULAR;
                return dir_e.i_no;
            }
        }
        else {
            return -1;
        }
    }

    // rollback??
    dir_close(searched_record->parent_dir); // not close rootdir
    searched_record->parent_dir = dir_open(cur_part, parent_inode_no);
    searched_record->file_type = FT_DIRECTORY;
    return dir_e.i_no;
}

// open file, return fd
//      support file only, for dir -> use `dir_open`
int32_t sys_open(const char* pathname, uint8_t flags) {

    // is dir (by name)
    if (pathname[strlen(pathname) - 1] == '/') {
        printk("can`t open a directory %s\n",pathname);
        return -1;
    }

    ASSERT(flags <= 7);
    int32_t fd = -1;

    struct path_search_record searched_record;
    memset(&searched_record, 0, sizeof(struct path_search_record));

    uint32_t pathname_depth = path_depth_cnt((char*)pathname);

    // search it
    int inode_no = search_file(pathname, &searched_record);
    // found (but maybe incorrect)
    bool found = inode_no != -1 ? true : false;

    // is dir (by dir_e)
    if (searched_record.file_type == FT_DIRECTORY) {
        printk("can`t open a direcotry with open(), use opendir() to instead\n");
        dir_close(searched_record.parent_dir);
        return -1;
    }

    uint32_t path_searched_depth = path_depth_cnt(searched_record.searched_path);

    // fail to search full path (requrie `/a/b/c`, but give `/a/b`)
    if (pathname_depth != path_searched_depth) {
        printk("cannot access %s: Not a directory, subpath %s is`t exist\n", \
               pathname, searched_record.searched_path);
        dir_close(searched_record.parent_dir);
        return -1;
    }

    // not found in last dir, and no creat flag
    if (!found && !(flags & O_CREAT)) {
        printk("in path %s, file %s is't exist\n", \
               searched_record.searched_path, \
               (strrchr(searched_record.searched_path, '/') + 1));
        dir_close(searched_record.parent_dir);
        return -1;
    }
        // found in last dir, but with create flag
        // NOTE: two files with the same name is not allow!!!
        //      so, search relevant API can simply stop when first match
        //      we only need to deal with file type
    else if (found && (flags & O_CREAT)) {
        printk("%s has already exist!\n", pathname);
        dir_close(searched_record.parent_dir);
        return -1;
    }

    // (not found + create flag) or (found + no create flag)
    switch (flags & O_CREAT) {
        case O_CREAT:
            ASSERT(!found); // inode_no == -1
            printk("creating file\n");
            fd = file_create(searched_record.parent_dir, (strrchr(pathname, '/') + 1), flags);
            dir_close(searched_record.parent_dir);
            break;
        // otherwise, should be other flag (r/w)
        default: // O_WRONLY O_RDWR O_RDONLY
            ASSERT(found);
            fd = file_open(inode_no, flags);
    }
    // local fd (index of `pcb->fd_table`)
    return fd;
}

static uint32_t fd_local2global(uint32_t local_fd) {
    int32_t global_fd = running_thread()->fd_table[local_fd];
    ASSERT(global_fd >= 0 && global_fd < MAX_FILE_OPEN);
    return (uint32_t)global_fd;
}

// close by local fd
int32_t sys_close(int32_t fd) {
    int32_t ret = -1;
    if (fd > 2) { //
        uint32_t _fd = fd_local2global(fd);
        ret = file_close(&file_table[_fd]);
        // free the slot in local tab
        running_thread()->fd_table[fd] = -1;
    }
    return ret;
}

// always write by append (change fd_pos)
int32_t sys_write(int32_t fd, const void* buf, uint32_t count) {
    if (fd < 0) {
        printk("sys_write: fd error\n");
        return -1;
    }

    // write to stdout: count < 1024
    //      warn: end 0!!!
    if (fd == stdout_no) {
        char tmp_buf[1024] = {0};
        memcpy(tmp_buf, buf, count);
        console_put_str(tmp_buf);
        return count;
    }

    uint32_t _fd = fd_local2global(fd);
    struct file* wr_file = &file_table[_fd];

    // write flag
    if (wr_file->fd_flag & O_WRONLY || wr_file->fd_flag & O_RDWR) {
        uint32_t bytes_written  = file_write(wr_file, buf, count);
        return bytes_written;
    }
    else {
        console_put_str("sys_write: not allowed to write file without flag O_RDWR or O_WRONLY\n");
        return -1;
    }
}

// use fd_pos
int32_t sys_read(int32_t fd, void* buf, uint32_t count) {
    ASSERT(buf != NULL);
    if (fd < 0 || fd == stdout_no || fd == stderr_no) {
        printk("sys_read: fd error\n");
        return -1;
    }

    // read from device
    if (fd == stdin_no) {
        char* buffer = buf;
        uint32_t bytes_read = 0;
        while (bytes_read < count) {
            *buffer = ioq_getchar(&kbd_buf);
            bytes_read++;
            buffer++;
        }
        return (bytes_read == 0 ? -1 : (int32_t)bytes_read);
    }

    // read from file
    uint32_t _fd = fd_local2global(fd);
    return file_read(&file_table[_fd], buf, count);
}

// change fd_pos (file table entry)
int32_t sys_lseek(int32_t fd, int32_t offset, uint8_t whence) {
    if (fd < 0) {
        printk("sys_lseek: fd error\n");
        return -1;
    }
    ASSERT(whence > 0 && whence < 4);
    uint32_t _fd = fd_local2global(fd);
    struct file* pf = &file_table[_fd];

    int32_t new_pos = 0;
    int32_t file_size = (int32_t)pf->fd_inode->i_size;
    switch (whence) {
        case SEEK_SET: // + only
            new_pos = offset;
            break;

        case SEEK_CUR:	// +/-
            new_pos = (int32_t)pf->fd_pos + offset;
            break;
        case SEEK_END:	// -only
            new_pos = file_size + offset;
    }
    // merge
    if (new_pos < 0 || new_pos > (file_size - 1)) {
        return -1;
    }
    pf->fd_pos = new_pos;
    return pf->fd_pos;
}

// delete file (not dir)
//      abort if in use: in file table
//      delete dir entry
//      delete inode(bitmap, block)
int32_t sys_unlink(const char* pathname) {
    ASSERT(strlen(pathname) < MAX_PATH_LEN);

    struct path_search_record searched_record;
    memset(&searched_record, 0, sizeof(struct path_search_record));
    int inode_no = search_file(pathname, &searched_record);
    ASSERT(inode_no != 0);

    // no file
    if (inode_no == -1) {
        printk("file %s not found!\n", pathname);
        dir_close(searched_record.parent_dir);
        return -1;
    }
    // is dir
    if (searched_record.file_type == FT_DIRECTORY) {
        printk("can`t delete a direcotry with unlink(), use rmdir() to instead\n");
        dir_close(searched_record.parent_dir);
        return -1;
    }

    // if open?
    uint32_t file_i = 0;
    while (file_i < MAX_FILE_OPEN) {
        if (file_table[file_i].fd_inode != NULL &&
            (uint32_t)inode_no == file_table[file_i].fd_inode->i_no) {
            break;
        }
        file_i++;
    }
    // is open
    if (file_i < MAX_FILE_OPEN) {
        dir_close(searched_record.parent_dir);
        printk("file %s is in use, not allow to delete!\n", pathname);
        return -1;
    }

    // not open
    ASSERT(file_i == MAX_FILE_OPEN);

    //  may cross secs when sync inode in lower API
    void* io_buf = sys_malloc(SECTOR_SIZE + SECTOR_SIZE);
    if (io_buf == NULL) {
        dir_close(searched_record.parent_dir);
        printk("sys_unlink: malloc for io_buf failed\n");
        return -1;
    }

    struct dir* parent_dir = searched_record.parent_dir;

    // delete in dir
    delete_dir_entry(cur_part, parent_dir, inode_no, io_buf);

    // delete inode
    inode_release(cur_part, inode_no);

    sys_free(io_buf);
    dir_close(searched_record.parent_dir);

    return 0;
}

// create a dir
int32_t sys_mkdir(const char* pathname) {

    uint8_t rollback_step = 0;
    void* io_buf = sys_malloc(SECTOR_SIZE * 2);
    if (io_buf == NULL) {
        printk("sys_mkdir: sys_malloc for io_buf failed\n");
        return -1;
    }

    struct path_search_record searched_record;
    memset(&searched_record, 0, sizeof(struct path_search_record));
    int inode_no = -1;
    inode_no = search_file(pathname, &searched_record);
    // exist
    if (inode_no != -1) {
        printk("sys_mkdir: file or directory %s exist!\n", pathname);
        rollback_step = 1;
        goto rollback;
    }
        // not found pdir
    else {
        uint32_t pathname_depth = path_depth_cnt((char*)pathname);
        uint32_t path_searched_depth = path_depth_cnt(searched_record.searched_path);
        if (pathname_depth != path_searched_depth) {
            printk("sys_mkdir: can`t access %s, subpath %s is`t exist\n",
                   pathname, searched_record.searched_path);
            rollback_step = 1;
            goto rollback;
        }
    }

    // pdir exist, just create it

    struct dir* parent_dir = searched_record.parent_dir;
    //      not use pathname here: not sure if tail with '/'
    char* dirname = strrchr(searched_record.searched_path, '/') + 1;

    // alloc a inode
    inode_no = inode_bitmap_alloc(cur_part);
    if (inode_no == -1) {
        printk("sys_mkdir: allocate inode failed\n");
        rollback_step = 1;
        goto rollback;
    }
    struct inode new_dir_inode;
    inode_init(inode_no, &new_dir_inode);

    // alloc a blk (bitmap, inode ptr, bm sync)
    uint32_t block_bitmap_idx = 0;
    int32_t block_lba = -1;
    block_lba = block_bitmap_alloc(cur_part);
    if (block_lba == -1) {
        printk("sys_mkdir: block_bitmap_alloc for create directory failed\n");
        rollback_step = 2;
        goto rollback;
    }
    new_dir_inode.i_sectors[0] = block_lba;
    block_bitmap_idx = block_lba - cur_part->sb->data_start_lba;
    ASSERT(block_bitmap_idx != 0);
    bitmap_sync(cur_part, block_bitmap_idx, BLOCK_BITMAP);

    // init two defaut entries in dir to be created: '.', '..'
    memset(io_buf, 0, SECTOR_SIZE * 2);
    struct dir_entry* p_de = (struct dir_entry*)io_buf;

    memcpy(p_de->filename, ".", 1);
    p_de->i_no = inode_no ;
    p_de->f_type = FT_DIRECTORY;
    p_de++;
    memcpy(p_de->filename, "..", 2);
    p_de->i_no = parent_dir->inode->i_no;
    p_de->f_type = FT_DIRECTORY;
    ide_write(cur_part->my_disk, new_dir_inode.i_sectors[0], io_buf, 1);
    new_dir_inode.i_size = 2 * cur_part->sb->dir_entry_size;

    // add this dir's entry in parent
    //      different from init, it need a complex API
    //      because it may alloc new block
    struct dir_entry new_dir_entry;
    memset(&new_dir_entry, 0, sizeof(struct dir_entry));
    create_dir_entry(dirname, inode_no, FT_DIRECTORY, &new_dir_entry);
    memset(io_buf, 0, SECTOR_SIZE * 2);
    if (!sync_dir_entry(parent_dir, &new_dir_entry, io_buf)) {
        printk("sys_mkdir: sync_dir_entry to disk failed!\n");
        rollback_step = 2;
        goto rollback;
    }
    // sync pdir's inode  (pdir->inode->i_size changed)
    memset(io_buf, 0, SECTOR_SIZE * 2);
    inode_sync(cur_part, parent_dir->inode, io_buf);

    // sync dir's inode
    memset(io_buf, 0, SECTOR_SIZE * 2);
    inode_sync(cur_part, &new_dir_inode, io_buf);

    // sync dir's inode bitmap
    bitmap_sync(cur_part, inode_no, INODE_BITMAP);

    sys_free(io_buf);

    dir_close(searched_record.parent_dir);
    return 0;

rollback:
    switch (rollback_step) {
        case 2:
            bitmap_set(&cur_part->inode_bitmap, inode_no, 0);
        case 1:
            dir_close(searched_record.parent_dir);
            break;
    }
    sys_free(io_buf);
    return -1;
}

// get dit ptr
struct dir* sys_opendir(const char* name) {
    ASSERT(strlen(name) < MAX_PATH_LEN);

    if (name[0] == '/' && (name[1] == 0 || name[0] == '.')) {
        return &root_dir;
    }

    struct path_search_record searched_record;
    memset(&searched_record, 0, sizeof(struct path_search_record));
    int inode_no = search_file(name, &searched_record);
    struct dir* ret = NULL;

    // no such dir
    if (inode_no == -1) {
        printk("In %s, sub path %s not exist\n", name, searched_record.searched_path);
    }
        // is regular file
    else {
        if (searched_record.file_type == FT_REGULAR) {
            printk("%s is regular file!\n", name);
        } else if (searched_record.file_type == FT_DIRECTORY) {
            ret = dir_open(cur_part, inode_no);
        }
    }

    dir_close(searched_record.parent_dir);
    return ret;
}

int32_t sys_closedir(struct dir* dir) {
    int32_t ret = -1;
    if (dir != NULL) {
        dir_close(dir);
        ret = 0;
    }
    return ret;
}

// read one entry, and update dir->dir_pos
//      ok, return the entry (which is in dir->dir_buf)
//      fail, return NULL
struct dir_entry* sys_readdir(struct dir* dir) {
    ASSERT(dir != NULL);
    return dir_read(dir);
}

void sys_rewinddir(struct dir* dir) {
    dir->dir_pos = 0;
}

// rm an empty dir by name
//      so we need to open it to know if empty
int32_t sys_rmdir(const char* pathname) {

    struct path_search_record searched_record;
    memset(&searched_record, 0, sizeof(struct path_search_record));
    int inode_no = search_file(pathname, &searched_record);

    ASSERT(inode_no != 0);

    int retval = -1;
    if (inode_no == -1) {
        printk("In %s, sub path %s not exist\n", pathname, searched_record.searched_path);
        goto fail_before_dir_open;
    }

    if (searched_record.file_type == FT_REGULAR) {
        printk("%s is regular file!\n", pathname);
        goto fail_before_dir_open;
    }

    struct dir* dir = dir_open(cur_part, inode_no);
    if (!dir_is_empty(dir)) {
        printk("dir %s is not empty, it is not allowed to delete a nonempty directory!\n", pathname);
        goto fail_after_dir_open;
    }

    if (!dir_remove(searched_record.parent_dir, dir)) {
        retval = 0;
    } // else fail to remove anyway

fail_after_dir_open:
    dir_close(dir);
fail_before_dir_open:
    dir_close(searched_record.parent_dir);
    return retval;
}


// get inode_no of parent_dir(..) from cur dir
static uint32_t get_parent_dir_inode_nr(uint32_t child_inode_nr, void* io_buf) {

    // get cur child dir's inode
    struct inode* child_dir_inode = inode_open(cur_part, child_inode_nr);
    uint32_t block_lba = child_dir_inode->i_sectors[0];
    ASSERT(block_lba >= cur_part->sb->data_start_lba);
    inode_close(child_dir_inode);

    // get dir entry
    ide_read(cur_part->my_disk, block_lba, io_buf, 1);
    struct dir_entry* dir_e = (struct dir_entry*)io_buf;

    // 0->".", 1->".."
    ASSERT(dir_e[1].i_no < MAX_FILES_PER_PART && dir_e[1].f_type == FT_DIRECTORY);
    return dir_e[1].i_no;
}

//  search `c_inode_nr`(dir) in in `p_inode_nr`(dir)
//  ok, then write name to buf `path`
static int get_child_dir_name(uint32_t p_inode_nr, uint32_t c_inode_nr, char* path, void* io_buf) {

    struct inode* parent_dir_inode = inode_open(cur_part, p_inode_nr);

    uint8_t blk_i = 0;
    uint32_t all_blocks[FLATTEN_PTRS] = {0}, block_cnt = DIRECT_PTRS;
    while (blk_i < DIRECT_PTRS) {
        all_blocks[blk_i] = parent_dir_inode->i_sectors[blk_i];
        blk_i++;
    }
    if (parent_dir_inode->i_sectors[DIRECT_PTRS]) {
        ide_read(cur_part->my_disk, parent_dir_inode->i_sectors[DIRECT_PTRS], all_blocks + DIRECT_PTRS, 1);
        block_cnt = FLATTEN_PTRS;
    }
    inode_close(parent_dir_inode);

    struct dir_entry* dir_e = (struct dir_entry*)io_buf;
    uint32_t dir_entry_size = cur_part->sb->dir_entry_size;
    uint32_t dir_entrys_per_sec = (SECTOR_SIZE / dir_entry_size);

    for (blk_i = 0; blk_i < block_cnt; blk_i++) {
        if(all_blocks[blk_i]) {
            ide_read(cur_part->my_disk, all_blocks[blk_i], io_buf, 1);
            for (uint8_t dir_e_i = 0; dir_e_i < dir_entrys_per_sec; dir_e_i++) {
                if ((dir_e + dir_e_i)->i_no == c_inode_nr) {
                    strcat(path, "/");
                    strcat(path, (dir_e + dir_e_i)->filename);
                    return 0;
                }
            }
        }
    }
    return -1;
}

// write pwd to buf (with specific `size`)
//      syscall wrapper: if buf is NULL, os will alloc
char* sys_getcwd(char* buf, uint32_t size) {

    //  so here buf must be safe
    ASSERT(buf != NULL);

    void* io_buf = sys_malloc(SECTOR_SIZE);
    if (io_buf == NULL) {
        return NULL;
    }

    struct task_struct* cur_thread = running_thread();
    int32_t parent_inode_nr = 0;
    int32_t child_inode_nr = cur_thread->cwd_inode_nr;
    ASSERT(child_inode_nr >= 0 && child_inode_nr < MAX_FILES_PER_PART);

    // rootdir
    if (child_inode_nr == 0) {
        buf[0] = '/';
        buf[1] = 0;
        return buf;
    }

    memset(buf, 0, size);
    char full_path_reverse[MAX_PATH_LEN] = {0};	  // 用来做全路径缓冲区

    // reversely find root (then will get a full path)
    while (child_inode_nr) {
        parent_inode_nr = get_parent_dir_inode_nr(child_inode_nr, io_buf);

        if (get_child_dir_name(parent_inode_nr, child_inode_nr, full_path_reverse, io_buf) == -1) {
            // not found cwd in current pdir?
            sys_free(io_buf);
            return NULL;
        }
        child_inode_nr = parent_inode_nr;
    }
    ASSERT(strlen(full_path_reverse) <= size);

    // reversely copy the `full_path_reverse`
    //  then we get correct absolute path
    char* last_slash;
    while ((last_slash = strrchr(full_path_reverse, '/'))) {
        uint16_t len = strlen(buf);
        strcpy(buf + len, last_slash);
        *last_slash = 0;
    }
    sys_free(io_buf);
    return buf;
}

// change cwd to a absolute path
int32_t sys_chdir(const char* path) {
    int32_t ret = -1;
    struct path_search_record searched_record;
    memset(&searched_record, 0, sizeof(struct path_search_record));
    int inode_no = search_file(path, &searched_record);
    if (inode_no != -1) {
        if (searched_record.file_type == FT_DIRECTORY) {
            running_thread()->cwd_inode_nr = inode_no;
            ret = 0;
        }
        else {
            printk("sys_chdir: %s is regular file or other!\n", path);
        }
    } // else no such path
    dir_close(searched_record.parent_dir);
    return ret;
}

// get the stat of given file (absolute path)
//      file type in dir entry
//      inode no  in dir entry
//      file size in inode
int32_t sys_stat(const char* path, struct stat* buf) {

    if (!strcmp(path, "/") || !strcmp(path, "/.") || !strcmp(path, "/..")) {
        buf->st_filetype = FT_DIRECTORY;
        buf->st_ino = 0;
        buf->st_size = root_dir.inode->i_size;
        return 0;
    }

    int32_t ret = -1;
    struct path_search_record searched_record;
    memset(&searched_record, 0, sizeof(struct path_search_record));
    int inode_no = search_file(path, &searched_record);
    if (inode_no != -1) {
        struct inode* obj_inode = inode_open(cur_part, inode_no);
        buf->st_size = obj_inode->i_size;
        inode_close(obj_inode);
        buf->st_filetype = searched_record.file_type;
        buf->st_ino = inode_no;
        ret = 0;
    } 
    else {
        printk("sys_stat: %s not found\n", path);
    }
    dir_close(searched_record.parent_dir);
    return ret;
}

// scan fs(super_block) in each partition
// if none fs on it, then install default fs
void fs_init() {

    // super block
    struct super_block* sb_buf = (struct super_block*)sys_malloc(SECTOR_SIZE);
    if (sb_buf == NULL) {
        PANIC("alloc memory failed!");
    }

    printk("searching filesystem......\n");

    // foreach channel (only one we know)
    for (uint8_t chan_no = 0; chan_no < chan_cnt; chan_no++) {
        // foreach disk on channel
        for (uint8_t dev_no; dev_no < 2; dev_no++) {
            if (dev_no == 0) {  // ignore the raw disk: hd60M.img
                continue;
            }
            struct disk* hd = &channels[chan_no].devices[dev_no];
            struct partition* part = hd->prim_parts;
            // foreach parts on disk
            //          we have 4 prim + 8 logic on (chan0, dev2)
            for (uint8_t part_i = 0; part_i < 12; part_i++, part++) {
                if (part_i == 4) {  // now switch to logic parts
                    part = hd->logic_parts;
                }

                // partition exist?
                //      1. nest: channels->disk->part
                //      2. channels in bss?
                //      so: no partition <=> `sec_cnt`
                //      TODO: not elegant
                if (part->sec_cnt != 0) {
                    memset(sb_buf, 0, SECTOR_SIZE);

                    // read the supoer block and check type (magic_num)
                    // super_block is the 2rd block from data area
                    ide_read(hd, part->start_lba + 1, sb_buf, 1);

                    if (sb_buf->magic == 0x19491001) {
                        printk("%s has filesystem (magic: 0x%x)\n", part->name, sb_buf->magic);
                    }
                    else {  // recognize no other type
                        printk("formatting %s`s partition %s......\n", hd->name, part->name);
                        partition_format(part); // install the format what we know on it
                    }
                }
            } // part
        } // disk
    } // channel

    sys_free(sb_buf);

    // mount a partition on fs
    char default_part[8] = "sdb1";
    list_traversal(&partition_list, mount_partition, (int)default_part);
    open_root_dir(cur_part);
    uint32_t fd_idx = 0;
    while (fd_idx < MAX_FILE_OPEN) { // init: no open file
        file_table[fd_idx++].fd_inode = NULL;
    }
}
