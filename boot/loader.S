%include "boot.S"

section loader vstart=LOADER_BASE_ADDR

      ; actually it is initial stack top...
      ; the top will reduce down in protect mode
    LOADER_STACK_TOP equ LOADER_BASE_ADDR
    ; jmp near loader_start

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
    ; get selector
    SELECTOR_CODE  equ (0x0001 << 3) + TI_GDT + RPL0
    SELECTOR_DATA  equ (0x0002 << 3) + TI_GDT + RPL0
    SELECTOR_VIDEO equ (0x0003 << 3) + TI_GDT + RPL0

      ; reserve 60 entry for easy extend..
    times 60 dq 0

    ; mem size
      ; addr = 0x900 + (60+4)*8 = 0xb00
    total_mem_bytes dd 0

    ; content of gdtr
      ; to be load into gdtr
    gdt_ptr dw GDT_LIMIT
            dd GDT_BASE

    ; align to 256 = 244 + (2 + 6 + 4)
    ards_buf times 244 db 0
    ards_nr dw 0
    ; loader_msg db "loader in the real"

  [bits 16]
  loader_start:
    ; int 0x15, to get mem size
    xor ebx, ebx
    mov edx, 0x534d4150
    mov di, ards_buf
    .e820_mem_get_loop:
      mov eax, 0x0000e820
      mov ecx, 20
      int 0x15
        ; error if return cf = 1
      jc .e820_failed_so_try_e801
        ; not fail in current try
      inc word [ards_nr]
      add di, cx
        ; if ebx = 0, then no more ARDS
      cmp ebx, 0
      jnz .e820_mem_get_loop

      ; now we need to find the max addr area,
      ;   why `max`? just because mem is usually max...
        ; num of ARDS
      mov cx, [ards_nr]
        ; start of ARDS array
      mov ebx, ards_buf
        ; max size
      xor edx, edx
      .find_max_mem_area:
        ; no need to check type
        mov eax, [ebx]
        add eax, [ebx+8]
          ; base_low + len_low
          ; aim to find out "the highest place the addr can touch..."
        add ebx, 20
        cmp edx, eax
        jge .next_ards
        mov edx, eax
        .next_ards:
        loop .find_max_mem_area
        jmp .mem_get_ok

    .e820_failed_so_try_e801:
      mov ax, 0xe081
      int 0x15
      jc .e801_failed_so_try_88
        ; low mem part (<15MB)
      mov cx, 0x400
      mul cx ; dx:ax = ax*1024
      and eax, 0x0000ffff ; clear high bits
      shl edx, 16
      or edx, eax
      add edx, 0x100000
      mov esi, edx ; low 15 MB
        ; high mem part
      xor eax, eax
      mov ax, bx
      mov ecx, 0x10000
      mul ecx; edx:eax = bx*1024*64

        ; here we suppose mem <= 4G
        ; so trunc the high edx part
      add esi, eax
      mov edx, esi
      jmp .mem_get_ok

    .e801_failed_so_try_88:
      mov ah, 0x88
      int 0x15
      ; all policies fail...
      ; jc .error_hlt
      jc .mem_get_ok
      ; eax < 64 (KB)
      and eax, 0x0000ffff
      mov cx, 0x400
      mul cx ; dx:ax
      shl edx, 16
      or edx, eax
      add edx, 0x100000


    .mem_get_ok:
      mov [total_mem_bytes], edx

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

    ; load kernel
    mov eax, KERNEL_START_SECTOR
    mov ebx, KERNEL_BIN_BASE_ADDR
    mov ecx, 200 ; kernel <= 100K
    call rd_disk_m_32

    ; prepare to page
    ; NOTE: after paging, paddr -> vaddr
      ; explicitly remap paddr in vaddr
    call setup_page
      ; backup the gdtr before paging
        ; (though actually unnecessary here)
    sgdt [gdt_ptr]

      ; get video gdte
      ; remap video gdte's vaddr to 3G
    mov ebx, [gdt_ptr + 2]
    or dword [ebx + 0x18 + 4], 0xc0_000000

    ; remap the gdt base to 3G
    add dword [gdt_ptr + 2], 0xc0_000000
    add esp, 0xc0000000

    ; set cr3, point to the PD
    mov eax, PAGE_DIR_TABLE_POS
    mov cr3, eax

    ; enable PG in cr0
    mov eax, cr0
    or eax, 0x80000000
    mov cr0, eax

    ; load gdtr again after paging
    lgdt [gdt_ptr]

    ; flush the pipeline
    jmp SELECTOR_CODE:enter_kernel

    enter_kernel:
    mov byte [gs:160], 'V'
    ; jmp $
    call kernel_init ; load kernel into 3G+
    mov esp, 0xc009f000 ; 4k align ...
    jmp KERNEL_ENTRY_POINT ; 0x1500


    ; setup PD and PT
  setup_page:

    ; set PD at 0x100000
    mov ecx, 4096
    mov esi, 0
    .clear_page_dir:
      mov byte [PAGE_DIR_TABLE_POS + esi], 0
      inc esi
      loop .clear_page_dir

    ; first 3 pde
      ; vaddr 0x0 -> pt at 0x101000 -> paddr 0x0
      ; vaddr 3G  -> pt at 0x101000 -> paddr 0x0
    .create_pde:
      mov eax, PAGE_DIR_TABLE_POS
      add eax, 0x1000
      mov ebx, eax ; addr of 1th pt
        ; set low bit (i.e. attribution of entry)
      or eax, PG_US_U | PG_RW_W | PG_P
        ; pde: 0x0 -> pt at 0x101000
      mov [PAGE_DIR_TABLE_POS + 0x0], eax
        ; pde: 3G  -> pt at 0x101000
          ; (1024*3/4*4M)
      mov [PAGE_DIR_TABLE_POS + 0xc00], eax

      sub eax, 0x1000
        ; pde: top 1M -> pt at 0x100000 (i.e pde itself)
        ; which means the PD will be parsed as PT
      mov [PAGE_DIR_TABLE_POS + 4092], eax ; 4092 = 1023 * 4

    ; set PT at 0x101000
      ; map vaddr 0-1M -> paddr 0-1M
      ; set 256/1024 entry (256*4K=1M)
    mov ecx, 256
    mov esi, 0
    mov edx, PG_US_U | PG_RW_W | PG_P
    .create_pte:
      mov [ebx + esi * 4], edx
      add edx, 0x1000
      inc esi
      loop .create_pte

    ; set other pde (3G-4G)
    mov eax, PAGE_DIR_TABLE_POS
    add eax, 0x2000
    or eax, PG_US_U | PG_RW_W | PG_P
    mov ebx, PAGE_DIR_TABLE_POS
    mov ecx, 254
    mov esi, 769
    .create_kernel_pde:
      ; 769-1022(3G-4G) -> 0x102000,0x103000 ... 0x200000
      ; 1-767 ??
      mov [ebx + esi * 4], eax
      inc esi
      add eax, 0x1000
      loop .create_kernel_pde
      ret

  ; load entry into vaddr
