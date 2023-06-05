target remote 127.0.0.1:1234
set logging enabled

layout asm
layout regs
# set follow-fork-mode child
# set disassembly-flavor intel

set arch i386
break *0x7c00
# break *0xc1a
break *0x0caf

# continue
