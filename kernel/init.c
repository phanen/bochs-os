#include "init.h"
#include "print.h"
#include "interrupt.h"
#include "../device/timer.h"

void init_all() {
   put_str("init_all\n");
   idt_init(); // init all interrupt (set idt and PIC)
   timer_init(); // init PIT
}
