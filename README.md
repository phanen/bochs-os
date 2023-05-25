
## build

If run from scratch, you need to prepare two disk image
```bash
# disk image to store bootloaders(mbr, osloader) and kernel
make raw-boot-disk
# another disk image, which has a filesystem on it
make raw-slave-disk
```

After preparation of your disk
```bash
# both build and generate `compile_commands.json` for lsp:
bear -- make bochs
```

## dependencies

The dependencies you may need:
- necessary
    - bochs: simulator to run the OS
    - gtk-2: dep for bochs, to display the virtual terminal
    - nasm: for IA-32 assembler
    - clang: compile the kernel
- optional
    - bochs-gdb
    - gdb: debugger

(for archlinux) You can install all dependencies as a target in `Makefile`
> you can also try bochs in AUR (not tested)
```make
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
```

> other distro can be install in similarly way
