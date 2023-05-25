#include "ide.h"
#include "sync.h"
#include "stdio.h"
#include "stdio-kernel.h"
#include "interrupt.h"
#include "memory.h"
#include "debug.h"
#include "string.h"
#include "io.h"
#include "string.h"
#include "list.h"
#include "timer.h"

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

// select the disk to r/w
static void select_disk(struct disk* hd) {
   uint8_t reg_device = BIT_DEV_MBS | BIT_DEV_LBA;
   if (hd->dev_no == 1) { // slave disk
      reg_device |= BIT_DEV_DEV;
   }
   outb(reg_dev(hd->my_channel), reg_device);
}

// select which sector to r/w
static void select_sector(struct disk* hd, uint32_t lba, uint8_t sec_cnt) {
   ASSERT(lba <= max_lba);
   struct ide_channel* channel = hd->my_channel;

   // cnt
   outb(reg_sect_cnt(channel), sec_cnt);	 // sec_cnt=0 -> 256 sectors

   // lba (outb %b0, %w1)
   outb(reg_lba_l(channel), lba);		 // lba 0-8
   outb(reg_lba_m(channel), lba >> 8);		 // lba 8-15
   outb(reg_lba_h(channel), lba >> 16);		 // lba 16-23

   // lba 24-27 (write to reg_dev 0-3)
   // be careful, don't flip the other bits
   outb(reg_dev(channel), BIT_DEV_MBS | BIT_DEV_LBA | (hd->dev_no == 1 ? BIT_DEV_DEV : 0) | lba >> 24);
}

// cmd
static void cmd_out(struct ide_channel* channel, uint8_t cmd) {
   channel->expecting_intr = true; // used by intr_hd_handler
   outb(reg_cmd(channel), cmd);
}

// (context: only when disk is ready)
// read (<= 256) sectors to mem buf
static void read_from_sector(struct disk* hd, void* buf, uint8_t sec_cnt) {
   uint32_t size_in_byte;
   if (sec_cnt == 0) {
      size_in_byte = 256 * 512;
   }
   else {
      size_in_byte = sec_cnt * 512;
   }
   // word by word
   insw(reg_data(hd->my_channel), buf, size_in_byte / 2);
}

// (context: only when disk is ready)
// write mem buf to to (<= 256) sectors
static void write2sector(struct disk* hd, void* buf, uint8_t sec_cnt) {
   uint32_t size_in_byte;
   if (sec_cnt == 0) {
      size_in_byte = 256 * 512;
   }
   else {
      size_in_byte = sec_cnt * 512;
   }
   outsw(reg_data(hd->my_channel), buf, size_in_byte / 2);
}

// wait 30s (call it after intr?)
static bool busy_wait(struct disk* hd) {
   struct ide_channel* channel = hd->my_channel;
   // set timeout: 30s (only a approx)
   //       it's said thaht the timeout is based on doc 
   //       (so where is the fucking docs)
   uint16_t time_limit = 30 * 1000;
   // polling + sleep (better than polling only)
   while (time_limit -= 10 >= 0) {
      if (!(inb(reg_status(channel)) & BIT_STAT_BSY)) {
         return (inb(reg_status(channel)) & BIT_STAT_DRQ);
      }
      else {
         mtime_sleep(10);
      }
   }
   return false;
}

// read secs to buf
//    wait before read
//    select_disk -> select_sector -> cmd_out -> sema_down
//    weak: check again(busy_wait) -> read_from_sector
void ide_read(struct disk* hd, uint32_t lba, void* buf, uint32_t sec_cnt) {
   ASSERT(lba <= max_lba);
   ASSERT(sec_cnt > 0);

   // note: block the whole channel?
   //       so another disk is also block
   lock_acquire (&hd->my_channel->lock);

   select_disk(hd);
   uint32_t secs_op;		 // the secs to query (each time)
   uint32_t secs_done = 0;	 // total accumulators
   while(secs_done < sec_cnt) {
      if ((secs_done + 256) <= sec_cnt) {
         secs_op = 256; // 256 secs each time
      } 
      else {
         secs_op = sec_cnt - secs_done; // once
      }

      select_sector(hd, lba + secs_done, secs_op);
      cmd_out(hd->my_channel, CMD_READ_SECTOR);

      // wait for the intr handler
      sema_down(&hd->my_channel->disk_done);

      // disk should be ready now
      if (!busy_wait(hd)) { // fail
         char error[64];
         sprintf(error, "%s read sector %d failed!!!!!!\n", hd->name, lba);
         PANIC(error);
      }

      read_from_sector(hd, (void*)((uint32_t)buf + secs_done * 512), secs_op);
      secs_done += secs_op;
   }
   lock_release(&hd->my_channel->lock);
}

// write buf to secs
//    wait after write
//    select_disk -> select_sector -> cmd_out -> write2sectorw -> sema_down
//    weak: check again(busy_wait) -> read_from_sector
void ide_write(struct disk* hd, uint32_t lba, void* buf, uint32_t sec_cnt) {
   ASSERT(lba <= max_lba);
   ASSERT(sec_cnt > 0);
   lock_acquire (&hd->my_channel->lock);

   select_disk(hd);

   uint32_t secs_op;
   uint32_t secs_done = 0;

   while(secs_done < sec_cnt) {
      if ((secs_done + 256) <= sec_cnt) {
         secs_op = 256;
      } 
      else {
         secs_op = sec_cnt - secs_done;
      }

      select_sector(hd, lba + secs_done, secs_op);
      cmd_out(hd->my_channel, CMD_WRITE_SECTOR);

      if (!busy_wait(hd)) { // fail
         char error[64];
         sprintf(error, "%s write sector %d failed!!!!!!\n", hd->name, lba);
         PANIC(error);
      }
      write2sector(hd, (void*)((uint32_t)buf + secs_done * 512), secs_op);

      sema_down(&hd->my_channel->disk_done);
      secs_done += secs_op;
   }
   lock_release(&hd->my_channel->lock);
}

void intr_hd_handler(uint8_t irq_no) {
   ASSERT(irq_no == 0x2e || irq_no == 0x2f);
   uint8_t ch_no = irq_no - 0x2e;
   struct ide_channel* channel = &channels[ch_no];
   ASSERT(channel->irq_no == irq_no);

   // expecting_intr? lock enusre consistence
   if (channel->expecting_intr) {
      channel->expecting_intr = false;
      sema_up(&channel->disk_done);
      // consume the reg, to nofity the disk
      //       can also reset/reg_cmd
      //       otherise the disk will block
      inb(reg_status(channel));
   }
}

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
