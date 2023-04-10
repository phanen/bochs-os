%include "boot.S"

section loader vstart=LOADER_BASE_ADDR

      ; actually it is initial stack top...
      ; the top will reduce down in protect mode
    LOADER_STACK_TOP equ LOADER_BASE_ADDR
    jmp near loader_start

  ; the following is data area
    ; build gdt
    GDT_BASE: ; index 0 is unused
      dd 0x00000000
      dd 0x00000000
      ; a code seg (flat)
    GDT_DESC:
      dd 0x0000ffff
      dd DESC_CODE_HIGH4
      ; a data seg (flat)
    DATA_STACK_DESC:
      dd 0x0000ffff
      dd DESC_DATA_HIGH4    
      ; a display seg
    VIDOE_DESC:
      ; (0xbffff - 0xb8000) / 4k
      dd 0x80000007
      dd DESC_VIDEO_HIGH4

    GDT_SIZE  equ $ - GDT_BASE
    GDT_LIMIT equ GDT_SIZE - 1

      ; reserve 60 entry for easy extend..
    ; times 60 dq 0

    ; get selector
    SELECTOR_CODE  equ (0x0001 << 3) + TI_GDT + RPL0
    SELECTOR_DATA  equ (0x0002 << 3) + TI_GDT + RPL0
    SELECTOR_VIDEO equ (0x0003 << 3) + TI_GDT + RPL0

    ; content of gdtr
      ; to be load into gdtr
    gdt_ptr dw GDT_LIMIT
            dd GDT_BASE

    loader_msg db "loader in the real"

  loader_start:  
      ; print msg
    mov sp, LOADER_BASE_ADDR
    mov bp, loader_msg
    mov cx, 18
    mov ax, 0x1301
    mov bx, 0x001f
    mov dx, 0x1800
    int 0x10

    ; enable A20
    in al, 0x92
    or al, 0000_0010b
    out 0x92, al

    ; set gdtr
    lgdt [gdt_ptr]

    ; enable PE in CR0
    mov eax, cr0
    or eax, 0x00000001
    mov cr0, eax

    ; flush the pipeline?
      ; avoid to exec old prefetch-inst
    jmp dword SELECTOR_CODE:p_mode_start

[bits 32]
p_mode_start:

  mov ax, SELECTOR_DATA
  mov ds, ax
  mov es, ax
  mov ss, ax
  mov esp, LOADER_STACK_TOP
  mov ax, SELECTOR_VIDEO
  mov gs, ax
  
  mov byte[gs:160], 'P'
  jmp $
