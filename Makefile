.PHONY = run disk dep

run: mbr.bin
	bochs -q -f bochs.conf

mbr.bin: mbr.asm hd60M.img
	nasm mbr.asm -o mbr.bin
	dd if=mbr.bin of=hd60M.img bs=512B count=1 conv=notrunc

disk:
	bximage -q -hd -mode="flat" -size=60 hd60M.img
	# TODO
	# dd if=/dev/zero of=hd60M.img bs=4K count=15360

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
