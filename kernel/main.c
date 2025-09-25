#include "print.h"
#include "init.h"
#include "debug.h"
#include "thread.h"
#include "console.h"
#include "syscall.h"
#include "stdio.h"
#include "stdio-kernel.h"
#include "memory.h"
#include "fs.h"
#include "ide.h"
#include "shell.h"

char name_buf[TASK_NAME_LEN];

void init();

void loadelf2fs(uint32_t sec_base, uint32_t file_size, char *filename);

int main()
{
	put_str("you are in kernel now\n");
	init_all();

	// FIXME: shell don't print first prompt when bootstrap
	cls_screen();
	console_put_str("[phanium@bochs /]$ ");

	loadelf2fs(300, 25000, "/hello");
	loadelf2fs(400, 25000, "/fork-exec");
	loadelf2fs(500, 25000, "/cat");
	loadelf2fs(600, 25000, "/pipe");
	loadelf2fs(700, 25000, "/echo");
	loadelf2fs(800, 50000, "/c4");

	int fd = sys_open("/file0", O_CREAT | O_RDWR);
	sys_write(fd, "hello world", 11);
	// printf("main pid is %d\n", getpid());

	thread_exit(running_thread(), true);

	// shell_run();
	while (1)
		;
	return 0;
}

void loadelf2fs(uint32_t sec_base, uint32_t file_size, char *filename)
{
	uint32_t sec_cnt = DIV_ROUND_UP(file_size, SECTOR_SIZE);
	struct disk *sda = &channels[0].devices[0];

	void *buf = sys_malloc(sec_cnt * SECTOR_SIZE);
	ide_read(sda, sec_base, buf, sec_cnt);
	int32_t fd = sys_open(filename, O_CREAT | O_RDWR);
	if (fd == -1) {
		printk("open error");
		return;
	}

	if (sys_write(fd, buf, file_size) == -1) {
		printk("file write error!\n");
		while (1)
			;
	}
	sys_close(fd);
}

void init(void)
{
	pid_t ret_pid = fork();

	char buf[17];
	if (ret_pid) {
		int status, child_pid;
		printf("init: hello\n");
		while (1) {
			child_pid = wait(&status);
			printf("init: recieve %d, (status: %d)\n", child_pid,
			       status);
		}
	} else {
		// getname(name_buf);
		// printf("shell pid is %d, name is %s", getpid(), name_buf);
		// strcpy(running_thread()->name, "shell");
		printf("shell: hello\n");
		shell_run();
	}

	printf("init fork spin\n");
	while (1)
		;
}
