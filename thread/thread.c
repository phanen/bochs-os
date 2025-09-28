#include "thread.h"
#include "stdint.h"
#include "string.h"
#include "global.h"
#include "memory.h"
#include "interrupt.h"
#include "debug.h"
#include "print.h"
#include "process.h"
#include "file.h"
#include "stdio.h"
#include "pid.h"

extern void init(void);

static struct task_struct
	*main_thread; // TCB for main thread (take `main()` as a thread)
static struct task_struct *idle_thread; // a dummy never exit

struct list thread_ready_list; // TASK_READY task
struct list thread_all_list; // all tasks

static struct list_elem *thread_tag; // just a buffer

extern void switch_to(struct task_struct *cur, struct task_struct *next);

// a patch, backward compatible
pid_t fork_pid()
{
	return allocate_pid();
}

// get the TCB of running thread
struct task_struct *running_thread()
{
	uint32_t esp;
	asm("mov %%esp, %0" : "=g"(esp)); // get esp
	// get the page start of stack top (i.e. the addr of TCB)
	return (struct task_struct *)(esp & 0xfffff000);
}

// a entry which provides a eip addr
//    bootstrap
static void kernel_thread(thread_func *function, void *func_arg)
{
	intr_enable(); // !!!
	function(func_arg);
}

// init thread stack
void thread_stack_init(struct task_struct *pthread, thread_func function,
		       void *func_arg)
{
	// preserve the intr stack, kernel thread stack
	pthread->self_kstack -=
		sizeof(struct intr_stack) + sizeof(struct thread_stack);
	struct thread_stack *kthread_stack =
		(struct thread_stack *)pthread->self_kstack;
	kthread_stack->eip =
		kernel_thread; // make the thread bootstrap to `kernel_thread`
	kthread_stack->function = function;
	kthread_stack->func_arg = func_arg;
	kthread_stack->ebp = 0;
	kthread_stack->ebx = 0;
	kthread_stack->esi = 0;
	kthread_stack->edi = 0;
}

// init basic info of pcb
void init_task(struct task_struct *pthread, char *name, int prio)
{
	// the pcb/tcb should be valid
	ASSERT(pthread != NULL);

	memset(pthread, 0, sizeof(*pthread));
	strcpy(pthread->name, name);

	pthread->pid = allocate_pid();

	if (pthread == main_thread) {
		pthread->status = TASK_RUNNING;
	} else {
		pthread->status = TASK_READY;
	}

	pthread->priority = prio;

	// top of TCB page
	pthread->self_kstack = (uint32_t *)((uint32_t)pthread + PG_SIZE);

	pthread->ticks = prio;
	pthread->elapsed_ticks = 0;

	pthread->pgdir = NULL;

	// open fd for std IO
	pthread->fd_table[0] = 0;
	pthread->fd_table[1] = 1;
	pthread->fd_table[2] = 2;
	// close other fds
	for (uint8_t fd_i = 3; fd_i < MAX_FILES_OPEN_PER_PROC; fd_i++) {
		pthread->fd_table[fd_i] = -1;
	}

	pthread->cwd_inode_nr = 0; // default, rootdir
	pthread->parent_pid = -1; // no father (init from scrach)

	pthread->stack_magic = 0x20021225; // magic number
}

