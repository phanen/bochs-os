#include "timer.h"
#include "../kernel/io.h"
#include "print.h"

#define IRQ0_FREQUENCY	   100
#define INPUT_FREQUENCY	   1193180
#define COUNTER0_VALUE	   INPUT_FREQUENCY / IRQ0_FREQUENCY
#define CONTRER0_PORT	   0x40
#define COUNTER0_NO	       0
#define COUNTER_MODE	   2
#define READ_WRITE_LATCH   3
#define PIT_CONTROL_PORT   0x43

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

// init PIT8253
void timer_init() {
  put_str("timer_init start\n");
  // set frequency_set of clock
  frequency_set(CONTRER0_PORT, COUNTER0_NO, READ_WRITE_LATCH, COUNTER_MODE, COUNTER0_VALUE);
  put_str("timer_init done\n");
}
