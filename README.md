## features
* a x86 operating system
* round-robbin schedule
* virtual memory management
* ext2-like file system
* unix-like syscall support
* 32 bit elf
![pic](http://img.phanium.top/20230617161507.png)

> documents: [doc.md](./doc/doc.md)
> testcases: [test.md](./doc/tests.md)

## dependencies
* `bochs` (<3.0, emulator)
* `gtk2` (gui)
* `nasm` (x86 assembler)
* `clang`/`gcc`
* `gdb` (optional)
* `bochs-gdb` (optional)
* `bear` (optional)

## build
> ensure all necessary dependencies installed

To know more about the build, you can `make` in the follow procedure:
```sh
# master disk image for bootloaders and kernel
make boot.img

# slave disk image for fs (partitioning in mbr format)
make fs.img

# load user process into master disk
# the kernel will load them into fs
make userelf

# run
make

# rebuild && run
make -B

# generate lsp info
bear -- make -B
```

## TODO
* [ ] port to qemu
* [ ] built-in editor
* [ ] shell readline support
* [ ] built-in compiler

## credit
* https://www.nasm.us
