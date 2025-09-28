include common.mk

BUILD_DIR = ./build
ELF_DIR = ${BUILD_DIR}/elf

$(shell mkdir -p $(BUILD_DIR))
$(shell mkdir -p $(ELF_DIR))

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
	   $(BUILD_DIR)/pid.o \
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
	   $(BUILD_DIR)/builtin_cmd.o \
	   $(BUILD_DIR)/path_parse.o \
	   $(BUILD_DIR)/exec.o \
	   $(BUILD_DIR)/wait_exit.o \
	   $(BUILD_DIR)/pipe.o \

# FIXME: dependencies not complete, it may cause unexpected error
#	for example:
#		if you see error stack magic, it may not caused by stackoverflow,
#		but you need to recomplie timer.c (due to change of task_struct)
.PHONY: run
run: boot.img fs.img userelf
	# nm build/kernel.elf | grep " T " | awk '{ print $$1" "$$3 }' > kernel.sym
	# nm build/kernel.elf | awk '{ print $$1" "$$3 }' > kernel.sym
	rm *.img.lock || true
	$(BOCHS) -f $(BCONF)

qemu: boot.img fs.img userelf
	$(QEMU) $(QFLAGS)

qemu-gdb: boot.img fs.img userelf
	$(QEMU) $(QFLAGS) -S -s &>/dev/null &
	gdb

boot.img: $(BUILD_DIR)/mbr.bin $(BUILD_DIR)/loader.bin $(BUILD_DIR)/kernel.elf
	# yes | $(BXIMAGE) -q -hd=60M -imgmode=flat -func=create $@
	# yes | $(BXIMAGE) -q -hd -mode="flat" -size=60 $@
	dd if=/dev/zero of=$@ bs=1M count=60
	dd if=$(BUILD_DIR)/mbr.bin of=$@ bs=512B count=1 conv=notrunc
	dd if=$(BUILD_DIR)/loader.bin of=$@ bs=512B count=4 seek=2 conv=notrunc
	# strip -R .got.plt kernel.elf -R .note.gnu.property -R .eh_frame kernel.elf
	dd if=$(BUILD_DIR)/kernel.elf of=$@ bs=512B count=200 seek=9 conv=notrunc # dd is smart enough

fs.img:
	dd if=/dev/zero of=$@ bs=1M count=80
	cat script/part.sfdisk | sfdisk fs.img

.PHONY: userelf
userelf: $(BUILD_DIR)/kernel.elf # ensure lib exist
	$(MAKE) -C ./user
	$(MAKE) -C ./user load

.PHONY: clean
clean:
	rm $(BUILD_DIR) -rf
	rm *.img || true
	rm *.img.lock || true

.PHONY: gdb
gdb: boot.img fs.img userelf
	gdb
	# bochs-gdb -q -f script/bochs-gdb.conf

.PHONY: doc
doc:
	sh ./script/doc-merge.sh ./doc > ./doc/merge.md

.PHONY: code
code:
	objdump -D -b binary -mi386 -Mintel,i8086 kernel.elf | less
	# AT&T
	# objdump -D -b binary -mi386 -Maddr16,data16 mbr.bin
	# ndisasm -b16 -o7c00h -a -s7c3eh mbr.bin
	# ndisasm -b16 -o900h -a -s7c3eh build/loader.bin

$(BUILD_DIR)/mbr.bin: boot/mbr.S
	$(AS) $(ASINCS) $< -o $@

$(BUILD_DIR)/loader.bin: boot/loader.S
	$(AS) $(ASINCS) $< -o $@

$(BUILD_DIR)/kernel.elf: $(OBJS)
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

$(BUILD_DIR)/pid.o: thread/pid.c thread/pid.h
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

$(BUILD_DIR)/builtin_cmd.o: shell/builtin_cmd.c shell/builtin_cmd.h
	$(CC) $(INCS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/path_parse.o: shell/path_parse.c shell/path_parse.h
	$(CC) $(INCS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/exec.o: userprog/exec.c userprog/exec.h
	$(CC) $(INCS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/wait_exit.o: userprog/wait_exit.c userprog/wait_exit.h
	$(CC) $(INCS) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/pipe.o: shell/pipe.c shell/pipe.h
	$(CC) $(INCS) $(CFLAGS) -c $< -o $@
