#include "memory.h"
#include "console.h"
#include "stdint.h"
#include "global.h"
#include "debug.h"
#include "print.h"
#include "string.h"
#include "sync.h"
#include "interrupt.h"

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
    struct lock lock;
};

// meta info for mem block
struct arena {
    struct mem_block_desc* desc;
    uint32_t cnt;
    //  for large:      frame num
    //  for non-large:  free mem_block
    bool large;
};


struct pool kernel_pool, user_pool; // paddr pool ofr kernel and user
struct virtual_addr kernel_vaddr; // vaddr pool for kernel

// kernel mem block desc
struct mem_block_desc k_block_descs[DESC_CNT];

// init a mem bock desc array
void block_desc_init(struct mem_block_desc* desc_array) {

    uint16_t block_size = 16;

    for (uint16_t i = 0; i < DESC_CNT; i++) {
        desc_array[i].block_size = block_size;
        desc_array[i].blocks_per_arena = (PG_SIZE - sizeof(struct arena)) / block_size;
        list_init(&desc_array[i].free_list);
        block_size <<= 1;
    }
}

// request `pg_cnt` pages from vaddr pool
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
    }
    else {
        struct task_struct* cur = running_thread();
        bit_idx_start  = bitmap_scan(&cur->userprog_vaddr.vaddr_bitmap, pg_cnt);
        if (bit_idx_start == -1) {
            return NULL;
        }

        while(cnt < pg_cnt) {
            bitmap_set(&cur->userprog_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 1);
        }
        vaddr_start = cur->userprog_vaddr.vaddr_start + bit_idx_start * PG_SIZE;
        ASSERT((uint32_t)vaddr_start < (0xc0000000 - PG_SIZE));
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
// NOTE: when only pvaddr appear, it must be pure calculation
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

void* get_kernel_pages(uint32_t pg_cnt) {
    lock_acquire(&kernel_pool.lock);
    void* vaddr =  malloc_page(PF_KERNEL, pg_cnt);
    if (vaddr != NULL) {
        memset(vaddr, 0, pg_cnt * PG_SIZE);
    }
    lock_release(&kernel_pool.lock);
    return vaddr;
}

// user malloc by pages
void* get_user_pages(uint32_t pg_cnt) {
    lock_acquire(&user_pool.lock);
    void* vaddr = malloc_page(PF_USER, pg_cnt);
    if (vaddr != NULL) { // clean the page frame
        memset(vaddr, 0, pg_cnt * PG_SIZE);
    }
    memset(vaddr, 0, pg_cnt * PG_SIZE);
    lock_release(&user_pool.lock);
    return vaddr;
}

// malloc, but use specific vaddr
void* get_a_page(enum pool_flags pf, uint32_t vaddr) {
    struct pool* mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;
    lock_acquire(&mem_pool->lock);

    struct task_struct* cur = running_thread();
    int32_t bit_idx = -1;

    if (cur->pgdir != NULL && pf == PF_USER) {
        bit_idx = (vaddr - cur->userprog_vaddr.vaddr_start) / PG_SIZE;
        ASSERT(bit_idx > 0);
        bitmap_set(&cur->userprog_vaddr.vaddr_bitmap, bit_idx, 1);

    }
    else if (cur->pgdir == NULL && pf == PF_KERNEL){
        bit_idx = (vaddr - kernel_vaddr.vaddr_start) / PG_SIZE;
        ASSERT(bit_idx > 0);
        bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx, 1);
    }
    else {
        PANIC("get_a_page: not allow kernel alloc userspace or user alloc kernelspace by get_a_page");
    }

    void* page_phyaddr = palloc(mem_pool);
    if (page_phyaddr == NULL) {
        return NULL;
    }

    page_table_add((void*)vaddr, page_phyaddr);
    lock_release(&mem_pool->lock);
    return (void*)vaddr;
}

// yet another `get_a_page` (for fork)
//      do not need bitmap manipulation
//      if we copy bitmap before (in fork)
void* get_a_page_without_opvaddrbitmap(enum pool_flags pf, uint32_t vaddr) {
   struct pool* mem_pool = pf & PF_KERNEL ? &kernel_pool : &user_pool;

   lock_acquire(&mem_pool->lock);
   void* page_phyaddr = palloc(mem_pool);
   if (page_phyaddr == NULL) {
      lock_release(&mem_pool->lock);
      return NULL;
   }
   page_table_add((void*)vaddr, page_phyaddr);

   lock_release(&mem_pool->lock);
   return (void*)vaddr;
}

// vaddr to paddr (get its pte, then read the pte)
uint32_t addr_v2p(uint32_t vaddr) {
    uint32_t* pte = pte_ptr(vaddr);
    return ((*pte & 0xfffff000) + (vaddr & 0x00000fff));
}

