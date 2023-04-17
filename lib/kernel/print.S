TI_GDT         equ  0
RPL0           equ  000
SELECTOR_VIDEO equ (0x0003 << 3) + TI_GDT + RPL0

[bits 32]
section .text
global put_char ; export
put_char:
      ; push all double: eax ecx edx ebx esp ebp esi edi
    pushad
    mov ax, SELECTOR_VIDEO
    mov gs, ax

    ; get the pos of cursor (1D, 80 * 25 = 2000)
      ; get high 8 bits of pos
    mov dx, 0x03d4
    mov al, 0x0e
    out dx, al
    mov dx, 0x03d5
    in al, dx
    mov ah, al

      ; get low 8 bits of pos
    mov dx, 0x03d4
    mov al, 0x0f
    out dx, al
    mov dx, 0x03d5
    in al, dx

    mov bx, ax
    ; get the char from stack
    mov ecx, [esp + 36] ; 36 = 8 dw.. + size of eip

    ; different case: normal char or ...
    cmp cl, 0xd
    jz .is_carriage_return ; jmp if equ
    cmp cl, 0xa
    jz .is_inline_feed
    cmp cl, 0x8
    jz .is_backsapce
    jmp .put_other

  .is_backsapce:
    dec bx ; TODO: what if bx = -1 ...
    shl bx, 1 ; 1 char = 2 byte (ascii + attribute..)
    ; put a space to fake
    mov byte [gs: bx], 0x20
    inc bx
    mov byte [gs: bx], 0x07
    shr bx, 1
    jmp .set_cursor ; set cursor on pos bx

  .put_other:
    shl bx, 1 ; pos to byte
    mov [gs: bx], cl        ; put ascii in g_mem
    inc bx
    mov byte [gs: bx], 0x07 ; put attr in g_mem
    shr bx, 1 ; resume
    inc bx
    cmp bx, 2000
      ; if bx(pos) is still in screen, then set cursor
    jl .set_cursor
      ; else fallback to CRLF ??

  .is_inline_feed: ; \n LF
  .is_carriage_return: ; \r CR
    ; LF or CR -> CRLF (like linux)
      ; do \r
    xor dx, dx
    mov ax, bx
    mov si, 80
    div si ; dx = bx % 80
    sub bx, dx ;  pos after cr = bx - bx % 80
  .is_carriage_end:
      ; do \n
    add bx, 80
    cmp bx, 2000
  .is_inline_feed_end:
      ; if bx is still in screen, then set cursor
    jl  .set_cursor
      ; else fallback to roll screen

  .roll_screen:
    ; copy line 1-24 -> line 0-23
    cld ; set DF, direction of edi esi
    mov ecx, 960 ; (2000-80)*2/4
                ; move 4 bytes each time
    mov esi, 0xc00b80a0 ; src: line 0-23
    mov edi, 0xc00b8000 ; dst: line 1-24
    rep movsd

    ; fill new line 24 with space
    mov ebx, 3840
    mov ecx, 80
    .cls:
    mov word [gs: ebx], 0x0720
    add ebx, 2
    loop .cls
     ; new cursor will be at the start of last line
    mov bx, 1920

  ; set cursor on pos bx
  .set_cursor:
    ; set high 8 bits
    mov dx, 0x03d4
    mov al, 0x0e
    out dx, al
    mov dx, 0x03d5
    mov al, bh
    out dx, al

    ; set high 8 bits
    mov dx, 0x03d4
    mov al, 0x0f
    out dx, al
    mov dx, 0x03d5
    mov al, bl
    out dx, al
    .put_char_done:
    popad
    ret

global put_str
put_str:
    push ebx
    push ecx
    xor ecx, ecx
    mov ebx, [esp + 12] ; 4 + 4 + size(eip)
  .goon:
    mov cl, [ebx]
    cmp cl, 0
      ; c str end with \0
    jz .str_over
    push ecx
    call put_char
    add esp, 4
    inc ebx
    jmp .goon

  .str_over:
    pop ecx
    pop ebx
    ret
