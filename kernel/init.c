#include "init.h"
#include "memory.h"
#include "print.h"
#include "interrupt.h"
#include "timer.h"
#include "thread.h"
#include "console.h"
#include "keyboard.h"

void init_all() {
   put_str("init_all\n");
   idt_init(); // set idt and PIC
   mem_init(); // page table and mem alloc
   thread_init(); // abstract main as thread
   timer_init(); // init PIT
   console_init(); // define console as a public resource
   keyboard_init(); // register keyboard event handler
}
