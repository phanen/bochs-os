#include "keyboard.h"
#include "print.h"
#include "interrupt.h"
#include "io.h"
#include "global.h"

#define KBD_BUF_PORT 0x60 // 8042 spec I/O port

static void intr_keyboard_handler(void) {
   put_char('k');
   inb(KBD_BUF_PORT); // consume it (else 8042 will stop to response to keyboard)
}

void keyboard_init() {
   put_str("keyboard init start\n");
   register_handler(0x21, intr_keyboard_handler);
   put_str("keyboard init done\n");
}

