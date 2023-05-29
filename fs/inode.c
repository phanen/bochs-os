#include "inode.h"
#include "fs.h"
#include "file.h"
#include "global.h"
#include "debug.h"
#include "memory.h"
#include "interrupt.h"
#include "list.h"
#include "stdio-kernel.h"
#include "string.h"
#include "super_block.h"

struct inode_position {
    bool     two_sec;	// inode cross two secs
    uint32_t sec_lba;	// sec no
    uint32_t off_size;	// offset in byte
};

// get the sec and offset of inode
static void inode_locate(struct partition* part, uint32_t inode_no, struct inode_position* inode_pos) {
    ASSERT(inode_no < MAX_FILES_PER_PART);

    uint32_t inode_table_lba = part->sb->inode_table_lba;

    uint32_t inode_size = sizeof(struct inode);
    uint32_t off_size = inode_no * inode_size;
    uint32_t off_sec  = off_size / SECTOR_SIZE;
    uint32_t off_size_in_sec = off_size % SECTOR_SIZE;

    // cross secs or not
    if (SECTOR_SIZE < off_size_in_sec + inode_size) {
        inode_pos->two_sec = true;
    } else {
        inode_pos->two_sec = false;
    }
    inode_pos->sec_lba = inode_table_lba + off_sec;
    inode_pos->off_size = off_size_in_sec;
}

// sync inode to disk partition (inode changes: i_no, i_size, i_sectors)
//      why called `sync`:
//          read -> embed -> write
//      why we provide a io_buf before?
//          avoid fail in function (fail outside maybe easier to debug)
void inode_sync(struct partition* part, struct inode* inode, void* io_buf) {
    uint8_t inode_no = inode->i_no;
    struct inode_position inode_pos;
    inode_locate(part, inode_no, &inode_pos);
    ASSERT(inode_pos.sec_lba < (part->start_lba + part->sec_cnt));

    // inode_tag, i_open_cnts, write_deny is mem-only
    struct inode inode_buf;
    memcpy(&inode_buf, inode, sizeof(struct inode));
    inode_buf.i_open_cnts = 0;
    inode_buf.write_deny = false;
    inode_buf.inode_tag.prev = inode_buf.inode_tag.next = NULL;

    char* sec_buf = (char*)io_buf;
    uint32_t sync_secs = (inode_pos.two_sec ? 2 : 1);
    ide_read(part->my_disk, inode_pos.sec_lba, sec_buf, sync_secs);
    memcpy((sec_buf + inode_pos.off_size), &inode_buf, sizeof(struct inode));
    ide_write(part->my_disk, inode_pos.sec_lba, sec_buf, sync_secs);
}

// inode no -> inode (open or add links)
//      get the inode in cache list
//      none, then load it from disk (and update cache)
struct inode* inode_open(struct partition* part, uint32_t inode_no) {
    // search inode in cache list (has been open?)
    struct inode* inode_found;
    for (struct list_elem* elem = part->open_inodes.head.next;
    elem != &part->open_inodes.tail;
    elem = elem->next) {
        inode_found = elem2entry(struct inode, inode_tag, elem);
        if (inode_found->i_no == inode_no) {
            inode_found->i_open_cnts++;
            return inode_found;
        }
    }

    // not found in cache list(not open?)
    // read inode from disk to mem
    //      first get its disk pos
    struct inode_position inode_pos;
    inode_locate(part, inode_no, &inode_pos);

    //      malloc for inode
    struct task_struct* cur = running_thread();
    uint32_t* cur_pagedir_bak = cur->pgdir;
    //          to share inode among procs
    //          we need load inode to kernel space
    //          here we hack to use kernel pgdir
    cur->pgdir = NULL;
    inode_found = (struct inode*)sys_malloc(sizeof(struct inode));
    cur->pgdir = cur_pagedir_bak;

    char* inode_buf;
    if (inode_pos.two_sec) {
        inode_buf = (char*)sys_malloc(SECTOR_SIZE * 2);
        ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 2);
    }
    else {
        inode_buf = (char*)sys_malloc(SECTOR_SIZE);
        ide_read(part->my_disk, inode_pos.sec_lba, inode_buf, 1);
    }
    memcpy(inode_found, inode_buf + inode_pos.off_size, sizeof(struct inode));

    // insert to head
    list_push(&part->open_inodes, &inode_found->inode_tag);
    inode_found->i_open_cnts = 1;

    sys_free(inode_buf);
    return inode_found;
}

// close or unlink
void inode_close(struct inode* inode) {
    enum intr_status old_status = intr_disable();

    // reduce links
    --inode->i_open_cnts;

    // no links, delete cache
    if (inode->i_open_cnts == 0) {
        list_remove(&inode->inode_tag);
        struct task_struct* cur = running_thread();
        uint32_t* cur_pagedir_bak = cur->pgdir;
        // switch to kernel mem pool (always create inode in kernel space)
        cur->pgdir = NULL;
        sys_free(inode);
        cur->pgdir = cur_pagedir_bak;
    }
    intr_set_status(old_status);
}

// init new_inode
//      without attached block...
void inode_init(uint32_t inode_no, struct inode* new_inode) {
    new_inode->i_no = inode_no;
    new_inode->i_size = 0;
    new_inode->i_open_cnts = 0;
    new_inode->write_deny = false;

    // init i_sector
    for (uint8_t i = 0; i < TOTAL_PTRS; i++) {
        new_inode->i_sectors[i] = 0;
    }
}
