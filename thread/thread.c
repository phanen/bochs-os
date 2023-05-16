#include "thread.h"
#include "stdint.h"
#include "string.h"
#include "global.h"
#include "memory.h"
#include "interrupt.h"
#include "debug.h"
#include "print.h"

#define NULL ((void*)0)

#define PG_SIZE 4096

struct task_struct* main_thread; // PCB for main thread (take `main()` as a thread)
struct list thread_ready_list; // TASK_READY
struct list thread_all_list; // all kind of task

static struct list_elem* thread_tag; // just a buffer
//  but... why not use locale var?

extern void switch_to(struct task_struct* cur, struct task_struct* next);

// get the PCB of running thread
struct task_struct* running_thread() {
    uint32_t esp;
    asm ("mov %%esp, %0": "=g" (esp)); // get stack top
    // get the page start of stack top (i.e. the addr of PCB)
    return (struct task_struct*)(esp & 0xfffff000);
}

// simply call
static void kernel_thread(thread_func* function, void* func_arg) {
    intr_enable();
    function(func_arg);
}

// init thread_stack
void thread_create(struct task_struct* pthread, thread_func function, void* func_arg) {
    // preserve intr stack
    pthread->self_kstack -= sizeof(struct intr_stack);
    // preserve thread stack
    pthread->self_kstack -= sizeof(struct thread_stack);

    struct thread_stack* kthread_stack = (struct thread_stack*)pthread->self_kstack;
    kthread_stack->eip = kernel_thread;
    kthread_stack->function = function;
    kthread_stack->func_arg = func_arg;
    kthread_stack->ebp = kthread_stack->ebx = kthread_stack->esi = kthread_stack->edi = 0;
}

// init basic info
void init_thread(struct task_struct* pthread, char* name, int prio) {
    memset(pthread, 0, sizeof(*pthread));
    strcpy(pthread->name, name);

    if (pthread == main_thread) {
	pthread -> status = TASK_RUNNING;
    } else {
	pthread->status = TASK_READY;
    }

    pthread->priority = prio;
    pthread->self_kstack = (uint32_t*)((uint32_t)pthread + PG_SIZE); // top of PCB page

    pthread->ticks = prio;
    pthread->elapsed_ticks = 0;

    pthread->pgdir = NULL;

    pthread->stack_magic = 0x20021225; // magic number
}

// create a new thread (TCB) with provided description
struct task_struct* thread_start(char* name, int prio, thread_func function, void* func_arg) {
    struct task_struct* thread = get_kernel_pages(1); // PCBs (include user PCB) are all in kernel space (because we implement kernel-level thread)
    init_thread(thread, name, prio);
    thread_create(thread, function, func_arg); // bind function to thread
    // asm volatile ("movl %0, %%esp; pop %%ebp; pop %%ebx; pop %%edi; pop %%esi; ret" \
		  : : "g" (thread->self_kstack) : "memory");

    // ensure the thread to be created not in list now
    ASSERT(!elem_find(&thread_ready_list, &thread->general_tag));
    list_append(&thread_ready_list, &thread->general_tag);

    ASSERT(!elem_find(&thread_all_list, &thread->all_list_tag));
    list_append(&thread_all_list, &thread->all_list_tag);

    return thread;
}

// to better taking kernel main as a abstract of thread
static void make_main_thread() {
    // main thread has been running (since `esp,0xc009f000` in loader.S)
    // we preserve the TCB at 0xc009e000, no need to alloc page
    main_thread = running_thread();
    init_thread(main_thread, "main", 31);

    // only in all_list
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
    } else {
	// due to waiting event (e.g. block by IO, sync...)
    }

    // pop ready thread from rdy list (converted from a list elem tag)
    ASSERT(!list_empty(&thread_ready_list)); // no idle now...
    thread_tag = NULL; // necessary? good habit?
    thread_tag = list_pop(&thread_ready_list);
    // to get the addr of PCB from a PCB field
    //	though we can still do the same as in `running_thread`
    //	why bother a new way? because we can
    struct task_struct* next = elem2entry(struct task_struct, general_tag, thread_tag);
    next->status = TASK_RUNNING;

    switch_to(cur, next); // written in asm
}

// init thread (thread lists and main thread)
void thread_init(void) {
    put_str("thread_init start\n");
    list_init(&thread_ready_list);
    list_init(&thread_all_list);
    make_main_thread();
    put_str("thread_init done\n");
}

