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

// scan fs(super_block) in each partition
// if none then format it
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
