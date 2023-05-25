#include "ide.h"
#include "sync.h"
#include "stdio.h"
#include "stdio-kernel.h"
#include "interrupt.h"
#include "memory.h"
#include "debug.h"
#include "string.h"

// get the port num from channel
#define reg_data(channel)	 (channel->port_base + 0)
#define reg_error(channel)	 (channel->port_base + 1)
#define reg_sect_cnt(channel)	 (channel->port_base + 2)
#define reg_lba_l(channel)	 (channel->port_base + 3)
#define reg_lba_m(channel)	 (channel->port_base + 4)
#define reg_lba_h(channel)	 (channel->port_base + 5)
#define reg_dev(channel)	 (channel->port_base + 6)
#define reg_status(channel)	 (channel->port_base + 7)
#define reg_cmd(channel)	 (reg_status(channel))
#define reg_alt_status(channel)  (channel->port_base + 0x206)
#define reg_ctl(channel)	 reg_alt_status(channel)

// bits for reg_alt_status
#define BIT_STAT_BSY	   0x80	    // busy
#define BIT_STAT_DRDY	   0x40	    // drive ready
#define BIT_STAT_DRQ	   0x8	    // data ready

// bits for device
#define BIT_DEV_MBS	   0xa0	    // 7, 5
#define BIT_DEV_LBA	   0x40
#define BIT_DEV_DEV	   0x10

// cmd for disk operaition
#define CMD_IDENTIFY	   0xec	    // identify
#define CMD_READ_SECTOR	   0x20     // read
#define CMD_WRITE_SECTOR   0x30	    // write

// max lab addr(max sector no.) (debug)
#define max_lba ((80 * 1024 * 1024 / 512) - 1)	// slave disk is 80MB

uint8_t channel_cnt;
struct ide_channel channels[2]; // god know

void ide_init() {
   printk("ide_init start\n");

   uint8_t hd_cnt = *((uint8_t*)(0x475)); // disk cnt
   ASSERT(hd_cnt > 0);
   channel_cnt = DIV_ROUND_UP(hd_cnt, 2); // ide cnt
   struct ide_channel* channel;
   uint8_t channel_no = 0;

   // init each channel
   while (channel_no < channel_cnt) {
      channel = &channels[channel_no];
      sprintf(channel->name, "ide%d", channel_no);

      switch (channel_no) {
         case 0:
            channel->port_base	 = 0x1f0;	   // ide0 channel, base: 0x1f0
            channel->irq_no	 = 0x20 + 14;	   // irq14 (two disks)
            break;
         case 1:
            channel->port_base	 = 0x170;	   // ide1 channel, base: 0x170
            channel->irq_no	 = 0x20 + 15;	   // irq15
            break;
      }

      channel->expecting_intr = false;             // no cmd

      lock_init(&channel->lock);
      // semaphore to deal with disk I/O
      //    request disk data -> driver proc -> sema_down -> block the thread
      //    after: disk intr -> intr handler -> sema_up -> weak the thread
      sema_init(&channel->disk_done, 0);

      channel_no++;
   }

   printk("ide_init done\n");
}
