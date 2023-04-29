#include "interrupt.h"
#include "stdint.h"
#include "global.h"
#include "io.h"
#include "print.h"

// 8259A
#define PIC_M_CTRL 0x20 // master ctrl port
#define PIC_M_DATA 0x21 // master data port
#define PIC_S_CTRL 0xa0 // slave ctrl port
#define PIC_S_DATA 0xa1 // salve data port

#define IDT_DESC_CNT 0x21 // current total num of intr

// idt entry
struct gate_desc {
  uint16_t    func_offset_low_word;
  uint16_t    selector;
  uint8_t     dcount; // fixed field
  uint8_t     attribute;
  uint16_t    func_offset_high_word;
};

// static mem for idt
static struct gate_desc idt[IDT_DESC_CNT];
// get the addr of handler
extern intr_handler intr_entry_table[IDT_DESC_CNT];

static void make_idt_desc(struct gate_desc* p_gdesc, uint8_t attr, intr_handler function);

// init 8259A
static void pic_init(void) {
  // master
  outb (PIC_M_CTRL, 0x11);  // ICW1: LTIM, SNGL, IC4
  outb (PIC_M_DATA, 0x20);  // ICW2: IR[0-7], 0x20 ~ 0x27
  outb (PIC_M_DATA, 0x04);  // ICW3: IR[2] -> slave
  outb (PIC_M_DATA, 0x01);  // ICW4: uPM=1, AEOI=0

  // slave
  outb (PIC_S_CTRL, 0x11);	// ICW1: LTIM, SNGL, IC4
  outb (PIC_S_DATA, 0x28);	// ICW2: IR[8-15] 0x28 ~ 0x2F
  outb (PIC_S_DATA, 0x02);	// ICW3: slave -> IR[2]
  outb (PIC_S_DATA, 0x01);	// ICW4: uPM=1, AEOI=0

  // block all intrrupt except clock intrrupt
  outb (PIC_M_DATA, 0xfe);  // OCW1: IMR, only IR0 (clock intr)
  outb (PIC_S_DATA, 0xff);  // OCW1: IMR, all masked

  put_str("   pic_init done\n"); // manual tab, emm...
}

// create idt entry
static void make_idt_desc(struct gate_desc* p_gdesc, uint8_t attr, intr_handler function) {
  p_gdesc->func_offset_low_word = (uint32_t)function & 0x0000FFFF;
  p_gdesc->selector = SELECTOR_K_CODE;
  p_gdesc->dcount = 0;
  p_gdesc->attribute = attr;
  p_gdesc->func_offset_high_word = ((uint32_t)function & 0xFFFF0000) >> 16;
}

// init
static void idt_desc_init(void) {
  int i;
  for (i = 0; i < IDT_DESC_CNT; i++) {
    make_idt_desc(&idt[i], IDT_DESC_ATTR_DPL0, intr_entry_table[i]);
  }
  put_str("   idt_desc_init done\n");
}

// main procedure to do init
void idt_init() {
  put_str("idt_init start\n");
  idt_desc_init();
  pic_init();

    // why uint32_t?
  uint64_t idt_operand = ((sizeof(idt) - 1) | ((uint64_t)((uint32_t)idt << 16)));
  asm volatile("lidt %0" : : "m" (idt_operand));
  put_str("idt_init done\n");
}
