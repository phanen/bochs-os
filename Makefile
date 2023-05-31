.PHONY = bochs \
	gdb \
	boot-master \
	clean-disk \
	boot-master \
	dep \
	clean \
	mbr-disasm \
	loader-disasm

BUILD_DIR = ./build
ENTRY_POINT = 0xc0001500

AS = nasm
CC = clang
LD = ld

ASINCS = -I boot/include
INCS = -I lib/kernel/ \
	   -I lib/ \
	   -I kernel/ \
	   -I device/ \
	   -I thread/ \
	   -I userprog/ \
	   -I lib/user/ \
	   -I fs/ \
	   -I shell/ \

ASFLAGS = -f elf
CFLAGS = -m32 -static -fno-builtin -nostdlib -fno-stack-protector \
		 -mno-sse
		 # -W -Wstrict-prototypes -Wmissing-prototypes

LDFLAGS = -e main -static -Ttext $(ENTRY_POINT) -m elf_i386

OBJS = $(BUILD_DIR)/main.o \
	   $(BUILD_DIR)/init.o \
	   $(BUILD_DIR)/interrupt.o \
	   $(BUILD_DIR)/timer.o \
	   $(BUILD_DIR)/kernel.o \
	   $(BUILD_DIR)/print.o \
	   $(BUILD_DIR)/debug.o \
	   $(BUILD_DIR)/string.o \
	   $(BUILD_DIR)/bitmap.o \
	   $(BUILD_DIR)/memory.o \
	   $(BUILD_DIR)/thread.o \
	   $(BUILD_DIR)/list.o \
	   $(BUILD_DIR)/switch.o \
	   $(BUILD_DIR)/sync.o \
	   $(BUILD_DIR)/console.o \
	   $(BUILD_DIR)/keyboard.o \
	   $(BUILD_DIR)/ioqueue.o \
	   $(BUILD_DIR)/tss.o \
	   $(BUILD_DIR)/process.o \
	   $(BUILD_DIR)/syscall.o \
	   $(BUILD_DIR)/syscall-init.o \
	   $(BUILD_DIR)/stdio.o \
	   $(BUILD_DIR)/stdio-kernel.o \
	   $(BUILD_DIR)/ide.o \
	   $(BUILD_DIR)/fs.o \
	   $(BUILD_DIR)/dir.o \
	   $(BUILD_DIR)/file.o \
	   $(BUILD_DIR)/inode.o \
	   $(BUILD_DIR)/fork.o \
	   $(BUILD_DIR)/assert.o \
	   $(BUILD_DIR)/shell.o \

# FIXME: dependencies not complete, it may cause unexpected error
#	for example:
#		if you see error stack magic, it may not caused by stackoverflow,
#		but you need to recomplie timer.c (due to change of task_struct)
bochs: boot-master slave-hd80M.img
	bochs -q -f bochs.conf

gdb: boot-master partition-slave
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

$(BUILD_DIR)/memory.o: kernel/memory.c kernel/memory.h
	$(CC) $(INCS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/thread.o: thread/thread.c thread/thread.h
	$(CC) $(INCS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/sync.o: thread/sync.c thread/sync.h
	$(CC) $(INCS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/list.o: lib/kernel/list.c lib/kernel/list.h
	$(CC) $(INCS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/switch.o: thread/switch.S
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/console.o: device/console.c device/console.h
	$(CC) $(INCS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/keyboard.o: device/keyboard.c device/keyboard.h
	$(CC) $(INCS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/ioqueue.o: device/ioqueue.c device/ioqueue.h
	$(CC) $(INCS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/tss.o: userprog/tss.c userprog/tss.h
	$(CC) $(INCS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/process.o: userprog/process.c userprog/process.h
	$(CC) $(INCS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/syscall.o: lib/user/syscall.c lib/user/syscall.h
	$(CC) $(INCS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/syscall-init.o: userprog/syscall-init.c userprog/syscall-init.h
	$(CC) $(INCS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/stdio.o: lib/stdio.c lib/stdio.h
	$(CC) $(INCS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/stdio-kernel.o: lib/kernel/stdio-kernel.c lib/kernel/stdio-kernel.h
	$(CC) $(INCS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/ide.o: device/ide.c device/ide.h
	$(CC) $(INCS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/fs.o: fs/fs.c fs/fs.h
	$(CC) $(INCS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/dir.o: fs/dir.c fs/dir.h
	$(CC) $(INCS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/file.o: fs/file.c fs/file.h
	$(CC) $(INCS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/inode.o: fs/inode.c fs/inode.h
	$(CC) $(INCS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/fork.o: userprog/fork.c userprog/fork.h
	$(CC) $(INCS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/assert.o: lib/user/assert.c lib/user/assert.h
	$(CC) $(INCS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/shell.o: shell/shell.c shell/shell.h
	$(CC) $(INCS) $(CFLAGS) -c $< -o $@

# https://stackoverflow.com/questions/816370/how-do-you-force-a-makefile-to-rebuild-a-target
master-hd60M.img:
	rm -rf $@ && bximage -q -hd -mode="flat" -size=60 $@
	# TODO: dd if=/dev/zero of=hd60M.img bs=4K count=15360

slave-hd80M.img:
	rm -rf $@ && bximage -q -hd -mode="flat" -size=80 $@
	cat partition-slave.sfdisk | sfdisk $@

boot-master: master-hd60M.img $(BUILD_DIR)/mbr.bin $(BUILD_DIR)/loader.bin $(BUILD_DIR)/kernel.bin
	dd if=$(BUILD_DIR)/mbr.bin of=$< bs=512B count=1 conv=notrunc
	dd if=$(BUILD_DIR)/loader.bin of=$< bs=512B count=4 seek=2 conv=notrunc
	# strip -R .got.plt kernel.bin -R .note.gnu.property -R .eh_frame kernel.bin
	dd if=$(BUILD_DIR)/kernel.bin of=$< bs=512B count=200 seek=9 conv=notrunc # dd is smart enough

clean-disk:
	rm master-hd60M.img slave-hd80M.img -rf

loader-disasm: loader.bin
	objdump -D -b binary -mi386 -Mintel,i8086 $<

mbr-disasm: mbr.bin
	objdump -D -b binary -mi386 -Mintel,i8086 $<
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