kernel_init:
  xor eax, eax
  xor ebx, ebx ; e_phoff
  xor ecx, ecx ; e_phnum
  xor edx, edx ; e_phentsize

  mov dx, [KERNEL_BIN_BASE_ADDR + 42] ; 32B
  mov cx, [KERNEL_BIN_BASE_ADDR + 44] ; 8
  mov ebx, [KERNEL_BIN_BASE_ADDR + 28] ; 0x34 = 52
    ; e_phoff + base_addr = addr of first ph
  add ebx, KERNEL_BIN_BASE_ADDR

  .each_segment:
      ; if p_type = 0 (unused), then jmp
    cmp byte [ebx + 0], PT_NULL
    je .PTNULL

      ; load entry into vaddr?
    push dword [ebx + 16] ; p_filesz
    mov eax, [ebx + 4] ; p_offset
    add eax, KERNEL_BIN_BASE_ADDR
    push eax
    push dword [ebx + 8] ; p_vaddr
      ; memcpy(dst, src, size)
    call mem_cpy
    add esp, 12
  .PTNULL:
    add ebx, edx
    loop .each_segment
  ret

mem_cpy:
  cld ; set DF = 0 (edi, esi +)
  push ebp
  mov ebp, esp
  push ecx ; for loop, saved by callee

  mov edi, [ebp + 8] ; dst
  mov esi, [ebp + 12] ; src
  mov ecx, [ebp + 16] ; size
  rep movsb ; --ecx, [edi] <- [esi]

  ; restore the env
  pop ecx
  pop ebp
  ret

rd_disk_m_32:
     ; eax LBA (disk addr)
     ; ebx mem addr
     ; ecx sector count
    mov esi, eax
    mov di, cx

    ; set sector count
    mov dx, 0x1f2
    mov al, cl
    out dx, al

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
    and al, 0x0f ; low 4
    or al, 0xe0 ; high 4
    out dx, al

    ; set command
    mov dx, 0x1f7
    mov al, 0x20
    out dx, al

    ; PIO (bsy)
    .not_ready:
    nop
    in al, dx
    and al, 0x88
    cmp al, 0x08 ; 4th drdy, 7th bsy
    jnz .not_ready

    ; read
    mov ax, di
    mov dx, 256
    mul dx ; eax
    mov cx, ax
    mov dx, 0x1f0

    .go_on_read:
    in ax,dx
    mov [ebx], ax
    add ebx, 2
    loop .go_on_read
    ret
