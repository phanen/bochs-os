target remote 127.0.0.1:1234
set print pretty on

set logging enabled

layout asm
layout regs
# set follow-fork-mode child
# set disassembly-flavor intel

set arch i386
hbreak *0x7c00
# hbreak *0x7c20
hbreak *0x7c5c
# hbreak *0x7c59
hbreak *0xc00
# hbreak *0xc1a
# hbreak *0xc28
# hbreak *0xca6
#hbreak *0xcc2
#b *0xcc2
b *0xca8
b *0xcaf
b *0xcc2
# hbreak *0xcc8
# b *0xe61
b *0xe7a
b *0xd5c
# b *0xde0
b *0xd5c
# c
# hbreak *0xc1a
# hbreak *0x0caf

# continue
