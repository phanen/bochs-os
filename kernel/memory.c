#include "memory.h"
#include "stdint.h"
#include "global.h"
#include "debug.h"
#include "print.h"
#include "string.h"

#define NULL  ((void *) 0)

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

// mask to get index of PDE, PTE
#define PDE_IDX(addr) ((addr & 0xffc00000) >> 22)
#define PTE_IDX(addr) ((addr & 0x003ff000) >> 12)

// pool for paddr
struct pool {
    struct bitmap pool_bitmap;
    uint32_t phy_addr_start;
    uint32_t pool_size; // total size
};

struct pool kernel_pool, user_pool; // paddr pool ofr kernel and user
struct virtual_addr kernel_vaddr; // vaddr pool for kernel

// request `pg_cnt` page from vaddr pool
static void* vaddr_get(enum pool_flags pf, uint32_t pg_cnt) {
    int vaddr_start = 0, bit_idx_start = -1;
    uint32_t cnt = 0;
    if (pf == PF_KERNEL) {
        bit_idx_start  = bitmap_scan(&kernel_vaddr.vaddr_bitmap, pg_cnt);
        if (bit_idx_start == -1) {
            return NULL;
        }
        // set used bits
        while(cnt < pg_cnt) {
            bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 1);
        }
        // bit id to mem vaddr
        vaddr_start = kernel_vaddr.vaddr_start + bit_idx_start * PG_SIZE;
    } else {
        // TODO: user vaddr pool
    }
    return (void*)vaddr_start;
}

// from a vaddr, get its correspondent PTE ptr
// valid only if its PDE exist...
uint32_t* pte_ptr(uint32_t vaddr) {
    // first, 0xffc00000 to locate the PD
    // second, (vaddr&0xffc00000)>>10 to locate the PT
    // then we only care about the PTE id
    uint32_t* pte = (uint32_t*)(0xffc00000 + \
        ((vaddr & 0xffc00000) >> 10) + \
        PTE_IDX(vaddr) * 4);
    return pte;
}

// from a vaddr, get its correspondent PDE ptr
// always valid? even if vaddr is not mapped
uint32_t* pde_ptr(uint32_t vaddr) {
    // 0xfffff000 to locate the PD, and the last PDE (i.e. PD itself)
    // then we only care about the PDE id
    uint32_t* pde = (uint32_t*)((0xfffff000) + PDE_IDX(vaddr) * 4);
    return pde;
}

// alloc a phy page
static void* palloc(struct pool* m_pool) {
    int bit_idx = bitmap_scan(&m_pool->pool_bitmap, 1); // atom?
    if (bit_idx == -1 ) {
        return NULL;
    }
    bitmap_set(&m_pool->pool_bitmap, bit_idx, 1);
    uint32_t page_phyaddr = ((bit_idx * PG_SIZE) + m_pool->phy_addr_start);
    return (void*)page_phyaddr;
}

// map one vpage to one ppage(page frame), i.e. vaddr to paddr
// set PDE -> set PTE, if PDE not exist, palloc then map
// PTE should not exist (only `add`, not change)
static void page_table_add(void* _vaddr, void* _page_phyaddr) {
    uint32_t vaddr = (uint32_t)_vaddr, page_phyaddr = (uint32_t)_page_phyaddr;
    uint32_t* pde = pde_ptr(vaddr); // good utility
    uint32_t* pte = pte_ptr(vaddr); // good utility

    // PDE exist?
    if (*pde & 0x00000001) {
        ASSERT(!(*pte & 0x00000001)); // assert PTE not exist
        if (!(*pte & 0x00000001)) { // normally not exist
            *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
        } else { // PTE exist, should not happend
            PANIC("pte repeat");
            *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);
        }
    } else { // PDE not exist
        // first alloc its PT, assign the PDE
        uint32_t pde_phyaddr = (uint32_t)palloc(&kernel_pool);
        *pde = (pde_phyaddr | PG_US_U | PG_RW_W | PG_P_1);

        // NOTE: PTE is invalid if PDE not exist before
        pte = pte_ptr(vaddr); // update, now pte is a vaddr point into new `pde_phyaddr`

        memset((void*)((int)pte & 0xfffff000), 0, PG_SIZE); // ensure the new alloc PT is clean

        ASSERT(!(*pte & 0x00000001));
        *pte = (page_phyaddr | PG_US_U | PG_RW_W | PG_P_1);      // US=1,RW=1,P=1
    }
}

// alloc `pg_cnt` pages, return vaddr (i.e. malloc by pages)
// vaddr_get -> palloc -> page_table_add
//  pages is continuous in vaddr, 
//  but can be not continuous in paddr
void* malloc_page(enum pool_flags pf, uint32_t pg_cnt) {
    // should be alloc all space
    ASSERT(pg_cnt > 0 && pg_cnt < 3840); // 15M/4K = 3840 page (TODO: 3840 can be dynamic...)

    // alloc vpages
    void* vaddr_start = vaddr_get(pf, pg_cnt);
    if (vaddr_start == NULL) {
        return NULL;
    }
    uint32_t vaddr = (uint32_t)vaddr_start, cnt = pg_cnt;
    struct pool* mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;

    // alloc ppages then associte vpages, one by one
    while (cnt--) {
        void* page_phyaddr = palloc(mem_pool);
        if (page_phyaddr == NULL) { // TODO: better handle, need roll back all alloced vaddr and paddr
            return NULL;
        }
        page_table_add((void*)vaddr, page_phyaddr); // happily map
        vaddr += PG_SIZE;
    }
    return vaddr_start;
}

// kernel malloc by pages
void* get_kernel_pages(uint32_t pg_cnt) {
    void* vaddr =  malloc_page(PF_KERNEL, pg_cnt);
    if (vaddr != NULL) { // clean the page frame
        memset(vaddr, 0, pg_cnt * PG_SIZE);
    }
    return vaddr;
}

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