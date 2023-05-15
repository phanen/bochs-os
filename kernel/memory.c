#include "memory.h"
#include "stdint.h"
#include "print.h"

#define PG_SIZE 4096 // bytes

#define MEM_BITMAP_BASE 0xc009a000
// at most 512M
// 0xc009f000 = stack top of kernel main thread
// 0xc009e000 = pcb of kernel main thread
// 1 bitmap in 1 page -> 128 M (4096 * 8 bit * 4K/bit)
// at most 4 bitmap -> at most 512M allocation
// actually we only have 32M phy mem in total

// vaddr 3G + 1M
#define K_HEAP_START 0xc0100000

// pool for paddr
struct pool {
    struct bitmap pool_bitmap;
    uint32_t phy_addr_start;
    uint32_t pool_size; // total size
};

struct pool kernel_pool, user_pool; // paddr pool ofr kernel and user
struct virtual_addr kernel_vaddr; // vaddr pool for kernel

// init
static void mem_pool_init(uint32_t all_mem) {

    put_str("   mem_pool_init start\n");

    // calc free phy mem, pages, used phy mem
    uint32_t page_table_size = PG_SIZE * 256; // 256 -> num of page table: PD + 1PT(by PDE 0, 768) + 254PT(by PDE 769~1022)
    uint32_t used_mem = 0x100000 + page_table_size; // all mem used for now
    uint32_t free_mem = all_mem - used_mem;
    uint16_t all_free_pages = free_mem / PG_SIZE; // may not devidable, just ignore it

    // divide phy mem into two parts
    uint16_t kernel_free_pages = all_free_pages / 2;
    uint16_t user_free_pages = all_free_pages - kernel_free_pages;
    // calc the bytes of each bitmap (1 page -> 1 bit)
    uint32_t kbm_length = kernel_free_pages / 8;
    uint32_t ubm_length = user_free_pages / 8;
    // calc each base addr
    uint32_t kp_start = used_mem; // because we used from bottom to top...
    uint32_t up_start = kp_start + kernel_free_pages * PG_SIZE; // user pool

    kernel_pool.phy_addr_start = kp_start; // about 2M ?
    user_pool.phy_addr_start   = up_start; // about 17M ?

    kernel_pool.pool_size = kernel_free_pages * PG_SIZE;
    user_pool.pool_size = user_free_pages * PG_SIZE;

    kernel_pool.pool_bitmap.btmp_bytes_len = kbm_length;
    user_pool.pool_bitmap.btmp_bytes_len  = ubm_length;

    // alloc bitmap, 32M -> need only 1K phy mem...
    kernel_pool.pool_bitmap.bits = (void*)MEM_BITMAP_BASE;
    user_pool.pool_bitmap.bits = (void*)(MEM_BITMAP_BASE + kbm_length);

    // logging
    put_str("      kernel_pool_bitmap_start:");
    put_int((int)kernel_pool.pool_bitmap.bits); put_str("\n");

    put_str("      kernel_pool_bitmap_end:");
    put_int((int)kernel_pool.pool_bitmap.bits + kernel_pool.pool_bitmap.btmp_bytes_len); put_str("\n");

    put_str("       kernel_pool_phy_addr_start:");
    put_int(kernel_pool.phy_addr_start); put_str("\n");

    put_str("       kernel_pool_phy_addr_end:");
    put_int(kernel_pool.phy_addr_start + kernel_pool.pool_size); put_str("\n");

    put_str("      user_pool_bitmap_start:");
    put_int((int)user_pool.pool_bitmap.bits); put_str("\n");

    put_str("      user_pool_bitmap_end:");
    put_int((int)user_pool.pool_bitmap.bits + user_pool.pool_bitmap.btmp_bytes_len); put_str("\n");

    put_str("       user_pool_phy_addr_start:");
    put_int(user_pool.phy_addr_start); put_str("\n");
    put_str("       user_pool_phy_addr_end:");
    put_int(user_pool.phy_addr_start + user_pool.pool_size); put_str("\n");

    // init all bit to zero(free)
    bitmap_init(&kernel_pool.pool_bitmap); 
    bitmap_init(&user_pool.pool_bitmap);


    // vaddr
    kernel_vaddr.vaddr_bitmap.btmp_bytes_len = kbm_length;

    kernel_vaddr.vaddr_bitmap.bits = (void*)(MEM_BITMAP_BASE + kbm_length + ubm_length);
    kernel_vaddr.vaddr_start = K_HEAP_START;

    put_str("     kernel_vaddr.vaddr_bitmap.start:");
    put_int((int)kernel_vaddr.vaddr_bitmap.bits); put_str("\n");
    put_str("     kernel_vaddr.vaddr_bitmap.end:");
    put_int((int)kernel_vaddr.vaddr_bitmap.bits + kernel_vaddr.vaddr_bitmap.btmp_bytes_len); put_str("\n");

    bitmap_init(&kernel_vaddr.vaddr_bitmap);
    put_str("   mem_pool_init done\n");
}

void mem_init() {
    put_str("mem_init start\n");
    uint32_t mem_bytes_total = (*(uint32_t*)(0xb00)); // from total_mem_bytes
    mem_pool_init(mem_bytes_total);
    put_str("mem_init done\n");
}
