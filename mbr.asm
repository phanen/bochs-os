SECTION MBR vstart=0x7c00
    mov ax, cs
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov sp, 0x7c00

    ; clear the screen
      ; ah=0x6
    mov     ax, 0x600
    mov     bx, 0x700
      ; vga mode: (0, 0) -> (79, 24)
    mov     cx, 0
    mov     dx, 0x184f
    int     0x10

    ; get the pos of cursor
    mov ah, 3
    mov bh, 0
    int 0x10

    ; print message
    mov ax, message
      ; es:bp -> start of message
    mov bp, ax
      ; cx: len of
    mov cx, 11
    mov ax, 0x1301
      ; bh: page number
      ; bl: font attribute
    mov bx, 0x2
    int 0x10

    jmp $
    message db "hello world"
    times 510 - ($-$$) db 0
    db 0x55, 0xaa
