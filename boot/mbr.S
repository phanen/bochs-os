; vim:ft=nasm

%include "boot.S"

SECTION MBR vstart=0x7c00
    cli ; Disable interrupts
    cld ; String operations increment
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
    ; 100 char pre line....
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

    mov ah, 0x42
    mov dl, 0x80
    mov si, dap
    int 0x13 ; https://en.wikipedia.org/wiki/INT_13H
    jmp LOADER_ENTRY_POINT

    mov eax, LOADER_START_SECTOR ; disk addr
    mov bx, LOADER_BASE_ADDR ; mem addr
    mov cx, 4 ; len/ sector count
    call rd_disk_m_16
    jmp LOADER_ENTRY_POINT

dap:
    db 0x10                 ; packet size
    db 0                    ; ..
    dw 4                    ; block cnt
    dw LOADER_BASE_ADDR     ; mem offset
    dw 0x0000               ; mem seg
    dq LOADER_START_SECTOR  ; LBA=2

; load from disk to mem, in 16 bit mode
; http://wiki.osdev.org/ATA_PIO_Mode
; eax = disk addr
; bx = mem addr
; cx = len/ sector count
rd_disk_m_16:

      ; backup
    mov esi, eax
    mov di, cx

    ; set sector count
    mov dx, 0x1f2
    mov al, cl
    out dx, al

      ; resume
    mov eax, esi

    ; set lba low
    mov dx, 0x1f3
    out dx, al

    ; set lba mid
    mov dx, 0x1f4
    mov cl, 8
    shr eax, cl
    out dx, al

    ; set lba high
    mov dx, 0x1f5
    shr eax, cl
    out dx, al

    ; set device
    mov dx, 0x1f6
    shr eax, cl
      ; low 4:  addr
    and al, 0x0f
      ; high 4: mode
    or al, 0xe0
    out dx, al

    ; set command
    mov dx, 0x1f7
    mov al, 0x20
    out dx, al

    ; polling (PIO)
  .not_ready:
    nop
    in al, dx
    and al, 0x88 ; 7th: bsy, 4th: drdy
    cmp al, 0x08 ; not bsy and rdy
    jnz .not_ready

    ; read data
      ; ax = sec_cnt
    mov ax, di
      ; read_cnt = sec_cnt * bytes_per_sec / bytes_per_read = sec_cnt * 256
    mov dx, 256
    mul dx
      ; dx:ax := dx * ax
      ; cs = 0:ax = ax
    mov cx, ax

    mov dx, 0x1f0
  .go_on_read:
      ; read a word each time
    in ax, dx
      ; ensure bx <= 0xffff
      ; otherwise it wrap back to 0x0000
    mov [bx], ax
    add bx, 2
      ; check cx

    ; not for bochs, but necessary for qemu
    ; https://wiki.osdev.org/ATA_PIO_Mode#400ns_delays
    or dl, 7
    in al, dx ; delay 400ns to allow drive to set new values of BSY and DRQ
    .pior_l:
      in al, dx   ; grab a status byte
      test al, 0x80   ; BSY flag set?
      jne short .pior_l ; (all other flags are meaningless if BSY is set)
      test al, 0x21   ; ERR or DF set?
    sub dl, 7
    loop .go_on_read
    ret


    times 510-($-$$) db 0
    db 0x55, 0xaa
