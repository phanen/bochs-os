#include "process.h"
#include "global.h"
#include "debug.h"
#include "memory.h"
#include "thread.h"
#include "list.h"
#include "tss.h"
#include "interrupt.h"
#include "string.h"
#include "console.h"

extern void intr_exit(void);

// process_execute
//    -> get_kernel_pages
//    -> init_tcb
//    -> create_user_vaddr_bitmap
//    -> create_page_dir

// schedule
//     -> process_activate
//      -> kernel_thread
//        -> start_process
//          -> intr_exit
//            -> user_prog

// init context
void start_process(void* userprog) {
  void* function = userprog;

  // point to intr_stack
  struct task_struct* cur = running_thread();
  cur->self_kstack += sizeof(struct thread_stack);
  struct intr_stack* proc_stack = (struct intr_stack*)cur->self_kstack;

  proc_stack->edi = proc_stack->esi = proc_stack->ebp = proc_stack->esp_dummy = 0;
  proc_stack->ebx = proc_stack->edx = proc_stack->ecx = proc_stack->eax = 0;
  proc_stack->gs = 0; // ban
  proc_stack->ds = proc_stack->es = proc_stack->fs = SELECTOR_U_DATA;
  proc_stack->eip = function;
  proc_stack->cs = SELECTOR_U_CODE;
  proc_stack->eflags = (EFLAGS_IOPL_0 | EFLAGS_MBS | EFLAGS_IF_1);
  proc_stack->esp = (void*)((uint32_t)get_a_page(PF_USER, USER_STACK3_VADDR) + PG_SIZE) ;
  proc_stack->ss = SELECTOR_U_DATA;

  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (proc_stack) : "memory");
}

// update PD addr in cr3
void page_dir_activate(struct task_struct* p_thread) {
  uint32_t pagedir_phy_addr;
  if (p_thread->pgdir != NULL)	{ // user
    pagedir_phy_addr = addr_v2p((uint32_t)p_thread->pgdir);
  }
  else { // kernel
    pagedir_phy_addr = 0x100000;
  }

  // update cr3
  asm volatile ("movl %0, %%cr3" : : "r" (pagedir_phy_addr) : "memory");
}

// page activate and update tss->esp0
//    only effective for proc
//    for thread, it run like nothing happend
void process_activate(struct task_struct* p_thread) {
  ASSERT(p_thread != NULL);
  page_dir_activate(p_thread);

  if (p_thread->pgdir) {
    update_tss_esp(p_thread);
  }
}

// create pt (copy current kernel pde)
uint32_t* create_page_dir(void) {

  // malloc a page (in kernel space) for user's PD
  uint32_t* page_dir_vaddr = get_kernel_pages(1);
  if (page_dir_vaddr == NULL) {
    console_put_str("create_page_dir: get_kernel_page failed!");
    return NULL;
  }

  // copy: kernel pde -> user pde
  memcpy((uint32_t*)((uint32_t)page_dir_vaddr + 0x300*4), (uint32_t*)(0xfffff000+0x300*4), 1024);

  // diff: only change the last pde, point to the user's pd
  uint32_t new_page_dir_phy_addr = addr_v2p((uint32_t)page_dir_vaddr);
  page_dir_vaddr[1023] = new_page_dir_phy_addr | PG_US_U | PG_RW_W | PG_P_1;
  return page_dir_vaddr;
}

void create_user_vaddr_bitmap(struct task_struct* user_prog) {
  user_prog->userprog_vaddr.vaddr_start = USER_VADDR_START;
  uint32_t bitmap_pg_cnt = DIV_ROUND_UP((0xc0000000 - USER_VADDR_START) / PG_SIZE / 8 , PG_SIZE);
  user_prog->userprog_vaddr.vaddr_bitmap.bits = get_kernel_pages(bitmap_pg_cnt);
  user_prog->userprog_vaddr.vaddr_bitmap.btmp_bytes_len = (0xc0000000 - USER_VADDR_START) / PG_SIZE / 8;
  bitmap_init(&user_prog->userprog_vaddr.vaddr_bitmap);
}

// create proc
void process_execute(void* userprog, char* name) {

  struct task_struct* thread = get_kernel_pages(1);
  init_task(thread, name, default_prio);
  create_user_vaddr_bitmap(thread);
  thread_stack_init(thread, start_process, userprog); // start_process(userprog)
  thread->pgdir = create_page_dir();
  block_desc_init(thread->u_block_desc);

  enum intr_status old_status = intr_disable();

  ASSERT(!elem_find(&thread_ready_list, &thread->general_tag));
  list_append(&thread_ready_list, &thread->general_tag);

  ASSERT(!elem_find(&thread_all_list, &thread->all_list_tag));
  list_append(&thread_all_list, &thread->all_list_tag);

  intr_set_status(old_status);
}
