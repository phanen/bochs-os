; nasm -f elf 1.asm
; nasm -f elf 2.asm
; ld 1.o 2.o -f elf_i386

section .bss
  resb 2*32

section file1data
  strHello db "Hello, world!", 0Ah
  STRLEN equ $ - strHello

section file1text
  extern print
  global _start

_start:
  push STRLEN
  push strHello
  call print

  mov ebx, 0
  mov eax, 1
  int 0x80
