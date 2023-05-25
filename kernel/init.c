#include "init.h"
#include "memory.h"
#include "print.h"
#include "interrupt.h"
#include "timer.h"
#include "thread.h"
#include "console.h"
#include "keyboard.h"
#include "tss.h"
#include "syscall-init.h"
#include "ide.h"

void init_all() {
   put_str("init_all\n");
   idt_init(); // set idt and PIC
   mem_init(); // page table and mem alloc
   thread_init(); // abstract main as thread
   timer_init(); // init PIT
   console_init(); // define console as a public resource
   keyboard_init(); // register keyboard event handler
   tss_init(); // update tss desc and ring3 desc
   syscall_init(); // make syscall_table
   ide_init();  // init the channels, disks, partitions
}
