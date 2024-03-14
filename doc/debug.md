# debug

## bochs 调试
- set size
- s n/p
- breakpoint
    - 地址: vb lb pb
    - 跳过特定指令数: sb
    - 时间戳: sba
    - IO: watch r w, watch, unwatch
    - blist (info b)
    - d bpd bpe
- CPU and memory contents
    - x xp, /2 /h /w /g, /x /d
    - show mode int call
    - setpmem
    - ptime print-stack
    - info cpu, fpu, idt, gdt, ldt, dss
    - info flags, sreg, dreg, creg
    - info tab, page

TODO
- 如何判断汇编代码某处的地址
- 猜吧: u /100

进入保护模式后, 不加 bits 32 有什么后果
- 指令会识别成乱七八糟的
- 此时处理器只认识 32 位指令, 而编译器按照 16 位编译出来, 当然乱七八糟

字符打印问题: 单步调试 s, 屏幕不会变化, 用 n 屏幕会能逐步闪烁

## 编译链接
```bash
gcc -c main.c
ld main.o
# ld: warning: cannot find entry symbol _start; defaulting to 0000000000401000

gcc -c main.c
ld main.o -e main
objdump -d a.out

gcc -c _start.c
ld _start.o
objdump -d a.out

gcc main.c
objdump -d a.out
```


## minimize
尝试 cc -static -nostdlib, 根本编译不出跟作者一样 minimize 的 a.out
- 首先是总会产生 .got.plt .eh_frame 之类的东西
    <https://reverseengineering.stackexchange.com/questions/2172/why-are-got-and-plt-still-present-in-linux-static-stripped-binaries>
- 然后似乎十年前, gcc 的 -static 就已经死了
    <https://stackoverflow.com/questions/3430400/linux-static-linking-is-dead#comment101507244_56771892>
- https://www.zhihu.com/question/22940048
- 或许可以 strip
    <https://stackoverflow.com/questions/64949042/how-do-i-compile-c-without-anything-but-my-code-in-the-binary>
- 用万能的 aur 装了个 gcc46 (虽然没 chroot...)

## faq


如何扩容 loader
- 不仅要增大 mbr 加载的空间
- 还要增大 dd 装到 hd60M.img 的大小
    - 否则 loader.bin 的后半部分会卡在外面, 运行的时候会有奇怪的错误

为什么要将 3G 和 0-1M 都映射到 0-1M
- ........

一级页表如何存在内存里, 供 OS 访问
- 本身要在物理地址中占用 4M (1K 个 4K 页)
- 如果 OS 要访问, 那就得拿到 虚拟地址中, 也要用 (1K 个 4K 页)
    - PT 要记录 1K 个项目指向自己 (自指也不总是优雅的)

二级页表要如何供 OS 访问
- 本身要在物理地址中占用 4K ~ 4M+4K (1K 个 4K 页)
- 如果 OS 要访问, 有两种策略
    - 普通人路线: PD 至少分配一个 PDE 指向特殊的 PT, 这个 PT 至少分配一个 PET 来指向自己, 所以物理内存至少 8K(PT + PD), 但虚拟内存最少只要 4K, 分配的 PET 的剩下 4M-4K 逻辑上还是能随便用
    - 魔法路线: PD 取一项完整的 PDE, 指向自己 (PD 当作 PT), 物理内存至少 4K(PD), 但虚拟内存至少 4M
- 那么二级页表机制, 我要访问 PT, 而不是 PD, 如何操作
    - 普通人路线就得再搞二级映射的傻逼范式, 而魔法路线早就解决这个问题了
