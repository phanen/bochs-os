target remote 127.0.0.1:1234
set print pretty on

set logging enabled

layout asm
layout regs
# set follow-fork-mode child
# set disassembly-flavor intel

set arch i386
hbreak *0x7c00
hbreak *0x7c20
hbreak *0x7c5f
# c
# hbreak *0xc1a
# hbreak *0x0caf

# continue
