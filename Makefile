.PHONY = run disk dep clean mbr-disasm loader-disasm

run: mbr.bin loader.bin kernel.bin
	bochs -q -f bochs.conf

mbr.bin: mbr.S hd60M.img
	nasm mbr.S -o mbr.bin -I include
	dd if=mbr.bin of=hd60M.img bs=512B count=1 conv=notrunc

loader.bin: loader.S hd60M.img
	nasm loader.S -o loader.bin -I include
	dd if=loader.bin of=hd60M.img bs=512B count=4 seek=2 conv=notrunc

kernel.bin: main.o main.c
	# nasm loader.S -o loader.bin -I include
	# dd is smart enough
	clang -o main.o -c -m32 --static -nostdlib main.c
	ld -o kernel.bin -e main --static -Ttext 0xc0001500 -m elf_i386 main.o
	# strip -R .got.plt kernel.bin -R .note.gnu.property -R .eh_frame kernel.bin
	dd if=kernel.bin of=hd60M.img bs=512B count=200 seek=9 conv=notrunc

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
	rm *.bin *.o *.out -rf

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
