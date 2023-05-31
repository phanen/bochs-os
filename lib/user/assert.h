#ifndef __LIB_USER_ASSERT_H
#define __LIB_USER_ASSERT_H

void user_panic_spin(char* filename, int line, const char* func, const char* condition);

#define panic(...) user_panic_spin(__FILE__, __LINE__, __func__, __VA_ARGS__)

// to enable: gcc -D
#ifdef NDEBUG
    #define assert(CONDITION) ((void)0)
#else
    // #CONDITION -> string format
    #define assert(CONDITION) if (CONDITION) {} else { panic(#CONDITION); }
#endif // __NDEBUG

#endif
