#include "init.h"
#include "memory.h"
#include "print.h"
#include "interrupt.h"
#include "timer.h"
#include "thread.h"

void init_all() {
   put_str("init_all\n");
   idt_init(); // init all interrupt (set idt and PIC)
   mem_init();
   thread_init();
   timer_init(); // init PIT
}
