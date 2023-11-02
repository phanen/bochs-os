AS = nasm
CC = clang
LD = ld

ASFLAGS = -f elf -g
CFLAGS = -m32 -static -fno-builtin -nostdlib -fno-stack-protector \
		 -mno-sse -g
		 # -W -Wstrict-prototypes -Wmissing-prototypes

ENTRY_POINT = 0xc0001500
LDFLAGS = -e main -static -Ttext $(ENTRY_POINT) -m elf_i386

BOCHS = bochs -q
BCONF = script/bochs.conf
BXIMAGE = bximage
