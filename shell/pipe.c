#include "pipe.h"
#include "memory.h"
#include "fs.h"
#include "file.h"
#include "ioqueue.h"
#include "thread.h"
#include "params.h"
#include "debug.h"

// if a given fd is a pipe
bool is_pipe(uint32_t local_fd)
{
	uint32_t global_fd = fd_local2global(local_fd);
	return file_table[global_fd].fd_flag == PIPE_FLAG;
}

// mutiplex
int32_t sys_pipe(int32_t pipefd[2])
{
	int32_t global_fd = get_free_slot_in_global();

	// kernel buf as a fake inode
	file_table[global_fd].fd_inode = get_kernel_pages(1);

	ioqueue_init((struct ioqueue *)file_table[global_fd].fd_inode);
	if (file_table[global_fd].fd_inode == NULL) {
		return -1;
	}

	// fd_flag -> pipe flag
	file_table[global_fd].fd_flag = PIPE_FLAG;

	// fd_pos -> pipe open cnt
	file_table[global_fd].fd_pos = 2;
	pipefd[0] = pcb_fd_install(global_fd);
	pipefd[1] = pcb_fd_install(global_fd);
	return 0;
}

uint32_t pipe_read(int32_t fd, void *buf, uint32_t count)
{
	char *buffer = buf;
	uint32_t bytes_read = 0;
	uint32_t global_fd = fd_local2global(fd);

	struct ioqueue *ioq = (struct ioqueue *)file_table[global_fd].fd_inode;

	uint32_t ioq_len = ioq_length(ioq);
	uint32_t size = MIN(ioq_len, count);

	// avoid block by ioq_getchar
	while (bytes_read < size) {
		*buffer = ioq_getchar(ioq);
		bytes_read++;
		buffer++;
	}
	return bytes_read;
}

uint32_t pipe_write(int32_t fd, const void *buf, uint32_t count)
{
	const char *buffer = buf;
	uint32_t bytes_write = 0;
	uint32_t global_fd = fd_local2global(fd);

	struct ioqueue *ioq = (struct ioqueue *)file_table[global_fd].fd_inode;

	uint32_t ioq_left = bufsize - ioq_length(ioq);
	uint32_t size = MIN(count, ioq_left);

	while (bytes_write < size) {
		ioq_putchar(ioq, *buffer);
		bytes_write++;
		buffer++;
	}
	return bytes_write;
}

// redirect old_local_fd  to new_local_fd
//    actually similar to dup
void sys_dup2(uint32_t new_local_fd, uint32_t old_local_fd)
{
	struct task_struct *cur = running_thread();

	ASSERT(old_local_fd >= 0);
	ASSERT(new_local_fd >= 0);

	// TODO: to design a better `close`
	if (new_local_fd < 3) {
		// change local ref only,
		//    then we can use dup(1, 1) to resume it
		cur->fd_table[old_local_fd] = new_local_fd;
	} else {
		uint32_t new_global_fd = cur->fd_table[new_local_fd];
		cur->fd_table[old_local_fd] = new_global_fd;
	}
}
