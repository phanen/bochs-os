.PHONY = bochs load clean

BUILD_DIR = ./build
LIB_DIR = ../build

AS = nasm
CC = clang
LD = ld

# ASINCS = -I boot/include
INCS = -I ../lib \
	   -I ../lib/user \
	   -I ../fs \

ASFLAGS = -f elf
CFLAGS = -m32 -static -fno-builtin -nostdlib -fno-stack-protector \
		 -mno-sse
		 # -Wl -O2
		 # -W -Wstrict-prototypes -Wmissing-prototypes

LDFLAGS = -e main -static -m elf_i386

LLIBS = $(LIB_DIR)/string.o \
	   $(LIB_DIR)/syscall.o \
	   $(LIB_DIR)/stdio.o \
	   $(LIB_DIR)/assert.o \

# OBJS = $(BUILD_DIR)/main.o

$(BUILD_DIR)/hello.out: $(BUILD_DIR)/hello.o $(LLIBS)
	$(LD) $(LDFLAGS) $^ -o $@

$(BUILD_DIR)/hello.o: hello.c
	$(CC) $(INCS) $(CFLAGS) -c $< -o $@

# nasm -f elf ./start.S -o ./start.o
# ar rcs simple_crt.a $OBJS start.o

# load to raw disk
load:
	dd if=$(BUILD_DIR)/hello.out of=../master-hd60M.img bs=512 count=30 seek=300 conv=notrunc

clean:
	rm -rf -- $(BUILD_DIR)/*

