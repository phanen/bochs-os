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

struct partition* cur_part;

// find a part named `part_name` in `partition_list`
//  then init `cur_part`
static bool mount_partition(struct list_elem* pelem, int arg) {

    char* part_name = (char*)arg;
    struct partition* part = elem2entry(struct partition, part_tag, pelem);

    if (!strcmp(part->name, part_name)) {
        cur_part = part;
        struct disk* hd = cur_part->my_disk;

        // read super_block form disk to mem
        //  frist to sb_buf
        //  then to cur_part->sb
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

        // find current path
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
                searched_record->file_type = FT_REGULAR;
                return dir_e.i_no; // FIXME: stop?
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

    // not found in last dir, and not creat
    if (!found && !(flags & O_CREAT)) {
        printk("in path %s, file %s is't exist\n", \
               searched_record.searched_path, \
               (strrchr(searched_record.searched_path, '/') + 1));
        dir_close(searched_record.parent_dir);
        return -1;
    }
        // found in last dir, but create
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

int32_t sys_read(int32_t fd, void* buf, uint32_t count) {
    if (fd < 0) {
        printk("sys_read: fd error\n");
        return -1;
    }
    ASSERT(buf != NULL);
    uint32_t _fd = fd_local2global(fd);
    return file_read(&file_table[_fd], buf, count);
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