// create a thread (alloc a TCB page ->  init TCB -> init stack)
struct task_struct *thread_create(char *name, int prio, thread_func function,
				  void *func_arg)
{
	// all TCBs (include user TCBs) are in kernel space
	// (because we implement kernel-level thread?)
	struct task_struct *thread = get_kernel_pages(1);

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
static void make_main_thread()
{
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
void schedule()
{
	ASSERT(intr_get_status() == INTR_OFF);

	// push current thread into rdy list
	struct task_struct *cur = running_thread();
	if (cur->status == TASK_RUNNING) { // due to ticks exhausted
		ASSERT(!elem_find(&thread_ready_list, &cur->general_tag));
		list_append(&thread_ready_list, &cur->general_tag);
		cur->ticks = cur->priority;
		cur->status = TASK_READY;
	} else {
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
	struct task_struct *next =
		elem2entry(struct task_struct, general_tag, thread_tag);
	next->status = TASK_RUNNING;

	process_activate(next);
	switch_to(cur, next);
}

// thread block itself
// why three kinds: block/wait/hang...
void thread_block(enum task_status stat)
{
	ASSERT(((stat == TASK_BLOCKED) || (stat == TASK_WAITING) ||
		(stat == TASK_HANGING)));
	enum intr_status old_status = intr_disable();

	struct task_struct *cur_thread = running_thread();
	cur_thread->status = stat;

	schedule(); // schedule will actually have a look on status
	// so it will not append block thread into rdy list

	// after unblock
	intr_set_status(old_status);
}

// unblock given thread
// if thread has been ready, then do nothing
void thread_unblock(struct task_struct *pthread)
{
	ASSERT(((pthread->status == TASK_BLOCKED) ||
		(pthread->status == TASK_WAITING) ||
		(pthread->status == TASK_HANGING)));
	enum intr_status old_status = intr_disable();

	if (pthread->status != TASK_READY) {
		ASSERT(!elem_find(&thread_ready_list, &pthread->general_tag));
		if (elem_find(&thread_ready_list, &pthread->general_tag)) {
			PANIC("thread_unblock: blocked thread in ready_list\n");
		}
		list_push(
			&thread_ready_list,
			&pthread->general_tag); // make the thread quickly get exec
		pthread->status = TASK_READY;
	}

	intr_set_status(old_status);
}

// yield not block (running -> ready)
void thread_yield(void)
{
	struct task_struct *cur = running_thread();
	enum intr_status old_status = intr_disable();

	ASSERT(!elem_find(&thread_ready_list, &cur->general_tag));
	list_append(&thread_ready_list, &cur->general_tag);
	cur->status = TASK_READY;
	schedule();

	intr_set_status(old_status);
}

// a dummy thread never exit
static void idle(void *arg UNUSED)
{
	while (1) {
		// at the first time no other thread,
		//  the scheduler wakes idle up
		thread_block(TASK_BLOCKED);

		// ensure enable intr before hlt
		//    hlt not exploit the cpu (hang util intr)
		asm volatile("sti; hlt" : : : "memory");

		// at the second time no other rdy thread
		//  idle either block or rdy
		//    if block: the scheduler wake it up again
		//    if rdy: block then wake (emm...)
		//  anyway, no panic will happend
	}
}

// write (buf_len-1) char, embed a attr in it
static void pad_print(char *buf, int32_t buf_len, void *ptr, char format)
{
	memset(buf, 0, buf_len);

	uint8_t out_pad_0idx = 0;
	switch (format) {
	case 's':
		out_pad_0idx = sprintf(buf, "%s", ptr);
		break;
	case 'd':
		out_pad_0idx = sprintf(buf, "%d", *((int16_t *)ptr));
	case 'x':
		out_pad_0idx = sprintf(buf, "%x", *((uint32_t *)ptr));
	}

	// attr overflow
	ASSERT(out_pad_0idx < buf_len);

	// pad space
	while (out_pad_0idx < buf_len) {
		buf[out_pad_0idx] = ' ';
		out_pad_0idx++;
	}

	sys_write(stdout_no, buf, buf_len - 1);
}

// print one thread info (in a line)
static bool elem2thread_info(struct list_elem *pelem, int arg UNUSED)
{
	struct task_struct *pthread =
		elem2entry(struct task_struct, all_list_tag, pelem);

	// strlen(max valid info string) < 15 = 16 - 1
	char out_pad[16] = { 0 };

	// write 15 char
	//      (embed a attr, left align, pad with space)
	pad_print(out_pad, 16, &pthread->pid, 'd');

	if (pthread->parent_pid == -1) {
		pad_print(out_pad, 16, "NULL", 's');
	} else {
		pad_print(out_pad, 16, &pthread->parent_pid, 'd');
	}

	switch (pthread->status) {
	case 0:
		pad_print(out_pad, 16, "RUNNING", 's');
		break;
	case 1:
		pad_print(out_pad, 16, "READY", 's');
		break;
	case 2:
		pad_print(out_pad, 16, "BLOCKED", 's');
		break;
	case 3:
		pad_print(out_pad, 16, "WAITING", 's');
		break;
	case 4:
		pad_print(out_pad, 16, "HANGING", 's');
		break;
	case 5:
		pad_print(out_pad, 16, "DIED", 's');
	}

	pad_print(out_pad, 16, &pthread->elapsed_ticks, 'x');

	memset(out_pad, 0, 16);

	ASSERT(strlen(pthread->name) < 16); // append CR
	memcpy(out_pad, pthread->name, strlen(pthread->name));
	strcat(out_pad, "\n");

	sys_write(stdout_no, out_pad, strlen(out_pad));

	return false; // list_traversal
}

// report a snapshot of the current processes
void sys_ps()
{
	// 15 column/attr, 15 * 5 < 80
	char *ps_title =
		"PID            PPID           STAT           TICKS          COMMAND\n";
	sys_write(stdout_no, ps_title, strlen(ps_title));

	// print each proc line
	list_traversal(&thread_all_list, elem2thread_info, 0);
}

// free the pcb, pgdir, rm it from list
void thread_exit(struct task_struct *pthread, bool need_schedule)
{
	intr_disable();

	pthread->status = TASK_DIED;

	// pgdir
	if (pthread->pgdir) {
		mfree_page(PF_KERNEL, pthread->pgdir, 1);
	}

	if (elem_find(&thread_ready_list, &pthread->general_tag)) {
		list_remove(&pthread->general_tag);
	}
	list_remove(&pthread->all_list_tag);

	// pcb
	if (pthread != main_thread) {
		mfree_page(PF_KERNEL, pthread, 1);
	}

	release_pid(pthread->pid);

	if (need_schedule) {
		schedule();
		PANIC("thread_exit: should not be here\n");
	}
}

static bool pid_check(struct list_elem *pelem, int32_t pid)
{
	struct task_struct *pthread =
		elem2entry(struct task_struct, all_list_tag, pelem);
	if (pthread->pid == pid) {
		return true;
	}
	return false;
}

struct task_struct *pid2thread(int32_t pid)
{
	struct list_elem *pelem =
		list_traversal(&thread_all_list, pid_check, pid);
	if (pelem == NULL) {
		return NULL;
	}
	struct task_struct *pthread =
		elem2entry(struct task_struct, all_list_tag, pelem);
	return pthread;
}

// init thread (thread lists and main thread)
void thread_init()
{
	put_str("thread_init start\n");
	list_init(&thread_ready_list);
	list_init(&thread_all_list);

	pid_pool_init();

	// init process has pid 1
	//    early than kernel main
	process_execute(init, "init");

	make_main_thread();
	idle_thread = thread_create("idle", 10, idle, NULL);
	put_str("thread_init done\n");
}
