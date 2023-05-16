#ifndef __THREAD_THREAD_H
#define __THREAD_THREAD_H

#include "stdint.h"
#include "list.h"

// template function type for thread
typedef void thread_func(void*);

// status for task
enum task_status {
    TASK_RUNNING,
    TASK_READY,
    TASK_BLOCKED,
    TASK_WAITING,
    TASK_HANGING,
    TASK_DIED
};

// when intr(hard or soft), push regfile into this context
// floating location in kernel stack
struct intr_stack {
    uint32_t vec_no; // kernel.S VECTOR push %1
    uint32_t edi;
    uint32_t esi;
    uint32_t ebp;
    uint32_t esp_dummy;	// pushad esp, but popad not pop esp...
    uint32_t ebx;
    uint32_t edx;
    uint32_t ecx;
    uint32_t eax;
    uint32_t gs;
    uint32_t fs;
    uint32_t es;
    uint32_t ds;

    // low privilege to high privilege
    uint32_t err_code;	// pushed after eip
    void (*eip) (void);
    uint32_t cs;
    uint32_t eflags;
    void* esp;
    uint32_t ss;
};

// floating location in kernel stack
// location dependes (on runtime...)
struct thread_stack {
    uint32_t ebp;
    uint32_t ebx;
    uint32_t edi;
    uint32_t esi;

    // when thread bootstrap: eip -> kernel_thread
    // otherwise: eip -> return addr of switch_to
    void (*eip) (thread_func* func, void* func_arg);

    // bootstrap
    void (*unused_retaddr); // placeholder
    thread_func* function; // call by kernel_thread
    void* func_arg;
};

// PCB (or TCB?)
struct task_struct {

    uint32_t* self_kstack; // each kernel thread has its own kernel stack
    enum task_status status;
    char name[16];
    uint8_t priority;

    uint8_t ticks; // time slice (-1 each clock intr)
    uint32_t elapsed_ticks; // total tick counter (since execution)

    // used with the help of macro `offset`, macro `elem2entry`
    struct list_elem general_tag; // `tag` to denote the thread in ready_list?
    struct list_elem all_list_tag; // `tag` to denote the thread in all_list?

    uint32_t* pgdir; // used by proc, NULL for thread

    uint32_t stack_magic; // guard to check stack overflow (some intrs may do tons of push)
};

void thread_create(struct task_struct* pthread, thread_func function, void* func_arg);
void init_thread(struct task_struct* pthread, char* name, int prio);
struct task_struct* thread_start(char* name, int prio, thread_func function, void* func_arg);

struct task_struct* running_thread();
void schedule();
void thread_init();

void thread_block(enum task_status stat);
void thread_unblock(struct task_struct* pthread);

#endif
