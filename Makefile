.PHONY = bochs gdb empty-disk disk dep clean mbr-disasm loader-disasm

BUILD_DIR = ./build
ENTRY_POINT = 0xc0001500

AS = nasm
CC = clang
LD = ld

ASINCS = -I boot/include
INCS = -I lib/kernel/ -I lib/ -I kernel/ -I device/

ASFLAGS = -f elf
CFLAGS = -m32 -static -fno-builtin -nostdlib -fno-stack-protector \
		 # -W -Wstrict-prototypes -Wmissing-prototypes
LDFLAGS = -e main -static -Ttext $(ENTRY_POINT) -m elf_i386

OBJS = $(BUILD_DIR)/main.o $(BUILD_DIR)/init.o $(BUILD_DIR)/interrupt.o \
	   $(BUILD_DIR)/timer.o $(BUILD_DIR)/kernel.o $(BUILD_DIR)/print.o \
	   $(BUILD_DIR)/debug.o $(BUILD_DIR)/string.o $(BUILD_DIR)/bitmap.o

bochs: disk
	bochs -q -f bochs.conf

gdb: mbr.bin loader.bin kernel.bin
	BXSHARE=/usr/local/share/bochs bochs-gdb -q -f bochs-gdb.conf

$(BUILD_DIR)/mbr.bin: boot/mbr.S
	$(AS) $(ASINCS) $< -o $@

$(BUILD_DIR)/loader.bin: boot/loader.S
	$(AS) $(ASINCS) $< -o $@

$(BUILD_DIR)/kernel.bin: $(OBJS)
	$(LD) $(LDFLAGS) $^ -o $@

$(BUILD_DIR)/kernel.o: kernel/kernel.S
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/print.o: lib/kernel/print.S lib/kernel/print.h
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/main.o: kernel/main.c kernel/init.h lib/kernel/print.h lib/stdint.h kernel/init.h
	$(CC) $(INCS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/init.o: kernel/init.c kernel/init.h
	$(CC) $(INCS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/interrupt.o: kernel/interrupt.c kernel/interrupt.h
	$(CC) $(INCS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/timer.o: device/timer.c device/timer.h
	$(CC) $(INCS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/debug.o: kernel/debug.c kernel/debug.h
	$(CC) $(INCS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/string.o: lib/string.c lib/string.h
	$(CC) $(INCS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/bitmap.o: lib/kernel/bitmap.c lib/kernel/bitmap.h
	$(CC) $(INCS) $(CFLAGS) -c $< -o $@

empty-disk:
	bximage -q -hd -mode="flat" -size=60 hd60M.img
	# TODO
	# dd if=/dev/zero of=hd60M.img bs=4K count=15360

disk: $(BUILD_DIR)/mbr.bin $(BUILD_DIR)/loader.bin $(BUILD_DIR)/kernel.bin
	dd if=$(BUILD_DIR)/mbr.bin of=hd60M.img bs=512B count=1 conv=notrunc
	dd if=$(BUILD_DIR)/loader.bin of=hd60M.img bs=512B count=4 seek=2 conv=notrunc
	# strip -R .got.plt kernel.bin -R .note.gnu.property -R .eh_frame kernel.bin
	dd if=$(BUILD_DIR)/kernel.bin of=hd60M.img bs=512B count=200 seek=9 conv=notrunc # dd is smart enough

loader-disasm: loader.bin
	objdump -D -b binary -mi386 -Mintel,i8086 loader.bin

mbr-disasm: mbr.bin
	objdump -D -b binary -mi386 -Mintel,i8086 mbr.bin
	# AT&T
	# objdump -D -b binary -mi386 -Maddr16,data16 mbr.bin
	# ndisasm -b16 -o7c00h -a -s7c3eh mbr.bin

clean:
	rm *.{bin,o,out} -rf
	rm build/* -rf

dep:
	sudo pacman -S nasm gtk-2 xorg
	wget "https://sourceforge.net/projects/bochs/files/bochs/2.6.2/bochs-2.6.2.tar.gz/download" &&\
		tar -xzf bochs-2.6.2.tar.gz &&\
		(cd bochs-2.6.2 && ./configure \
		--enable-debugger \
		--enable-disasm \
		--enable-iodebug \
		--enable-x86-debugger \
		--with-x \
		--with-x11)
	$(MAKE) -C bochs-2.6.2 install
