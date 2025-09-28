## features
* a x86 operating system
* round-robbin schedule
* virtual memory management
* ext2-like file system
* unix-like syscall support
* 32 bit elf
![pic](http://img.phanium.top/20230617161507.png)

## dependencies
* `bochs`/`qemu`
* `nasm`
* `clang`/`gcc`
* (optional: `gdb`/`bochs-gdb`/`bear`)

## build
```sh
# run in bochs
make

# run in qemu
make qemu

# rebuild && run
make -B

# generate lsp info
bear -- make -B
```

more documents
* [doc.md](./doc/doc.md)
* [test.md](./doc/tests.md)

## TODO
* [x] port to qemu
* [ ] built-in editor
* [ ] shell readline support
* [ ] built-in compiler

## credit
* https://github.com/yifengyou/os-elephant
* https://www.nasm.us
* https://wiki.osdev.org
* https://cdrdv2-public.intel.com/774493/325384-sdm-vol-3abcd.pdf
