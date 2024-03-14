## features
* a x86 operating system (tested on bochs-os)
* round-robbin schedule
* virtual memory management
* ext2-like file system
* unix-like syscall support
* support 32 bit elf
![pic](http://img.phanium.top/20230617161507.png)

> documents: [doc.md](./doc/doc.md)
> testcases: [test.md](./doc/tests.md)

## dependencies
* `bochs` (emulator)
    * (with `bximage` to create disk image)
* `gtk2` (gui)
* `nasm` (x86 assembler)
* `clang` (or `gcc`)
* `gdb` (optional)
* `bochs-gdb` (optional)
* `bear` (optional)

for archlinux: (`bochs` can be found in aur or archlinuxcn repo)
```sh
sudo yay -S nasm gtk-2 xorg bochs clang
# optional
# sudo yay -S bochs-gdb gdb bear
```

## build
> ensure all necessary dependencies installed

To know more about the build, you can `make` in the follow procedure:
```bash
# prepare a master disk image to store bootloaders and kernel
make master-hd60M.img

# prepare another disk image, and partitioning it (in mbr format)
make slave-hd80.img

# build an elf kernel, raw loader binary, and a mbr (512B)
make kernel.elf loader.bin mbr.bin
# load all these binary into the master disk
make boot-master

# load user process into master disk
# the kernel will load them into fs
make userelf

# run the os in bochs
make bochs
```

Trick: force rebuild
```sh
make -B slave-hd80.img
# generate lsp info
bear -- make -B
```

## TODO
* [ ] port to qemu
* [ ] built-in editor
* [ ] shell readline support
* [ ] built-in compiler
