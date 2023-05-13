.PHONY = bochs gdb disk dep clean mbr-disasm loader-disasm

INCS = -I lib/kernel/ -I lib/ -I kernel/
CFLAGS = -m32 -static -nostdlib -fno-stack-protector # -fno-builtin
LDFLAGS = -e main -static -Ttext 0xc0001500 -m elf_i386
NASMFLAGS = -f elf

CC = clang

bochs: mbr.bin loader.bin kernel.bin
	bochs -q -f bochs.conf

gdb: mbr.bin loader.bin kernel.bin
	BXSHARE=/usr/local/share/bochs bochs-gdb -q -f bochs-gdb.conf

mbr.bin: boot/mbr.S hd60M.img
	nasm -o mbr.bin -I boot/include boot/mbr.S
	dd if=mbr.bin of=hd60M.img bs=512B count=1 conv=notrunc

loader.bin: boot/loader.S hd60M.img
	nasm -o loader.bin -I boot/include boot/loader.S
	dd if=loader.bin of=hd60M.img bs=512B count=4 seek=2 conv=notrunc

kernel.bin: hd60M.img main.o init.o interrupt.o kernel.o print.o timer.o debug.o
	ld -o kernel.bin  $(LDFLAGS) main.o init.o interrupt.o kernel.o print.o timer.o debug.o
	# strip -R .got.plt kernel.bin -R .note.gnu.property -R .eh_frame kernel.bin
	dd if=kernel.bin of=hd60M.img bs=512B count=200 seek=9 conv=notrunc # dd is smart enough

main.o: kernel/main.c
	$(CC) -o main.o $(INCS) $(CFLAGS) -c kernel/main.c

init.o: kernel/init.c kernel/init.h
	$(CC) -o init.o $(INCS) $(CFLAGS) -c kernel/init.c

interrupt.o: kernel/interrupt.c kernel/interrupt.h
	$(CC) -o interrupt.o $(INCS) $(CFLAGS) -c kernel/interrupt.c

kernel.o: kernel/kernel.S
	nasm -o kernel.o $(NASMFLAGS) kernel/kernel.S

print.o: lib/kernel/print.S lib/kernel/print.h
	nasm -o print.o $(NASMFLAGS) lib/kernel/print.S

timer.o: device/timer.c device/timer.h
	$(CC) -o timer.o $(INCS) $(CFLAGS) -c device/timer.c

debug.o: kernel/debug.c kernel/debug.h
	$(CC) -o debug.o $(INCS) $(CFLAGS) -c kernel/debug.c

disk:
	bximage -q -hd -mode="flat" -size=60 hd60M.img
	# TODO
	# dd if=/dev/zero of=hd60M.img bs=4K count=15360

loader-disasm: loader.bin
	objdump -D -b binary -mi386 -Mintel,i8086 loader.bin

mbr-disasm: mbr.bin
	objdump -D -b binary -mi386 -Mintel,i8086 mbr.bin
	# AT&T
	# objdump -D -b binary -mi386 -Maddr16,data16 mbr.bin
	# ndisasm -b16 -o7c00h -a -s7c3eh mbr.bin

clean:
	rm *.{bin,o,out} -rf

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
