#include "thread.h"
#include "stdint.h"
#include "string.h"
#include "global.h"
#include "memory.h"
#include "interrupt.h"
#include "debug.h"
#include "print.h"
#include "process.h"
#include "sync.h"

static struct task_struct* main_thread;    // TCB for main thread (take `main()` as a thread)
static struct task_struct* idle_thread;    // a dummy never exit

struct list thread_ready_list;  // TASK_READY task
struct list thread_all_list;    // all tasks

static struct list_elem* thread_tag; // just a buffer
//  TODO: why not use local var?

struct lock pid_lock;

extern void switch_to(struct task_struct* cur, struct task_struct* next);

// alloc pid for proc
static pid_t allocate_pid() {
  static pid_t next_pid = 0;
  lock_acquire(&pid_lock);
  next_pid++;
  lock_release(&pid_lock);
  return next_pid;
}

// get the TCB of running thread
struct task_struct* running_thread() {
  uint32_t esp;
  asm ("mov %%esp, %0": "=g" (esp)); // get esp
  // get the page start of stack top (i.e. the addr of TCB)
  return (struct task_struct*)(esp & 0xfffff000);
}

// a entry which provides a eip addr
//    bootstrap
static void kernel_thread(thread_func* function, void* func_arg) {
  intr_enable(); // !!!
  function(func_arg);
}

// init thread stack
void thread_stack_init(struct task_struct* pthread, thread_func function, void* func_arg) {

  // preserve the intr stack
  pthread->self_kstack -= sizeof(struct intr_stack);

  // set kernel thread stack
  pthread->self_kstack -= sizeof(struct thread_stack);
  struct thread_stack* kthread_stack = (struct thread_stack*)pthread->self_kstack;
  kthread_stack->eip = kernel_thread; // make the thread bootstrap to `kernel_thread`
  kthread_stack->function = function;
  kthread_stack->func_arg = func_arg;
  kthread_stack->ebp = 0;
  kthread_stack->ebx = 0;
  kthread_stack->esi = 0;
  kthread_stack->edi = 0;
}

// init basic info of pcb
void init_task(struct task_struct* pthread, char* name, int prio) {

  // the pcb/tcb should be valid
  ASSERT(pthread != NULL);

  memset(pthread, 0, sizeof(*pthread));
  strcpy(pthread->name, name);

  pthread->pid = allocate_pid();

  if (pthread == main_thread) {
    pthread -> status = TASK_RUNNING;
  }
  else {
    pthread->status = TASK_READY;
  }

  pthread->priority = prio;

  // top of TCB page
  pthread->self_kstack = (uint32_t*)((uint32_t)pthread + PG_SIZE);

  pthread->ticks = prio;
  pthread->elapsed_ticks = 0;

  pthread->pgdir = NULL;

  pthread->stack_magic = 0x20021225; // magic number
}

// create a thread (alloc a TCB page ->  init TCB -> init stack)
struct task_struct* thread_create(char* name, int prio, thread_func function, void* func_arg) {

  // all TCBs (include user TCBs) are in kernel space
  // (because we implement kernel-level thread?)
  struct task_struct* thread = get_kernel_pages(1);

  init_task(thread, name, prio);

  thread_stack_init(thread, function, func_arg);

  // a hack way to bootstrap the thread immediately
  // asm volatile ("movl %0, %%esp; pop %%ebp; pop %%ebx; pop %%edi; pop %%esi; ret" \
          : : "g" (thread->self_kstack) : "memory");

  // push to list
  ASSERT(!elem_find(&thread_ready_list, &thread->general_tag));
  list_append(&thread_ready_list, &thread->general_tag);
  ASSERT(!elem_find(&thread_all_list, &thread->all_list_tag));
  list_append(&thread_all_list, &thread->all_list_tag);

  return thread;
}

