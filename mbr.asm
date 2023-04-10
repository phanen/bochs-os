SECTION MBR vstart=0x7c00
    mov ax, cs
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov sp, 0x7c00
    mov ax, 0xb800
    mov gs, ax

    ; clear the screen
    mov     ax, 0x600
    mov     bx, 0x700
      ; vga mode: (0, 0) -> (79, 24)
    mov     cx, 0
    mov     dx, 0x184f
    int     0x10

    ; print blink 'king'
    mov byte [gs:0x00], 'k'
      ; 0xa: bg is green + blink
      ; 0x4: fg is red
    mov byte [gs:0x01], 0xa4

    mov byte [gs:0x02], 'i'
    mov byte [gs:0x03], 0xa4

    mov byte [gs:0x04], 'n'
    mov byte [gs:0x05], 0xa4

    mov byte [gs:0x06], 'g'
    mov byte [gs:0x07], 0xa4


    mov byte [gs:0x80], 'i'
    mov byte [gs:0x81], 0xa4
    mov byte [gs:0x82], 'n'
    mov byte [gs:0x83], 0xa4
    mov byte [gs:0x84], 'g'
    mov byte [gs:0x85], 0xa4
    mov byte [gs:0x24], 'k'
    mov byte [gs:0x25], 0xa4

    ; 100 char pre line....

    mov byte [gs:0xa0], 'x'
    mov byte [gs:0xa1], 0xa4
    ; overwrite x
    mov byte [gs:0xa0], 'y'
    mov byte [gs:0xa1], 0xa4

    ; write a while word
    mov word [gs:0x9e], 'c'
      ; LE, overwrite 'y'
    mov word [gs:0x9f], 0xa4
    mov dword [gs:0xa4], 'c'
    mov dword [gs:0xa5], 0xa4

    ; 'ga' in vim to see unicode
    ; wired encoding...
      ; utf: 4f60, nasm: 00a0bde4
    mov dword [gs:0x160], '你' 
    mov byte [gs:0x161], 0xa4
      ; 597d, 00bda5e5
    mov dword [gs:0x162], '好'
    mov byte [gs:0x163], 0xa4 

    jmp $
    times 510-($-$$) db 0
    db 0x55, 0xaa
