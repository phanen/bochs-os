#include "console.h"
#include "print.h"
#include "stdint.h"
#include "sync.h"
#include "thread.h"

// console is a abstract public resource
// (actually print.c have enable us to interact with a single console
//    so just protect it using lock from thread race)
static struct lock console_lock;

void console_init() {
  lock_init(&console_lock);
}

void console_acquire() {
   lock_acquire(&console_lock);
}

void console_release() {
   lock_release(&console_lock);
}

// print string to console
void console_put_str(char* str) {
   console_acquire();
   put_str(str);
   console_release();
}

// print char to console
void console_put_char(uint8_t char_asci) {
   console_acquire();
   put_char(char_asci);
   console_release();
}

// print num to console (hex)
void console_put_int(uint32_t num) {
   console_acquire();
   put_int(num);
   console_release();
}
