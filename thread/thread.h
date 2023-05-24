#ifndef __THREAD_THREAD_H
#define __THREAD_THREAD_H

#include "stdint.h"
#include "list.h"
#include "memory.h"

// template function type for thread
typedef void thread_func(void*);

typedef uint16_t pid_t;

// status for task
enum task_status {
    TASK_RUNNING,
    TASK_READY,
    TASK_BLOCKED,
    TASK_WAITING,
    TASK_HANGING,
    TASK_DIED
};

// intr will push
struct intr_stack {

    // according to intr%1entry
    uint32_t vec_no;
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp_dummy;	// not used
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    uint32_t gs;
    uint32_t fs;
    uint32_t es;
    uint32_t ds;

    // according to spec
    uint32_t err_code; // must exist (by dummy)
    void (*eip) (void);
    uint32_t cs;
    uint32_t eflags;

    // stack switch (ring3 -> ring0)
    void* esp;
    uint32_t ss;
};

struct thread_stack {

    // according to ABI (protect for caller)
    //	    these reg should always be saved?
    //	    seems meaningless...
    uint32_t ebp;
    uint32_t ebx;
    uint32_t edi;
    uint32_t esi;

    // when return from switch_to
    //	    bootstrap: eip_return -> kernel_thread
    //	    otherwise: eip_return -> go on after switch_to

    // eip_return
    void (*eip) (thread_func* func, void* func_arg);

    // the following args are used only for bootstrap
    void (*unused_retaddr); // fake another eip (eip_call)
    thread_func* function;
    void* func_arg;
};

// pcb or tcb
struct task_struct {
    uint32_t* self_kstack; // each kernel thread has its own kernel stack

    pid_t pid;

    enum task_status status;
    char name[16];
    uint8_t priority;

    uint8_t ticks; // time slice (-1 each clock intr)
    uint32_t elapsed_ticks; // total tick counter (since execution)

    // used with the help of macro `offset`, macro `elem2entry`
    struct list_elem general_tag; // `tag` to denote the thread in ready_list?
    struct list_elem all_list_tag; // `tag` to denote the thread in all_list?

    uint32_t* pgdir; // used by proc, NULL for thread
    struct virtual_addr userprog_vaddr; // pool for ring3
    struct mem_block_desc u_block_desc[DESC_CNT]; // mem block descs local to user

    uint32_t stack_magic; // guard to check stack overflow (some intrs may do tons of push)
};

extern struct list thread_ready_list;
extern struct list thread_all_list;

void thread_stack_init(struct task_struct* pthread, thread_func function, void* func_arg);

void init_task(struct task_struct* pthread, char* name, int prio);

struct task_struct* thread_create(char* name, int prio, thread_func function, void* func_arg);

struct task_struct* running_thread();

void schedule();

void thread_init();

void thread_block(enum task_status stat);
void thread_unblock(struct task_struct* pthread);

#endif
