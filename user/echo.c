#include "stdio.h"

int main(int argc, char **argv)
{
	if (!(*++argv)) {
		printf("no args!\n");
		return -1;
	}

	printf(*argv);
	while (*++argv) {
		printf(" ");
		printf(*argv);
	}
	printf("\n");

	return 0;
}
