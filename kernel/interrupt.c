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

#define IDT_DESC_CNT 0x81 // current total num of intr

#define EFLAGS_IF   0x00000200  // mask of eflags.if
#define GET_EFLAGS(EFLAG_VAR) asm volatile("pushfl; popl %0" : "=g" (EFLAG_VAR)) // dump eflags to a macro?

// idt entry
struct gate_desc {
  uint16_t    func_offset_low_word;
  uint16_t    selector;
  uint8_t     dcount; // fixed field
  uint8_t     attribute;
  uint16_t    func_offset_high_word;
};

static void make_idt_desc(struct gate_desc* p_gdesc, uint8_t attr, intr_handler function);

// static mem for idt
static struct gate_desc idt[IDT_DESC_CNT];
// intr handler: asm entry
extern intr_handler intr_entry_table[IDT_DESC_CNT];
// intr handler: c body
intr_handler idt_table[IDT_DESC_CNT]; // use c to define body of handler
char *intr_name[IDT_DESC_CNT]; // for debug ...

// intr handler for syscall: asm entry
extern uint32_t syscall_handler(void);

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

  // enable slave(irq2), keyboard(irq1), clock(irq0)
  outb (PIC_M_DATA, 0xf8);  // OCW1: IMR 11111000
  // enable disk(irq14)
  outb (PIC_S_DATA, 0xbf);  // OCW1: IMR 10111111

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

// init all desc
static void idt_desc_init(void) {
  int i;
  for (i = 0; i < IDT_DESC_CNT; i++) {
    make_idt_desc(&idt[i], IDT_DESC_ATTR_DPL0, intr_entry_table[i]);
  }

  // use last entry as syscall
  make_idt_desc(&idt[IDT_DESC_CNT - 1], IDT_DESC_ATTR_DPL3, syscall_handler);
  put_str("   idt_desc_init done\n");
}

// general-purpose or so_called `template` handler
// support debug
static void general_intr_handler(uint8_t vec_nr) {
  // IRQ7 or IRQ15 -> spurious interrupt
  if (vec_nr == 0x27 || vec_nr == 0x2f) return;
  //
  // put_str("int vector: 0x"); put_int(vec_nr); put_char('\n');

  // pad a empty screen area
  set_cursor(0);
  for (int cursor_pos = 0; cursor_pos < 320; cursor_pos++) {
    put_char(' ');
  }
  set_cursor(0);
  put_str("!!!!!!!      exception message begin  !!!!!!!!\n");
  set_cursor(88); // 1:8
  put_str(intr_name[vec_nr]);

  if (vec_nr == 14) { // for page fault, we show more info here
    // print cr2 (the vaddr)
    int page_fault_vaddr = 0;
    asm ("movl %%cr2, %0" : "=r" (page_fault_vaddr));
    put_str("\npage fault addr is "); put_int(page_fault_vaddr);
  }
  put_str("\n!!!!!!!      exception message end    !!!!!!!!\n");
  while(1); // eflags.if = 0
}

// init register and `intr_name`
static void exception_init(void) {
  int i;
  for (i = 0; i < IDT_DESC_CNT; i++) {
    idt_table[i] = general_intr_handler; // init from template handler for now, we'll extend it by `register` in future
    intr_name[i] = "unknown intr or handler unimplemented"; // we will implement 20 of 33 entries next, so randomly name all first...
  }

  intr_name[0] = "#DE Divide Error";
  intr_name[1] = "#DB Debug Exception";
  intr_name[2] = "NMI Interrupt";
  intr_name[3] = "#BP Breakpoint Exception";
  intr_name[4] = "#OF Overflow Exception";
  intr_name[5] = "#BR BOUND Range Exceeded Exception";
  intr_name[6] = "#UD Invalid Opcode Exception";
  intr_name[7] = "#NM Device Not Available Exception";
  intr_name[8] = "#DF Double Fault Exception";
  intr_name[9] = "Coprocessor Segment Overrun";
  intr_name[10] = "#TS Invalid TSS Exception";
  intr_name[11] = "#NP Segment Not Present";
  intr_name[12] = "#SS Stack Fault Exception";
  intr_name[13] = "#GP General Protection Exception";
  intr_name[14] = "#PF Page-Fault Exception";
  // intr_name[15] reserved for intel?
  intr_name[16] = "#MF x87 FPU Floating-Point Error";
  intr_name[17] = "#AC Alignment Check Exception";
  intr_name[18] = "#MC Machine-Check Exception";
  intr_name[19] = "#XF SIMD Floating-Point Exception";

}

// register the intr handler by vector_no
void register_handler(uint8_t vector_no, intr_handler function) {
  // kernel/kernel.S -> call [idt_table + %1*4]
  idt_table[vector_no] = function;
}

// enable intr and return old status
enum intr_status intr_enable() {
  enum intr_status old_status;
  if (INTR_ON == intr_get_status()) {
    old_status = INTR_ON;
    return old_status;
  } else {
    old_status = INTR_OFF;
    asm volatile("sti"); // eflags.if=1
    return old_status;
  }
}

// disable intr and return old status
enum intr_status intr_disable() {
  enum intr_status old_status;
  if (INTR_ON == intr_get_status()) {
    old_status = INTR_ON;
    asm volatile("cli" : : : "memory");  // eflags.if=0
    return old_status;
  } else {
    old_status = INTR_OFF;
    return old_status;
  }
}

enum intr_status intr_set_status(enum intr_status status) {
  return status & INTR_ON ? intr_enable() : intr_disable();
}

enum intr_status intr_get_status() {
  uint32_t eflags = 0;
  GET_EFLAGS(eflags);
  return (EFLAGS_IF & eflags) ? INTR_ON : INTR_OFF;
}

// main procedure to do init
void idt_init() {
  put_str("idt_init start\n");
  idt_desc_init();
  exception_init(); // assign exception name, register handler
  pic_init();
  // intr_disable();
  // why uint32_t?
  uint64_t idt_operand = ((sizeof(idt) - 1) | ((uint64_t)idt << 16));
  asm volatile("lidt %0" : : "m" (idt_operand));
  put_str("idt_init done\n");
}
