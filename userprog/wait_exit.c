#include "wait_exit.h"
#include "global.h"
#include "debug.h"
#include "thread.h"
#include "list.h"
#include "stdio-kernel.h"
#include "memory.h"
#include "bitmap.h"
#include "fs.h"

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
