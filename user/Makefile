.PHONY = bochs load clean

BUILD_DIR = ./build
LIB_DIR = ../build

$(shell mkdir -p $(BUILD_DIR))

AS = nasm
CC = clang
LD = ld

# ASINCS = -I boot/include
INCS = -I ../lib \
	   -I ../lib/user \
	   -I ../fs \

ASFLAGS = -f elf
CFLAGS = -m32 -static -fno-builtin -nostdlib -fno-stack-protector \
		 # -mno-sse
		 # -Wl -O2
		 # -W -Wstrict-prototypes -Wmissing-prototypes

LDFLAGS = -static -m elf_i386

OBJS = $(BUILD_DIR)/start.o \
		$(LIB_DIR)/string.o \
		$(LIB_DIR)/syscall.o \
		$(LIB_DIR)/stdio.o \
		$(LIB_DIR)/assert.o \

LIBS = $(BUILD_DIR)/naive-crt.a

# OBJS = $(BUILD_DIR)/main.o

all: $(BUILD_DIR)/hello.out \
	 $(BUILD_DIR)/fork-exec.out \
	 $(BUILD_DIR)/cat.out \
	 $(BUILD_DIR)/pipe.out \
	 $(BUILD_DIR)/echo.out \
	 $(BUILD_DIR)/c4.out \

$(BUILD_DIR)/naive-crt.a: $(OBJS)
	ar rcs $@ $^

$(BUILD_DIR)/hello.out: $(BUILD_DIR)/hello.o $(LIBS)
	$(LD) $(LDFLAGS) $^ -o $@

$(BUILD_DIR)/fork-exec.out: $(BUILD_DIR)/fork-exec.o $(LIBS)
	$(LD) $(LDFLAGS) $^ -o $@

$(BUILD_DIR)/cat.out: $(BUILD_DIR)/cat.o $(LIBS)
	$(LD) $(LDFLAGS) $^ -o $@

$(BUILD_DIR)/pipe.out: $(BUILD_DIR)/pipe.o $(LIBS)
	$(LD) $(LDFLAGS) $^ -o $@

$(BUILD_DIR)/echo.out: $(BUILD_DIR)/echo.o $(LIBS)
	$(LD) $(LDFLAGS) $^ -o $@

$(BUILD_DIR)/c4.out: $(BUILD_DIR)/c4.o $(LIBS)
	$(LD) $(LDFLAGS) $^ -o $@

$(BUILD_DIR)/start.o: start.S
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/hello.o: hello.c
	$(CC) $(INCS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/fork-exec.o: fork-exec.c
	$(CC) $(INCS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/cat.o: cat.c
	$(CC) $(INCS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/pipe.o: pipe.c
	$(CC) $(INCS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/echo.o: echo.c
	$(CC) $(INCS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/c4.o: c4.c
	$(CC) $(INCS) $(CFLAGS) -c $< -o $@

MAX_ELF_SIZE = 50

# load to raw disk
load:
	dd if=$(BUILD_DIR)/hello.out of=../boot.img bs=512 count=$(MAX_ELF_SIZE) seek=300 conv=notrunc
	dd if=$(BUILD_DIR)/fork-exec.out of=../boot.img bs=512 count=$(MAX_ELF_SIZE) seek=400 conv=notrunc
	dd if=$(BUILD_DIR)/cat.out of=../boot.img bs=512 count=$(MAX_ELF_SIZE) seek=500 conv=notrunc
	dd if=$(BUILD_DIR)/pipe.out of=../boot.img bs=512 count=$(MAX_ELF_SIZE) seek=600 conv=notrunc
	dd if=$(BUILD_DIR)/echo.out of=../boot.img bs=512 count=$(MAX_ELF_SIZE) seek=700 conv=notrunc
	dd if=$(BUILD_DIR)/c4.out of=../boot.img bs=512 count=$(MAX_ELF_SIZE) seek=800 conv=notrunc

clean:
	rm -rf -- $(BUILD_DIR)/*

