#ifndef __DEVICE_IDE_H
#define __DEVICE_IDE_H

#include "stdint.h"
#include "sync.h"
#include "bitmap.h"

struct partition {
	uint32_t start_lba; // absolute in `struct disk`, but relative in disk
	uint32_t sec_cnt;

	struct disk *my_disk; // from which disk
	struct list_elem part_tag; // to index the partition in a list
	char name[8];

	struct super_block *sb; // interesting: we don't define it before

	struct bitmap block_bitmap;
	struct bitmap inode_bitmap;

	struct list open_inodes; // open i node
};

struct disk {
	char name[8]; // e.g. sda
	struct ide_channel *my_channel;
	uint8_t dev_no; // master 0, slave 1
	struct partition prim_parts[4];
	struct partition logic_parts[8]; // infinity, but we limit it to 8
};

struct ide_channel {
	char name[8];
	uint16_t port_base;
	uint8_t irq_no;
	struct lock lock;
	bool expecting_intr; // waiting for intr from disk
	struct semaphore disk_done; // block/weak the drive
	struct disk devices[2]; // master + slave
};

extern uint8_t chan_cnt;
extern struct ide_channel channels[];
extern struct list partition_list; // mount by fs

void ide_init(void);

void ide_read(struct disk *hd, uint32_t lba, void *buf, uint32_t sec_cnt);
void ide_write(struct disk *hd, uint32_t lba, void *buf, uint32_t sec_cnt);

void intr_hd_handler(uint8_t irq_no);

#endif
