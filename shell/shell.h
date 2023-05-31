#ifndef __SHELL_H
#define __SHELL_H

#include "fs.h"

void print_prompt();
void shell_run();
extern char final_path[MAX_PATH_LEN];

#endif
