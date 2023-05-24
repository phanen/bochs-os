#ifndef __KERNEL_MEMORY_H
#define __KERNEL_MEMORY_H

#include "stdint.h"
#include "bitmap.h"
#include "list.h"

// which pool to use
enum pool_flags {
   PF_KERNEL = 1,
   PF_USER = 2
};

#define	 PG_P_1	  1	// present in mem
#define	 PG_P_0	  0	// not present in mem
#define	 PG_RW_R  0	// r-x
#define	 PG_RW_W  2	// rwx
#define	 PG_US_S  0	// system
#define	 PG_US_U  4	// user

// block size: 16, 32, 64, 128, 256, 512, 1024
#define DESC_CNT  7	// num of mem desc

// alright... vaddr also need to be alloced
struct virtual_addr {
   struct bitmap vaddr_bitmap;
   uint32_t vaddr_start;
};

struct mem_block {
   struct list_elem free_elem;
};

struct mem_block_desc {
   uint32_t block_size;
   uint32_t blocks_per_arena; // num
   struct list free_list; // free list of mem_block
};

extern struct pool kernel_pool, user_pool;

void mem_init(void);

void* malloc_page(enum pool_flags pf, uint32_t pg_cnt);
void* get_kernel_pages(uint32_t pg_cnt);
void* get_user_pages(uint32_t pg_cnt);
void* get_a_page(enum pool_flags pf, uint32_t vaddr);

// good utilities, short but powerful
uint32_t* pte_ptr(uint32_t vaddr);
uint32_t* pde_ptr(uint32_t vaddr);
uint32_t addr_v2p(uint32_t vaddr);

#endif
