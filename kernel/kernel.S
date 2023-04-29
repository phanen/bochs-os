[bits 32]
%define ERROR_CODE nop ; error-code intr, do nothing
%define ZERO push 0 ; non-error-code intr, then pad a dummy error_code

extern put_str
section .data
intr_str db "interrupt occur!", 0xa, 0

global intr_entry_table
intr_entry_table:

; a helper to contruct interrupt handler
  ; 2 args: iv number, pre-hook (good idea)
%macro VECTOR 2
section .text
  intr%1entry:

  %2 ; push a dummy or not, depend on if error code exixt

    ; logging as handler
  push intr_str
  call put_str
  add esp, 4

  ; send EOI to 8259A
  mov al, 0x20
  out 0xa0, al ; to master chip
  out 0x20, al ; to slave chip

  ; ignore error code before iret
  add esp, 4
  iret

  ; be at ease, all data will finally merge together
  ; so we'll exactly get a array `intr_entry_table`
  ; where we can find all addr of interrupt
section .data
  dd intr%1_entry
%endmacro

; define 33 interrupt handler
  ; 0-19  is built-in for CPU
  ; 20-31 is reserved by intel
  ; so OS can customize from 0x20?
VECTOR 0x00, ZERO
VECTOR 0x01, ZERO
VECTOR 0x02, ZERO
VECTOR 0x03, ZERO
VECTOR 0x04, ZERO
VECTOR 0x05, ZERO
VECTOR 0x06, ZERO
VECTOR 0x07, ZERO
VECTOR 0x08, ZERO
VECTOR 0x09, ZERO
VECTOR 0x0a, ZERO
VECTOR 0x0b, ZERO
VECTOR 0x0c, ZERO
VECTOR 0x0d, ZERO
VECTOR 0x0e, ZERO
VECTOR 0x0f, ZERO
VECTOR 0x10, ZERO
VECTOR 0x11, ZERO
VECTOR 0x12, ZERO
VECTOR 0x13, ZERO
VECTOR 0x14, ZERO
VECTOR 0x15, ZERO
VECTOR 0x16, ZERO
VECTOR 0x17, ZERO
VECTOR 0x18, ZERO
VECTOR 0x19, ZERO
VECTOR 0x1a, ZERO
VECTOR 0x1b, ZERO
VECTOR 0x1c, ZERO
VECTOR 0x1d, ZERO
VECTOR 0x1e, ERROR_CODE
VECTOR 0x1f, ZERO
VECTOR 0x20, ZERO