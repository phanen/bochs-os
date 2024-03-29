#include "fork.h"
#include "pipe.h"
#include "process.h"
#include "memory.h"
#include "interrupt.h"
#include "debug.h"
#include "thread.h"
#include "string.h"
#include "file.h"
#include "stdio.h"

extern void intr_exit(void);

// copy pcb, vaddr bitmap, stack0 to child
static int32_t copy_pcb_vaddrbitmap_stack0(struct task_struct *child_thread,
					   struct task_struct *parent_thread)
{
	// copy the whole pcb then change
	//    share the same `pcb->self_kstack`
	memcpy(child_thread, parent_thread, PG_SIZE);
	child_thread->pid = fork_pid();
	child_thread->elapsed_ticks = 0;
	child_thread->status = TASK_READY;
	child_thread->ticks = child_thread->priority;
	child_thread->parent_pid = parent_thread->pid;
	child_thread->general_tag.prev = child_thread->general_tag.next = NULL;
	child_thread->all_list_tag.prev = child_thread->all_list_tag.next =
		NULL;
	block_desc_init(child_thread->u_block_desc);

	// copy the vaddr bitmap
	//    alloc new bitmap pages
	//    then copy the content of bitmap
	uint32_t bitmap_pg_cnt = DIV_ROUND_UP(
		(0xc0000000 - USER_VADDR_START) / PG_SIZE / 8, PG_SIZE);
	void *vaddr_btmp = get_kernel_pages(bitmap_pg_cnt);
	if (vaddr_btmp == NULL)
		return -1;
	memcpy(vaddr_btmp, child_thread->userprog_vaddr.vaddr_bitmap.bits,
	       bitmap_pg_cnt * PG_SIZE);
	child_thread->userprog_vaddr.vaddr_bitmap.bits = vaddr_btmp;

	// // debug
	// printf(child_thread->name);
	// ASSERT(strlen(child_thread->name) < 11);
	// strcpy(child_thread->name, "_fork");
	return 0;
}

// copy the body(content in vaddr) and stack3(sure, also just data in vaddr)
//    via a kernel buf: parent->kernel_buf, switch to child, kernel_buf->child
//    NOTE: btw, syscall won't switch addr space(page table)
static void copy_body_stack3(struct task_struct *child_thread,
			     struct task_struct *parent_thread, void *buf_page)
{
	uint8_t *vaddr_btmp = parent_thread->userprog_vaddr.vaddr_bitmap.bits;
	uint32_t btmp_bytes_len =
		parent_thread->userprog_vaddr.vaddr_bitmap.btmp_bytes_len;
	uint32_t vaddr_start = parent_thread->userprog_vaddr.vaddr_start;

	uint32_t prog_vaddr = 0;

	// find and copy all alloc pages (by iterate the bitmap)
	for (uint32_t idx_byte = 0; idx_byte < btmp_bytes_len; idx_byte++) {
		if (vaddr_btmp[idx_byte]) {
			for (uint32_t idx_bit = 0; idx_bit < 8; idx_bit++) {
				// foreach alloc page
				if ((BITMAP_MASK << idx_bit) &
				    vaddr_btmp[idx_byte]) {
					prog_vaddr =
						((idx_byte << 3) + idx_bit) *
							PG_SIZE +
						vaddr_start;

					// copy a page from parent `prog_vaddr` to kernel buf
					memcpy(buf_page, (void *)prog_vaddr,
					       PG_SIZE);
					// switch to child space (also avoid install new pte/pde in parent)
					page_dir_activate(child_thread);
					// alloc a page from `prog_vaddr` in child space
					get_a_page_without_opvaddrbitmap(
						PF_USER, prog_vaddr);
					// copy back
					memcpy((void *)prog_vaddr, buf_page,
					       PG_SIZE);
					// back to parent space
					page_dir_activate(parent_thread);
				}
			}
		}
	}
}