// thread_create specially for main
//    to abstract the kernel main as thread (different from the others)
//    since the main thread has been running (`esp,0xc009f000` in loader.S)
static void make_main_thread() {
  // get TCB at 0xc009e000 (no need to alloc a new TCB page)
  main_thread = running_thread();
  //
  // then init_pcb will know main is running
  init_task(main_thread, "main", 31);

  // no need to set TCB (esp at the top)

  // not in rdy list
  ASSERT(!elem_find(&thread_all_list, &main_thread->all_list_tag));
  list_append(&thread_all_list, &main_thread->all_list_tag);
}

// naive round-robin
void schedule() {

  ASSERT(intr_get_status() == INTR_OFF);

  // push current thread into rdy list
  struct task_struct* cur = running_thread();
  if (cur->status == TASK_RUNNING) { // due to ticks exhausted
    ASSERT(!elem_find(&thread_ready_list, &cur->general_tag));
    list_append(&thread_ready_list, &cur->general_tag);
    cur->ticks = cur->priority;
    cur->status = TASK_READY;
  }
  else {
    // due to waiting event (e.g. block by IO, sync...)
  }

  // if no remaining ready thread, then using idle to hlt
  if (list_empty(&thread_ready_list)) {
    thread_unblock(idle_thread);
  }

  // pop ready thread from rdy list (converted from a list elem tag)
  thread_tag = NULL; // necessary? good habit?
  thread_tag = list_pop(&thread_ready_list);

  // get TCB addr from its list tag (why bother a new way? because we can)
  struct task_struct* next = elem2entry(struct task_struct, general_tag, thread_tag);
  next->status = TASK_RUNNING;

  process_activate(next);
  switch_to(cur, next);
}

// thread block itself
// why three kinds: block/wait/hang...
void thread_block(enum task_status stat) {
  ASSERT(((stat == TASK_BLOCKED) || (stat == TASK_WAITING) || (stat == TASK_HANGING)));
  enum intr_status old_status = intr_disable();

  struct task_struct* cur_thread = running_thread();
  cur_thread->status = stat;

  schedule(); // schedule will actually have a look on status
  // so it will not append block thread into rdy list

  // after unblock
  intr_set_status(old_status);
}

// unblock given thread
// if thread has been ready, then do nothing
void thread_unblock(struct task_struct* pthread) {
  ASSERT(((pthread->status == TASK_BLOCKED) || (pthread->status == TASK_WAITING) || (pthread->status == TASK_HANGING)));
  enum intr_status old_status = intr_disable();

  if (pthread->status != TASK_READY) {
    ASSERT(!elem_find(&thread_ready_list, &pthread->general_tag));
    if (elem_find(&thread_ready_list, &pthread->general_tag)) {
      PANIC("thread_unblock: blocked thread in ready_list\n");
    }
    list_push(&thread_ready_list, &pthread->general_tag); // make the thread quickly get exec
    pthread->status = TASK_READY;
  }

  intr_set_status(old_status);
}

// yield not block (running -> ready)
void thread_yield(void) {
  struct task_struct* cur = running_thread();
  enum intr_status old_status = intr_disable();

  ASSERT(!elem_find(&thread_ready_list, &cur->general_tag));
  list_append(&thread_ready_list, &cur->general_tag);
  cur->status = TASK_READY;
  schedule();

  intr_set_status(old_status);
}

// a dummy thread never exit
static void idle(void* arg UNUSED) {
  while(1) {
    thread_block(TASK_BLOCKED);
    // ensure enable intr before hlt
    //    hlt not exploit the cpu
    asm volatile ("sti; hlt" : : : "memory");
  }
}


// init thread (thread lists and main thread)
void thread_init(void) {
  put_str("thread_init start\n");
  list_init(&thread_ready_list);
  list_init(&thread_all_list);
  lock_init(&pid_lock);
  make_main_thread();
  idle_thread =  thread_create("idle", 10, idle, NULL);
  put_str("thread_init done\n");
}

