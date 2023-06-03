## dependencies

The dependencies you may need:
- necessary
    - bochs: simulator to run the OS
    - gtk-2: dep for bochs, to display the virtual terminal
    - nasm: for IA-32 assembler
    - clang: compile the kernel
- optional
    - bochs-gdb: if you prefer gdb (but works not well, I personal use native bochs debugger)
    - gdb: debugger
    - bear: generate `compile_commands.json`, clangd lsp

(for archlinux) You can install all necessary dependencies as a target in `Makefile`
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

> other distro can install in similarly way

## build

Once you has all necessary dependencies installed, all you need is just `make`, and all chores will be done automatically.

You can also generate lsp support at the same time:
```bash
make clean && bear -- make
```

To know more about the build, you can `make` in the follow procedure:
```bash
# prepare a master disk image to store bootloaders and kernel
make master-hd60M.img

# prepare another disk image, and partitioning it (in mbr format)
make slave-hd80.img

# build an elf kernel, raw loader binary, and a mbr (512B)
make kernel.bin loader.bin mbr.bin
# load all these binary into the master disk
make boot-master

# load user process into master disk
# the kernel will load them into fs
make userelf

# run the os in bochs
make bochs
```

Trick: `make` use timestamp-based build by default, to force rebuilding a target with no dependencies, you can use `-B`:
```
make -B slave-hd80.img
```

## everything

> Keep in mind, that, there's no magic in a computer system.

This section aims to interpret everything about the OS in minimal words.

The basic hardware archtecture?

How does the OS run?
- First, the register of CPU (simulator) is set to a fixed value (address): `0xf000:fff0`
    - BTW, the magic number `0xf000:fff0` is just a initial status, which is a common conception of any state machine.
    - After initialization, all thing will be done automatically in the deterministic hardware rules. Until the only uncertainty happened: WIP
- Then, the CPU will read the code from that addr, which is (`jmp far f000:e05b`), then it will execute it to goto BIOS
    - BIOS is simply the first program to run. It's usually in ROM of motherboard, which is rarely be changed or even unchangable. It is usually designed as a hardcode by hardware vendor. It will 

How you interact with the OS?

How OS manage the hardware?

## some records

WIP