// change bitmap
// pmem is maintained by kernel
void pfree(uint32_t pg_phy_addr) {
    struct pool* mem_pool;
    uint32_t bit_idx = 0;
    if (pg_phy_addr >= user_pool.phy_addr_start) {
        mem_pool = &user_pool;
        bit_idx = (pg_phy_addr - user_pool.phy_addr_start) / PG_SIZE;
    }
    else {
        mem_pool = &kernel_pool;
        bit_idx = (pg_phy_addr - kernel_pool.phy_addr_start) / PG_SIZE;
    }
    bitmap_set(&mem_pool->pool_bitmap, bit_idx, 0);
}

// rm pte only (pde is not easy to trace)
static void page_table_pte_remove(uint32_t vaddr) {
    uint32_t* pte = pte_ptr(vaddr);
    *pte &= ~PG_P_1;
    asm volatile ("invlpg %0"::"m" (vaddr):"memory"); // flush tlb
}

// free vpages like what `vaddr_get` alloc
//      vmem is maintained by pcb
static void vaddr_remove(enum pool_flags pf, void* _vaddr, uint32_t pg_cnt) {
    uint32_t bit_idx_start = 0, vaddr = (uint32_t)_vaddr, cnt = 0;

    if (pf == PF_KERNEL) {
        bit_idx_start = (vaddr - kernel_vaddr.vaddr_start) / PG_SIZE;
        while(cnt < pg_cnt) {
            bitmap_set(&kernel_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 0);
        }
    }
    else {
        struct task_struct* cur_thread = running_thread();
        bit_idx_start = (vaddr - cur_thread->userprog_vaddr.vaddr_start) / PG_SIZE;
        while(cnt < pg_cnt) {
            bitmap_set(&cur_thread->userprog_vaddr.vaddr_bitmap, bit_idx_start + cnt++, 0);
        }
    }
}

// free `pg_cnt` pages
// pfree -> page_table_pte_remove -> vaddr_remove
// malloc_page: vaddr_get -> palloc -> page_table_add
void mfree_page(enum pool_flags pf, void* _vaddr, uint32_t pg_cnt) {

    uint32_t pg_phy_addr;
    uint32_t vaddr = (int32_t)_vaddr, page_cnt = 0;

    // vaddr align
    ASSERT(pg_cnt >=1 && vaddr % PG_SIZE == 0);
    pg_phy_addr = addr_v2p(vaddr);

    // paddr of PT align and based after 1M + (1k PD) + (1k PT for kernel)
    ASSERT((pg_phy_addr % PG_SIZE) == 0 && pg_phy_addr >= 0x102000);

    // kernel or user
    if (pg_phy_addr >= user_pool.phy_addr_start) {
        vaddr -= PG_SIZE;
        while (page_cnt < pg_cnt) {
            vaddr += PG_SIZE;
            pg_phy_addr = addr_v2p(vaddr);
            ASSERT((pg_phy_addr % PG_SIZE) == 0 && pg_phy_addr >= user_pool.phy_addr_start);

            pfree(pg_phy_addr);
            page_table_pte_remove(vaddr);
            page_cnt++;
        }
        vaddr_remove(pf, _vaddr, pg_cnt);

    }
    else {
        vaddr -= PG_SIZE;
        while (page_cnt < pg_cnt) {
            vaddr += PG_SIZE;
            pg_phy_addr = addr_v2p(vaddr);
            ASSERT((pg_phy_addr % PG_SIZE) == 0 && \
                   pg_phy_addr >= kernel_pool.phy_addr_start && \
                   pg_phy_addr < user_pool.phy_addr_start);

            pfree(pg_phy_addr);
            page_table_pte_remove(vaddr);

            page_cnt++;
        }
        vaddr_remove(pf, _vaddr, pg_cnt);
    }
}

