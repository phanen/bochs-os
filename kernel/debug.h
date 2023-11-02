#ifndef __KERNEL_DEBUG_H
#define __KERNEL_DEBUG_H

void panic_spin(char *filename, int line, const char *func,
		const char *condition);

#define PANIC(...) panic_spin(__FILE__, __LINE__, __func__, __VA_ARGS__)

// to enable: gcc -D
#ifdef NDEBUG
#define ASSERT(CONDITION) ((void)0)
#else
// #CONDITION -> string format
#define ASSERT(CONDITION)          \
	if (CONDITION) {           \
	} else {                   \
		PANIC(#CONDITION); \
	}
#endif // __NDEBUG

#endif // __KERNEL_DEBUG_H
