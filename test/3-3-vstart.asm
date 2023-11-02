; mbr is hoped to be load in 0x7c00
section code vstart=0x7c00

  ; offset determined by file, not by vstart
  mov ax,section.code.start
  mov ax,section.data.start

  ; calc based on vstart
  mov ax,$$
  mov ax,$
  mov ax,[var1]
  mov ax,[var2]
  jmp $ ; FIXME: bug in ndisasm

section data vstart=0x900
  var1 dd 0x4
  var2 dw 0x99
