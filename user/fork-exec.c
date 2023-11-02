#include "stdio.h"
#include "syscall.h"
#include "string.h"
#include "assert.h"

// NOTE:
//    if you learn how this proc interact with kernel
//    you basically can grok `fork` and `exec`...

int main(int argc, char **argv)
{
	for (int i = 0; argv[i]; i++) {
		printf("argv[%d] is %s\n", i, argv[i]);
	}

	int pid = fork();

	if (pid) {
		int delay = 900000;
		while (delay--)
			;

		printf("\nfork-exec: father pid: %d, \n", getpid());

		wait(&delay);
		// while (1);
		exit(0);
	}

	char abs_path[512] = { 0 };

	// or child will become a orphan (spin by assert)
	assert(argv[1]);

	printf("\nfork-exec: child pid: %d, exec: , \n", getpid(), argv[1]);
	if (argv[1][0] != '/') {
		getcwd(abs_path, 512);
		strcat(abs_path, "/");
		strcat(abs_path, argv[1]);
		execv(abs_path, argv);
	} else {
		execv(argv[1], argv);
	}

	return 0;
}
