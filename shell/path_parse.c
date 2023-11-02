#include "builtin_cmd.h"
#include "syscall.h"
#include "stdio.h"
#include "string.h"
#include "fs.h"
#include "global.h"
#include "dir.h"
#include "shell.h"
#include "assert.h"

// convert . or .. in abs_path
//    NOTE: /a/b/c/ -> /a/b/c
static void wash_path(char *old_abs_path, char *new_abs_path)
{
	assert(old_abs_path[0] == '/');

	char name[MAX_FILE_NAME_LEN] = { 0 };
	char *sub_path = old_abs_path;
	sub_path = path_parse(sub_path, name);

	// rootdir
	if (name[0] == 0) {
		new_abs_path[0] = '/';
		new_abs_path[1] = 0;
		return;
	}

	new_abs_path[0] = 0;
	strcat(new_abs_path, "/");
	while (name[0]) {
		if (!strcmp("..", name)) {
			// trunc name after current last '/'
			char *slash_ptr = strrchr(new_abs_path, '/');
			//  "/a/b" -> "/a"
			if (slash_ptr != new_abs_path) {
				*slash_ptr = 0;
			}
			//  "/xx" -> "/"
			else {
				*(slash_ptr + 1) = 0;
			}
		} else if (strcmp(".", name)) {
			if (strcmp(new_abs_path, "/")) {
				// append '/' only if not rootdir
				strcat(new_abs_path, "/");
			}
			strcat(new_abs_path, name);
		} else {
			// .
			// just goon
		}

		memset(name, 0, MAX_FILE_NAME_LEN);
		if (sub_path) {
			sub_path = path_parse(sub_path, name);
		}
	}
}

// convert path into `clear` abs_path
//      clear: no . or ..
void make_clear_abs_path(char *path, char *final_path)
{
	char abs_path[MAX_PATH_LEN] = { 0 };

	// get cur abs_path
	if (path[0] != '/') {
		memset(abs_path, 0, MAX_PATH_LEN);
		if (getcwd(abs_path, MAX_PATH_LEN) == NULL) {
			printf("fail to getcwd\n");
			return;
		}

		// not rootdir
		if (!((abs_path[0] == '/') && (abs_path[1] == 0))) {
			strcat(abs_path, "/");
		}
		// strcat(abs_path, path);
	}
	// get full abs_path
	strcat(abs_path, path);

	// memcpy(final_path, abs_path, strlen(abs_path));

	// convert . or .. in path
	wash_path(abs_path, final_path);
}
