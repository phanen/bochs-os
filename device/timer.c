#include "timer.h"
#include "io.h"
#include "print.h"
#include "interrupt.h"
#include "thread.h"
#include "debug.h"

#define IRQ0_FREQUENCY	   100
#define INPUT_FREQUENCY	   1193180
#define COUNTER0_VALUE	   INPUT_FREQUENCY / IRQ0_FREQUENCY
#define CONTRER0_PORT	   0x40
#define COUNTER0_NO	       0
#define COUNTER_MODE	   2
#define READ_WRITE_LATCH   3
#define PIT_CONTROL_PORT   0x43

uint32_t ticks; // global total ticks

// set frequency of clock (write control reg, then init counter value)
static void frequency_set(uint8_t counter_port, \
                          uint8_t counter_no, \
                          uint8_t rwl, \
                          uint8_t counter_mode, \
                          uint16_t counter_value) {
  // write control port 0x43
  outb(PIT_CONTROL_PORT, (uint8_t)(counter_no << 6 | rwl << 4 | counter_mode << 1));
  outb(counter_port, (uint8_t)counter_value); // first low 8
  outb(counter_port, (uint8_t)counter_value >> 8); // then high 8
}

// clock intr handler to be registered
static void intr_timer_handler() {
   struct task_struct* cur_thread = running_thread();

   ASSERT(cur_thread->stack_magic == 0x20021225); // check stack guard

   cur_thread->elapsed_ticks++;	// increase thread-local ticks
   ticks++;  // increase global ticks

   if (cur_thread->ticks == 0) { // ticks exhausted
      schedule();
   } else {
      cur_thread->ticks--;
   }
}

// init PIT8253
void timer_init() {
  put_str("timer_init start\n");
  // set frequency_set of clock
  frequency_set(CONTRER0_PORT, COUNTER0_NO, READ_WRITE_LATCH, COUNTER_MODE, COUNTER0_VALUE);
  register_handler(0x20, intr_timer_handler);
  put_str("timer_init done\n");
}
