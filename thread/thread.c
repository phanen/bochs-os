#include "thread.h"
#include "stdint.h"
#include "string.h"
#include "global.h"
#include "memory.h"

#define PG_SIZE 4096

// simply call
static void kernel_thread(thread_func* function, void* func_arg) {
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
    pthread->status = TASK_RUNNING;
    pthread->priority = prio;
    pthread->self_kstack = (uint32_t*)((uint32_t)pthread + PG_SIZE); // top of PCB page
    pthread->stack_magic = 0x20021225; // magic number
}

// create a new thread (PCB) with provided description
struct task_struct* thread_start(char* name, int prio, thread_func function, void* func_arg) {
    struct task_struct* thread = get_kernel_pages(1); // PCBs (include user PCB) are all in kernel space (because we implement kernel-level thread)
    init_thread(thread, name, prio);
    thread_create(thread, function, func_arg); // bind function to thread
    asm volatile ("movl %0, %%esp; pop %%ebp; pop %%ebx; pop %%edi; pop %%esi; ret" \
		  : : "g" (thread->self_kstack) : "memory");
    return thread;
}
