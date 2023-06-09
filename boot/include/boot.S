
; loader (size < 2000B)
LOADER_BASE_ADDR    equ      0x900
LOADER_START_SECTOR equ        0x2
PAGE_DIR_TABLE_POS  equ   0x100000 ; paddr of first PD

; kernel (size < 100K)
KERNEL_START_SECTOR   equ        0x9 ; 0x2~0x8 for loader
KERNEL_BIN_BASE_ADDR  equ    0x70000 ; in 0x7e00~0x9fbff
KERNEL_ENTRY_POINT    equ 0xc0001500

; gdt attribution
DESC_G_4K         equ   10000000_00000000_00000000b
DESC_D_32         equ    1000000_00000000_00000000b
DESC_L            equ     000000_00000000_00000000b
DESC_AVL          equ      00000_00000000_00000000b

DESC_LIMIT_CODE2  equ       1111_00000000_00000000b
DESC_LIMIT_DATA2  equ              DESC_LIMIT_CODE2
DESC_LIMIT_VIDEO2 equ        000_00000000_00000000b

DESC_P            equ            10000000_00000000b

DESC_DPL_0        equ            00_00000_00000000b
DESC_DPL_1        equ            01_00000_00000000b
DESC_DPL_2        equ            10_00000_00000000b
DESC_DPL_3        equ            11_00000_00000000b

DESC_S_CODE       equ               10000_00000000b
DESC_S_DATA       equ                   DESC_S_CODE
DESC_S_SYS        equ               00000_00000000b

                                   ; xcra
DESC_TYPE_CODE    equ                1000_00000000b
                                   ; xewa
DESC_TYPE_DATA    equ                0010_00000000b

; seg base addr(31-24, 7-0) is set to 0
DESC_CODE_HIGH4   equ       (0x00 << 24) + DESC_G_4K + DESC_D_32 + \
                            DESC_L + DESC_AVL + DESC_LIMIT_CODE2 + \
                            DESC_P + DESC_DPL_0 + DESC_S_CODE + \
                            DESC_TYPE_CODE + 0x00

DESC_DATA_HIGH4   equ       (0x00 << 24) + DESC_G_4K + DESC_D_32 + \
                            DESC_L + DESC_AVL + DESC_LIMIT_DATA2 + \
                            DESC_P + DESC_DPL_0 + DESC_S_DATA + \
                            DESC_TYPE_DATA + 0x00

DESC_VIDEO_HIGH4  equ       (0x00 << 24) + DESC_G_4K + DESC_D_32 + \
                            DESC_L + DESC_AVL + DESC_LIMIT_VIDEO2 + DESC_P + \
                            DESC_DPL_0 + DESC_S_DATA + DESC_TYPE_DATA + 0x0b
; selector attribution
  ; selector index(high 13 bits) will be defined in loader.S,
  ;   together with segment descriptor
RPL0     equ    00b
RPL1     equ    01b
RPL2     equ    10b
RPL3     equ    11b
TI_GDT   equ   000b
TI_LDT   equ   100b


; page attribution
PG_P                equ     1b
PG_RW_R             equ    00b
PG_RW_W             equ    10b
; PL 0 1 2
PG_US_S             equ   000b
PG_US_U             equ   100b

; program header table attribution
PT_NULL             equ      0
