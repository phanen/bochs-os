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

> other distro can install in similar way

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

How does the OS run/boot?
- First, the register of CPU (simulator) is set to a fixed value (address): `0xf000:fff0`
    - BTW, the magic number `0xf000:fff0` is just a initial status, which is a common conception of any state machine.
    - After initialization, all thing will be done automatically in the deterministic hardware rules. Until the only uncertainty happened: WIP
- Then, the CPU will read the code from that address, which is (`jmp far f000:e05b`), then it will execute it to goto BIOS
    - BIOS is simply the first program to run. It's usually in the ROM of motherboard, which is rarely changed or even unchangable, designed as a hardcode by hardware vendor. During this process, the CPU will scan your hardware and execute some necessary check.
    - After BIOS, you will also get an interrupt vector table (IVT). In the past time, DOS as a system run on real mode, is basically built on such interface. In a word, IVT is a basic abstract, which get you rid of directly manipulating device port or viewing tons of register manual. It's a good friend of system programmer. (However, we will see goodbye to him, when we enter protection mode.)
    - At the end of BIOS execution, it will load a disk area called MBR then jmp to it. That's actually where you can hack start from. 
- MBR is most well known as its address `0x7c00` (btw, we can only at most 20 bit address by combining register `cs` and register `ip`, before goto protection mode).
    - Since our system is actually designed from here, and this size of MBR is limited in 1 sector (512MB), we will only implement a loader on it, which load another large area of disk into memory, then jump to it (like what BIOS does, this process is often know as link load, though low efficient...). We will call the new area the OSloader anyway.
    - MBR is directly written in assemble code (fixed format), and there's another conventions. The last two bytes shuld be 0x55 0xaa. the magic number means that this sector(or disk, MBR is located in the first sectors) is bootable, in order to be located by BIOS routine. And the last 64 bytes before magic number is used to store disk partition table(DPT), to denote where does the disk start or end.
- The OSloader will enable protection mode and paging machism ("enable" is actually a legacy conception, nowadays these behaviours should usually boot as default).

How do you interact with the OS?

How does OS manage the hardware?

How does OS manage the process?

## some records
WIP

## TODO

- [ ] port to qemu
- [ ] built-in editor
- [ ] shell readline support
- [ ] built-in compiler