// init
static void mem_pool_init(uint32_t all_mem) {

    put_str("   mem_pool_init start\n");

    // calc free phy mem, pages, used phy mem
    //      256 -> num of page table: PD + 1PT(by PDE 0, 768) + 254PT(by PDE 769~1022)
    uint32_t page_table_size = PG_SIZE * 256;
    uint32_t used_mem = 0x100000 + page_table_size; // all mem used for now
    uint32_t free_mem = all_mem - used_mem;
    uint16_t all_free_pages = free_mem / PG_SIZE; // not devidable -> ignore it

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

    lock_init(&kernel_pool.lock);
    lock_init(&user_pool.lock);

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


// idx-th block in arena
static struct mem_block* arena2block(struct arena* a, uint32_t idx) {
    return (struct mem_block*)((uint32_t)a + sizeof(struct arena) + idx * a->desc->block_size);
}

// inversely
static struct arena* block2arena(struct mem_block* b) {
    return (struct arena*)((uint32_t)b & 0xfffff000);
}

// god-like
void* sys_malloc(uint32_t size) {

    struct mem_block_desc* descs;

    enum pool_flags PF;
    struct pool* mem_pool;
    uint32_t pool_size;

    struct task_struct* cur_thread = running_thread();

    // kernel or user
    if (cur_thread->pgdir == NULL) {
        PF = PF_KERNEL;
        pool_size = kernel_pool.pool_size;
        mem_pool = &kernel_pool;
        descs = k_block_descs;
    }
    else {
        PF = PF_USER;
        pool_size = user_pool.pool_size;
        mem_pool = &user_pool;
        descs = cur_thread->u_block_desc;
    }

    // invalid size
    if (!(size > 0 && size < pool_size)) {
        return NULL;
    }

    struct arena* a;
    struct mem_block* b;

    lock_acquire(&mem_pool->lock);

    // large block -> whole page
    if (size > 1024) {
        uint32_t page_cnt = DIV_ROUND_UP(size + sizeof(struct arena), PG_SIZE);

        a = malloc_page(PF, page_cnt);

        if (a != NULL) {
            memset(a, 0, page_cnt * PG_SIZE); // TODO: or not?

            // the header
            a->desc = NULL;
            a->cnt = page_cnt;
            a->large = true;

            lock_release(&mem_pool->lock);
            return (void*)(a + 1); // after header
        }
        else {
            lock_release(&mem_pool->lock);
            return NULL;
        }
    }
        // small block -> adaptive
    else {
        uint8_t desc_i;

        // find the first 2^k > size
        for (desc_i = 0; desc_i < DESC_CNT; desc_i++) {
            if (size <= descs[desc_i].block_size) {
                break;
            }
        }

        // if no free block in current arena
        //      create a new arena mem_block
        if (list_empty(&descs[desc_i].free_list)) {

            a = malloc_page(PF, 1); // new arena
            if (a == NULL) {
                lock_release(&mem_pool->lock);
                return NULL;
            }
            memset(a, 0, PG_SIZE);

            a->desc = &descs[desc_i]; // common desc
            a->large = false;
            a->cnt = descs[desc_i].blocks_per_arena;

            uint32_t block_i;

            enum intr_status old_status = intr_disable();

            // divided arena into small blocks
            // update free_list (must iterate to fill the free_list... seems dummy)
            for (block_i = 0; block_i < descs[desc_i].blocks_per_arena; block_i++) {
                b = arena2block(a, block_i);
                ASSERT(!elem_find(&a->desc->free_list, &b->free_elem));
                list_append(&a->desc->free_list, &b->free_elem);
            }

            intr_set_status(old_status);
        }

        // time to alloc
        //      alloc: list_tag -> block
        b = elem2entry(struct mem_block, free_elem, list_pop(&(descs[desc_i].free_list))); // type -> namae -> target
        memset(b, 0, descs[desc_i].block_size);
        //      update the arena header: block -> arena
        a = block2arena(b);
        a->cnt--;

        lock_release(&mem_pool->lock);
        return (void*)b;
    }
}

void sys_free(void* ptr) {
    ASSERT(ptr != NULL);

    if (ptr != NULL) {
        enum pool_flags PF;
        struct pool* mem_pool;

        // kernel thread or user process
        if (running_thread()->pgdir == NULL) {
            ASSERT((uint32_t)ptr >= K_HEAP_START);
            PF = PF_KERNEL;
            mem_pool = &kernel_pool;
        }
        else {
            PF = PF_USER;
            mem_pool = &user_pool;
        }

        lock_acquire(&mem_pool->lock);

        struct mem_block* b = ptr;
        // get the meta info of blk
        struct arena* a = block2arena(b);
        ASSERT(a->large == 0 || a->large == 1);

        // large blk
        if (a->desc == NULL && a->large == true) {
            mfree_page(PF, a, a->cnt);
        }
        // small blk
        else {
            // free list
            list_append(&a->desc->free_list, &b->free_elem);

            // arena has been empty?
            // clean the free_list (seems a little dummy...)
            //      if alloc a new blk, we remove free_list
            //      if free the arena, we also remove free_list
            if (++a->cnt == a->desc->blocks_per_arena) {
                uint32_t block_i;
                for (block_i = 0; block_i < a->desc->blocks_per_arena; block_i++) {
                    struct mem_block*  b = arena2block(a, block_i);
                    ASSERT(elem_find(&a->desc->free_list, &b->free_elem));
                    list_remove(&b->free_elem);
                }
                mfree_page(PF, a, 1);
            }
        }
        lock_release(&mem_pool->lock);
    }
}

void mem_init() {
    put_str("mem_init start\n");
    uint32_t mem_bytes_total = (*(uint32_t*)(0xb00)); // from total_mem_bytes
    mem_pool_init(mem_bytes_total);
    block_desc_init(k_block_descs);
    put_str("mem_init done\n");
}