// build child thread_stack (also change retval)
static int32_t build_child_stack(struct task_struct *child_thread)
{
	// retval(eax) in stack0
	struct intr_stack *intr_0_stack =
		(struct intr_stack *)((uint32_t)child_thread + PG_SIZE -
				      sizeof(struct intr_stack));
	intr_0_stack->eax = 0;

	// build `thread_stack` for `switch_to`
	uint32_t *ret_addr_in_thread_stack = (uint32_t *)intr_0_stack - 1;

	// unnecessary
	uint32_t *esi_ptr_in_thread_stack = (uint32_t *)intr_0_stack - 2;
	uint32_t *edi_ptr_in_thread_stack = (uint32_t *)intr_0_stack - 3;
	uint32_t *ebx_ptr_in_thread_stack = (uint32_t *)intr_0_stack - 4;

	// ebp在thread_stack中的地址便是当时的esp(0级栈的栈
	uint32_t *ebp_ptr_in_thread_stack = (uint32_t *)intr_0_stack - 5;

	// `switch_to` return to `intr_exit`
	*ret_addr_in_thread_stack = (uint32_t)intr_exit;

	// unnecessary (override by `intr_exit` pop)
	*ebp_ptr_in_thread_stack = *ebx_ptr_in_thread_stack =
		*edi_ptr_in_thread_stack = *esi_ptr_in_thread_stack = 0;

	child_thread->self_kstack = ebp_ptr_in_thread_stack;
	return 0;
}

// child open same inode as its parent
//    they share the same `pcb->fd_table`
static void update_inode_open_cnts(struct task_struct *thread)
{
	for (int32_t local_fd = 3; local_fd < MAX_FILES_OPEN_PER_PROC;
	     local_fd++) {
		int32_t global_fd = thread->fd_table[local_fd];
		ASSERT(global_fd < MAX_FILE_OPEN);

		if (global_fd == -1) {
			continue;
		}

		if (is_pipe(local_fd)) {
			file_table[global_fd].fd_pos++;
			continue;
		}

		file_table[global_fd].fd_inode->i_open_cnts++;
	}
}

// copy out child image
static int32_t copy_process(struct task_struct *child_thread,
			    struct task_struct *parent_thread)
{
	void *buf_page = get_kernel_pages(1);
	if (buf_page == NULL) {
		return -1;
	}

	// copy pcb, vaddr bitmap, stack0
	if (copy_pcb_vaddrbitmap_stack0(child_thread, parent_thread) == -1) {
		return -1;
	}

	// create page table for child  in kernel space
	//    (than change last pde only)
	child_thread->pgdir = create_page_dir();
	if (child_thread->pgdir == NULL) {
		return -1;
	}

	// copy proc body and stack3 to child via a kernel buf
	copy_body_stack3(child_thread, parent_thread, buf_page);

	// build thread_stack for `switch_to`
	//    and set retval to 0 for child
	build_child_stack(child_thread);

	// child and parent have the same copy of `pcb->fd_table` now
	// FIXME: `close` child file, also `close` parent file??
	update_inode_open_cnts(child_thread);

	mfree_page(PF_KERNEL, buf_page, 1);
	return 0;
}

pid_t sys_fork(void)
{
	struct task_struct *parent_thread = running_thread();
	struct task_struct *child_thread = get_kernel_pages(1);
	if (child_thread == NULL) {
		return -1;
	}
	// btw, thread cannot fork (no own addr space/page table)
	ASSERT(INTR_OFF == intr_get_status() && parent_thread->pgdir != NULL);

	if (copy_process(child_thread, parent_thread) == -1) {
		return -1;
	}

	// fake that child is ready to schedule
	ASSERT(!elem_find(&thread_ready_list, &child_thread->general_tag));
	list_append(&thread_ready_list, &child_thread->general_tag);
	ASSERT(!elem_find(&thread_all_list, &child_thread->all_list_tag));
	list_append(&thread_all_list, &child_thread->all_list_tag);

	return child_thread->pid; // for parent (chlid return from `switch_to`)
}
