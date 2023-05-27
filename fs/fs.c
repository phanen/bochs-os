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
                    // super_block is the 2rd block from data area读出分区的超级块,根据魔数是否正确来判断是否存在文件系统 */
                    ide_read(hd, part->start_lba + 1, sb_buf, 1);

                    if (sb_buf->magic == 0x19491001) {
                        printk("%s has filesystem (magic: 0x%x)\n", part->name, sb_buf->magic);
                    }
                    else {  // recognize no other type
                        printk("formatting %s`s partition %s......\n", hd->name, part->name);
                        partition_format(part); // format to what we know
                    }
                }
            } // part
        } // disk
    } // channel

    sys_free(sb_buf);
}
