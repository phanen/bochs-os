#include "syscall.h"
#include "stdio.h"
#include "string.h"
#include "fs.h"
#include "global.h"
#include "dir.h"
#include "shell.h"
#include "assert.h"

#include "path_parse.h"
#include "builtin_cmd.h"

void builtin_ls(uint32_t argc, char **argv)
{
	char *pathname = NULL;
	struct stat file_stat;
	memset(&file_stat, 0, sizeof(struct stat));
	bool long_info = false;

	uint32_t arg_path_cnt = 0;

	// parse out options and target path
	for (uint32_t i = 1; i < argc; i++) {
		// options
		if (argv[i][0] == '-') {
			if (!strcmp("-l", argv[i])) {
				long_info = true;
			} else if (!strcmp("-h", argv[i])) {
				printf("usage:\n");
				printf("  list all files in the current dirctory if no target path given\n");
				printf("  -l list all infomation about the file.\n");
				printf("  -h for help\n");
				return;
			} else {
				printf("ls: invalid option %s\n");
				printf("Try 'ls -h' for more information.\n",
				       argv[i]);
				return;
			}
		}
		// pathname
		else {
			if (arg_path_cnt == 0) {
				pathname = argv[i];
				arg_path_cnt = 1;
			} else {
				printf("ls: only support one path\n");
				return;
			}
		}
	}

	// parse pathname to fit `stat`
	if (pathname == NULL) {
		if (NULL != getcwd(final_path, MAX_PATH_LEN)) {
			pathname = final_path;
		} else {
			printf("ls: getcwd for default path failed\n");
			return;
		}
	} else {
		make_clear_abs_path(pathname, final_path);
		pathname = final_path;
	}

	if (stat(pathname, &file_stat) == -1) {
		printf("ls: cannot access %s: No such file or directory\n",
		       pathname);
		return;
	}

	if (file_stat.st_filetype == FT_REGULAR) {
		if (long_info) {
			printf("-  %d  %d  %s\n", file_stat.st_ino,
			       file_stat.st_size, pathname);
		} else {
			printf("%s\n", pathname);
		}
		return;
	}

	if (file_stat.st_filetype != FT_DIRECTORY) {
		// crazy
	}

	struct dir *dir = opendir(pathname);
	struct dir_entry *dir_e = NULL;

	// to `stat` each file in path
	//    filename need to be absolute path
	char path_buf[MAX_PATH_LEN] = { 0 };
	uint32_t len = strlen(pathname);
	memcpy(path_buf, pathname, len);

	if (path_buf[len - 1] != '/') {
		assert(len < MAX_PATH_LEN);
		path_buf[len] = '/';
		len++;
	}

	rewinddir(dir);
	if (long_info) {
		char ftype;
		printf("total: %d\n", file_stat.st_size);

		while ((dir_e = readdir(dir))) {
			path_buf[len] = 0;
			strcat(path_buf, dir_e->filename);
			// assert(len + strlen(dir_e->filename) < MAX_PATH_LEN);

			memset(&file_stat, 0, sizeof(struct stat));
			if (stat(path_buf, &file_stat) == -1) {
				printf("ls: cannot access %s: No such file or directory\n",
				       dir_e->filename);
				return;
			}

			ftype = 'd';
			if (dir_e->f_type == FT_REGULAR) {
				ftype = '-';
			}
			printf("%c  %d  %d  %s\n", ftype, dir_e->i_no,
			       file_stat.st_size, dir_e->filename);
		}
	} else {
		while ((dir_e = readdir(dir))) {
			printf("%s ", dir_e->filename);
		}
		printf("\n");
	}
	closedir(dir);
}

int32_t builtin_cd(uint32_t argc, char **argv)
{
	if (argc > 2) {
		printf("cd: too many arguments!\n");
		return -1;
	}

	// to root (home...)
	if (argc == 1) {
		final_path[0] = '/';
		final_path[1] = 0;
		return 0;
	}

	make_clear_abs_path(argv[1], final_path);
	if (chdir(final_path)) {
		printf("cd: no such directory %s\n", final_path);
		return -1;
	}

	return 0;
}

void builtin_pwd(uint32_t argc, char **argv UNUSED)
{
	if (argc != 1) {
		printf("pwd: too many arguments!\n");
		return;
	}

	if (NULL == getcwd(final_path, MAX_PATH_LEN)) {
		printf("pwd: get current work directory failed.\n");
		return;
	}

	printf("%s\n", final_path);
}

void builtin_ps(uint32_t argc, char **argv UNUSED)
{
	if (argc != 1) {
		printf("ps: too many arguments!\n");
		return;
	}
	ps();
}

void builtin_clear(uint32_t argc, char **argv UNUSED)
{
	if (argc != 1) {
		printf("clear: too many arguments!\n");
		return;
	}
	clear();
}

int32_t builtin_mkdir(uint32_t argc, char **argv)
{
	if (argc != 2) {
		printf("mkdir: too many arguments!\n");
		return -1;
	}

	make_clear_abs_path(argv[1], final_path);

	// cannot create root again
	if (!strcmp("/", final_path)) {
		return -1;
	}

	if (mkdir(final_path)) {
		printf("mkdir: create directory %s failed.\n", argv[1]);
		return -1;
	}

	return 0;
}

// non-classic
int32_t builtin_rmdir(uint32_t argc, char **argv)
{
	if (argc != 2) {
		printf("rmdir: too many arguments!\n");
		return -1;
	}

	make_clear_abs_path(argv[1], final_path);

	// cannot rm root again
	if (!strcmp("/", final_path)) {
		return -1;
	}

	if (rmdir(final_path)) {
		printf("rmdir: remove %s failed.\n", argv[1]);
		return -1;
	}

	return 0;
}

int32_t builtin_rm(uint32_t argc, char **argv)
{
	if (argc != 2) {
		printf("rm: too many arguments!\n");
		return -1;
	}

	make_clear_abs_path(argv[1], final_path);

	if (!strcmp("/", final_path)) {
		return -1;
	}

	if (unlink(final_path)) {
		printf("rm: delete %s failed.\n", argv[1]);
		return -1;
	}

	return 0;
}
