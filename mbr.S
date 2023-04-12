%include "boot.S"

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

      ; disk addr
    mov eax, LOADER_START_SECTOR
      ; mem addr
    mov bx, LOADER_BASE_ADDR
      ; len/ sector count
    mov cx, 4
    call rd_disk_m_16
    jmp LOADER_BASE_ADDR + 0x300

    ; load from disk to mem
    ; in 16 bit mode
rd_disk_m_16:
      ; eax = disk addr
      ; bx = mem addr
      ; cx = len/ sector count

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
      ; ... sleep a round
    nop
    in al, dx
      ; 7th: bsy, 4th: drdy
    and al, 0x88
      ; not bsy and rdy
    cmp al, 0x08
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
      ; check cs
    loop .go_on_read
    ret


    times 510-($-$$) db 0
    db 0x55, 0xaa
