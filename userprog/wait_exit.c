#include "wait_exit.h"
#include "global.h"
#include "debug.h"
#include "pipe.h"
#include "thread.h"
#include "list.h"
#include "stdio-kernel.h"
#include "memory.h"
#include "bitmap.h"
#include "fs.h"
#include "file.h"

// free ppage -> free bitmap -> close all files
//    care nothing about vpage
static void release_prog_resource(struct task_struct* release_thread) {

  uint32_t* pgdir_vaddr = release_thread->pgdir;

  uint16_t user_pde_nr = 768;
  uint16_t user_pte_nr = 1024;
  uint32_t pte = 0;
  uint32_t* v_pte_ptr = NULL;

  uint32_t* cur_ptable = NULL;
  uint32_t pg_phy_addr = 0;

  // free all ppages (alloc by bitmap)
  //    just search over page table
  for (uint16_t pde_i = 0; pde_i < user_pde_nr; pde_i++) {

    uint32_t cur_pde = *(pgdir_vaddr + pde_i);

    if (!(cur_pde & 0x00000001))
      continue;

    cur_ptable = pte_ptr(pde_i << 22);

    for (uint16_t pte_i = 0; pte_i < user_pte_nr; ++pte_i) {
      pte = *(cur_ptable + pte_i);
      if (!(pte & 0x00000001)) 
        continue;

      // free page frame
      pg_phy_addr = pte & 0xfffff000;
      free_a_phy_page(pg_phy_addr);
    }

    // free page frame for page table
    pg_phy_addr = cur_pde & 0xfffff000;
    free_a_phy_page(pg_phy_addr);
  }

  // free bitmap (recall that bitmap is alloced in memory)
  uint32_t bitmap_pg_cnt = (release_thread->userprog_vaddr.vaddr_bitmap.btmp_bytes_len) / PG_SIZE;
  uint8_t* user_vaddr_pool_bitmap = release_thread->userprog_vaddr.vaddr_bitmap.bits;
  mfree_page(PF_KERNEL, user_vaddr_pool_bitmap, bitmap_pg_cnt);


  // close all file
  //      parent's file? links count?
  for (uint8_t fd_i = 3; fd_i < MAX_FILES_OPEN_PER_PROC; fd_i++) {

    if (release_thread->fd_table[fd_i] == -1) {
      continue;
    }

    if (is_pipe(fd_i)) {
      int32_t global_fd = fd_local2global(fd_i);
      if (--file_table[global_fd].fd_pos == 0){
        mfree_page(PF_KERNEL, file_table[global_fd].fd_inode, 1);
        file_table[global_fd].fd_inode = NULL;
      }
      continue;
    }

    sys_close(fd_i);
  }
}

// callback
static bool find_child(struct list_elem* pelem, int32_t ppid) {
  struct task_struct* pthread = elem2entry(struct task_struct, all_list_tag, pelem);
  if (pthread->parent_pid == ppid) {
    return true;
  }
  return false;
}

static bool find_hanging_child(struct list_elem* pelem, int32_t ppid) {
  struct task_struct* pthread = elem2entry(struct task_struct, all_list_tag, pelem);
  if (pthread->parent_pid == ppid && pthread->status == TASK_HANGING) {
    return true;
  }
  return false;
}

static bool init_adopt_a_child(struct list_elem* pelem, int32_t pid) {
  struct task_struct* pthread = elem2entry(struct task_struct, all_list_tag, pelem);
  if (pthread->parent_pid == pid) {
    pthread->parent_pid = 1; // init
  }
  return false;
}

// get exit status of child
//    return pid or -1 (fail)
pid_t sys_wait(int32_t* status) {
  struct task_struct* pthread = running_thread();

  while(1) {

    struct list_elem* child_elem = list_traversal(&thread_all_list, find_hanging_child, pthread->pid);

    // find a hanging child, then free it
    if (child_elem != NULL) {
      struct task_struct* child_thread = elem2entry(struct task_struct, all_list_tag, child_elem);
      *status = child_thread->exit_status;

      uint16_t child_pid = child_thread->pid;

      thread_exit(child_thread, false);
      return child_pid;
    }


    child_elem = list_traversal(&thread_all_list, find_child, pthread->pid);

    // no child...
    if (child_elem == NULL) {
      return -1;
    }
      // just no hanging child
    else {
      thread_block(TASK_WAITING);
    }
  }
}

// child to init -> release -> wake up parent
void sys_exit(int32_t status) {
  struct task_struct* pthread = running_thread();

  pthread->exit_status = status;
  if (pthread->parent_pid == -1) { //
    PANIC("sys_exit: child_thread->parent_pid is -1\n");
  }

  // for all child (i.e. whose parent is pthread->pid)
  list_traversal(&thread_all_list, init_adopt_a_child, pthread->pid);

  release_prog_resource(pthread);

  // if parent `wait`
  struct task_struct* parent_thread = pid2thread(pthread->parent_pid);
  if (parent_thread->status == TASK_WAITING) {
    thread_unblock(parent_thread);
  }

  thread_block(TASK_HANGING);
}
