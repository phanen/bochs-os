#include "keyboard.h"
#include "print.h"
#include "interrupt.h"
#include "io.h"
#include "global.h"

#define KBD_BUF_PORT 0x60 // 8042 spec I/O port

static void intr_keyboard_handler(void) {
   uint8_t scancode = inb(KBD_BUF_PORT); // consume it (else 8042 will stop to response to keyboard)
   put_int(scancode);
   put_char(' ');
}

void keyboard_init() {
   put_str("keyboard init start\n");
   register_handler(0x21, intr_keyboard_handler);
   put_str("keyboard init done\n");
}

