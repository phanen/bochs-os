#include "pid.h"
#include "sync.h"
#include "bitmap.h"

uint8_t pid_bitmap_bits[128] = { 0 };

static struct lock pid_lock;

struct pid_pool {
	struct bitmap pid_bitmap;
	uint32_t pid_start;
	struct lock pid_lock;
} pid_pool;

void pid_pool_init()
{
	pid_pool.pid_start = 1;
	pid_pool.pid_bitmap.bits = pid_bitmap_bits;
	pid_pool.pid_bitmap.btmp_bytes_len = 128;
	bitmap_init(&pid_pool.pid_bitmap);
	lock_init(&pid_pool.pid_lock);
}

pid_t allocate_pid()
{
	lock_acquire(&pid_pool.pid_lock);
	int32_t bit_idx = bitmap_scan(&pid_pool.pid_bitmap, 1);
	bitmap_set(&pid_pool.pid_bitmap, bit_idx, 1);
	lock_release(&pid_pool.pid_lock);
	return (bit_idx + pid_pool.pid_start);
}

void release_pid(pid_t pid)
{
	lock_acquire(&pid_pool.pid_lock);
	int32_t bit_idx = pid - pid_pool.pid_start;
	bitmap_set(&pid_pool.pid_bitmap, bit_idx, 0);
	lock_release(&pid_pool.pid_lock);
}
