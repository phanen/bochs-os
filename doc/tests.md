### protection mode
我们希望加载 os 的 kernel, 但 mbr 大小有限
- 因此先从磁盘加载一个 loader, 然后转移执行流

把 loader 加载到 0x900
![跳转到 0x900](http://img.phanium.top/20230410151020.png)
可以看到, 运行一个 invalid 地址会触发某种奇怪的东西, 跳转到 iret, 左右横跳

进入保护模式后, 看看段地址啥样
![进入保护模式后的段地址](http://img.phanium.top/20230411163115.png)

### ards

获取 内存大小和 ARDS, 代码如下
```
total_mem_bytes dd 0
gdt_ptr dw GDT_LIMIT
dd GDT_BASE
ards_buf times 244 db 0
ards_nr dw 0
```
![total_mem_bytes + gdt_base + gdt_limit](http://img.phanium.top/20230411164013.png)
- 内存大小是 0x02000000 B = 32 MB
- GDT_LIMIT 是 0x001f (+1 = 0x20 = 32 字节 = 4 项)
- GDT_BASE 是 0x900, 就是 loader 的所在
    - 事先, 把 loader 的 jmp 删掉, 直接从 mbr 跳到 loader 的代码部分
- 然后是一堆 ARDS
    - 一共有六个
    ![6 个 ARDS](http://img.phanium.top/20230411170349.png)

vim 简单处理一下
```
0x00000000 0x00000000 0x0009f000 0x00000000 0x00000001
0x0009f000 0x00000000 0x00001000 0x00000000 0x00000002
0x000e8000 0x00000000 0x00018000 0x00000000 0x00000002
0x00100000 0x00000000 0x01ef0000 0x00000000 0x00000001
0x01ff0000 0x00000000 0x00010000 0x00000000 0x00000003
0xfffc0000 0x00000000 0x00040000 0x00000000 0x00000002
```
- type 分别为 1 2 2 1 3 2
    - 2 3 分别为 ARR 和 未定义, 貌似都不能用
    - 不过写逻辑的时候, 直接不管 type
- 第二三列, high 都是 0 (这也是当然么? 毕竟 32 位 机器)
- 我们保证内存是 <4G 就好

获取内存容量的部分逻辑如下, 只管找最大的内存
ebx 开始指向开头, 取 BaseAddrLow, LengthLow, 然后便利下一个
- 只要我们保证内存是绝对不会超过 4G 的, 那这样的逻辑就没问题
```asm
mov eax, [ebx]
add eax, [ebx+8]
add ebx, 20
cmp edx, eax
jge .next_ards
mov edx, eax
```

0x00000000 - .+0x0009f000
0x0009f000 - .+0x00001000 = 0x000a0000
断层
0x000e8000 - .+0x00018000
0x00100000 - .+0x01ef0000
0x01ff0000 - .+0x00010000
断层
0xfffc0000 - .+0x00040000

注意, 这里算的是 base + len, 是算的最大延伸的地方... 单纯就是看看你的所有的地址空间占多大, 而不是关心你的这个内存是不是 rom 映射来之类的

### paging

开启分页步骤
- 设置 PD, PT
- 备份 gdtr 的值到内存
- 修改一些 paddr0 到能将它映射到 paddr0 的 vaddr0
    - 实际上, 我们要修改那些分页前如果不 explicit 修改分页后就会出问题的地方,
    - 也就是修改段表 gdt 的 entry, 将里面的老 paddr 改成新 vaddr
        - (因为第一步设置了 PD 和 PT, 这个新 vaddr 会映射到 老 paddr)
- 设置 cr3, 设置 cr0 的 PG
- 重新加载 gdtr (此时 `lgdt [xxaddr]` )
    - 段表本身是用 gdtr 访问的, 分段前 gdtr 是 paddr, 分段后要改成 vaddr
- remap
![pagetable](http://img.phanium.top/20230412102542.png)

```
cr3: 0x000000100000
0x00000000-0x000fffff -> 0x000000000000-0x0000000fffff
0xc0000000-0xc00fffff -> 0x000000000000-0x0000000fffff
0xffc00000-0xffc00fff -> 0x000000101000-0x000000101fff
0xfff00000-0xffffefff -> 0x000000101000-0x0000001fffff
0xfffff000-0xffffffff -> 0x000000100000-0x000000100fff
```

### IDT

进入内核, 开中断: 初始化内存中的 IDT 表和 PIC 设备
![idt-pic](http://img.phanium.top/20230505163226.png)

使用将 IDT 指向汇编仅作为 entry, 跳转到 C 去执行具体的处理函数:
![idt-jmp2c](http://img.phanium.top/20230506104457.png)

如何观察中断发生的具体行为? 作者的范式:
- 首先定位到中断大概位置
    ```
    b 0x1500
    show int
    s 70000
    ```
- 观察到时钟中断发生, 然后在相应的指令数附近设下断点重新调试即可
![debug-intr1](http://img.phanium.top/20230506110306.png)
- 后续调试: sba 设置指令数断点后, c 执行过去, r , print-stack
    - 大写的 eflags.IF 表示开中断
    ![debug-intr2](http://img.phanium.top/20230506112259.png)
- 再执行一步, jmp 会在栈中压入 16 字节
![debug-intr3](http://img.phanium.top/20230506113236.png)
    ```asm
    intr%1entry:
    %2 ; push a dummy or not, depend on if error code exixt
    push ds ; 执行到这里
    ; ...
    ```
- 后续压栈: ds es fs gs pushad
![debug-intr3](http://img.phanium.top/20230506113957.png)

### clock

没啥好说的, 但值得注意的是, bochs 死循环跑的时候, host 的 cpu 也会跑满

### assert
![test-assert](http://img.phanium.top/20230513164724.png)

### memory

![memory](http://img.phanium.top/20230515173026.png)
简析一下这些数值:
- paddr 部分
    - 管理 kernel pmem 的 bitmap 的 vaddr: 0xc009a000
    - physical mem 除去 1M 起始, 以及随后的 PD, PT, 254PT, 恰好到达 2M, 因此 kernel pmem pool 的起始 paddr: 0x200000
    - 剩余空间 32-2=30M, 对半分是 15M, 因此 kernel pmem pool 的终止 paddr 为 0x1100000 = 2M + 15M
    - kernel pmem 的 bitmap 的大小是 15M / 4K / 8 = 32 * 15 = 480 = 0x1e0
    - user pmem 紧随 kernel pmem 其后, 大小也是 15M
    - 类似, user bitmap 也紧随 kernel pmem 的 bitmap 其后
- vaddr 部分 (kernel vmem)
    - kernel vmem 从 3G + 1M 开始(也就是 kernel heap), 大小和 kernel pmem 一致
    - 其 bitmap 在 user pmem 的 bitmap 之后

查看此时的 映射情况:
```bash
<bochs:3> info tab
cr3: 0x000000100000
0x00000000-0x000fffff -> 0x000000000000-0x0000000fffff
0x00100000-0x00102fff -> 0x000000200000-0x000000202fff # 多了的内容
0xc0000000-0xc00fffff -> 0x000000000000-0x0000000fffff
0xc0100000-0xc0102fff -> 0x000000200000-0x000000202fff # 多了的内容
0xffc00000-0xffc00fff -> 0x000000101000-0x000000101fff
0xfff00000-0xffffefff -> 0x000000101000-0x0000001fffff
0xfffff000-0xffffffff -> 0x000000100000-0x000000100fff
```
可以看到, 将 kernel heap 区开始 3 page, 映射到了 2M 开始的物理地址


### thread

姑且是创建了线程, 但不是很懂代码的逻辑
![test-thread](http://img.phanium.top/20230516102910.png)


进一步实现了 调度
![schedule](http://img.phanium.top/20230516200206.png)
- main 初始化为主线程, 优先级 main = a > b
- RR 调度, 优先级即是 时间片

问题: 通用保护性异常

debug
- show int, 尽量在离发生 GP 异常近 一点的地方用
    - show extdnt 只显示外部中断
    - nm 查找符号具体位置
- 得到 中断发生的 时间点 e.g. 00159442234
- sba 159442233
![debug](http://img.phanium.top/20230516204423.png)

结果是 gs 段访问越界了, 但滚屏的设计是 bx 至多到 4000, 很奇怪

### I/O

抽象 console, 以及实现出 thread-safe 的 print 就不单独演示了

keyboard handler 比较有意思
![](http://img.phanium.top/20230517151726.png)
- 写 handler 接收并打印 8042 输出寄存器里的 scancode (8042 一律转为 set 1)
- 最后三个依次是 lctrl rctrl prtsc


进一步优化驱动, 能完成基本键盘输入
![](http://img.phanium.top/20230517170341.png)
- 现在的问题是 按住 实际上会发送 多次 makecode 事件?
- 可能因为 bochs 是遵循了 X 键盘协议


### ring3 tss

运行 tss_init 的时候出问题:
![ud-exception](http://img.phanium.top/20230518204014.png)

反汇编发现是编译出了 SIMD 指令...
![xmm](http://img.phanium.top/20230518203728.png)
```c
*((struct gdt_desc*)0xc0000920) = make_gdt_desc((uint32_t*)&tss, tss_size - 1, TSS_ATTR_LOW, TSS_ATTR_HIGH);
```

解决: 加禁 SIMD 的编译选项
```bash
-fno-tree-vectorize # 这个不行, 不知道是干啥的
-mno-sse # 这个 work
```
![tss](http://img.phanium.top/20230522201154.png)

### user prog

用函数模拟实现一个进程
- 有独立地址空间(页表), 能调度
![prog](http://img.phanium.top/20230523091157.png)

为什么递增不连续
为啥能和原来的进程共享 bss?

### syscall

获取 pid
![pid](http://img.phanium.top/20230524151321.png)
- 别问为啥线程也有 pid, 作者乐意 ...
- 0x1 分给了 main, main 是 init 阶段就创建了

实现 write 和 printf
![printf](http://img.phanium.top/20230524223807.png)
- kernel 可直接使用封装 console_put_str 的 sys_write
- 基于 write 封装一个 printf, 解耦
    - vsprintf 负责格式化部分
    - 打印部分由 syscall write 负责

kernel malloc
![](http://img.phanium.top/20230525004446.png)
- main 首先运行, 分配一个 64 的 arena
    - arena head 在 0xc0134000
    - 然后第一个 block 位于 0xc013400c
- 调度到 thread a, 重新分配 32 的 arena
    - arena head 在 0xc0135000
    - 然后第一个 block 位于 0xc013500c
- 调度到 thread b, 以及有没满的 64 的 arena
    - 直接分配到第二个 block 0xc013404c

kernel 的 malloc 和 sys_free
- 在运行 thread_c 前, bitmap 位 0x7, 表面申请了 三个 page
    - 分别为三个内核线程的: main thread_c thread_d
- 之后运行过程中不断查看, 可以看到 bitmap 在动态变化
![](http://img.phanium.top/20230525104034.png)

ring3 下使用 syscall 进行 malloc 和 free
![](http://img.phanium.top/20230525105220.png)
```c
void* addr1 = malloc(256);
void* addr2 = malloc(255);
void* addr3 = malloc(254);
printf(" prog_a malloc addr:0x%x,0x%x,0x%x\n", (int)addr1, (int)addr2, (int)addr3);
free(addr1);
free(addr2);
free(addr3);
```

### partition


![](http://img.phanium.top/20230525160505.png)
- 上来用 u 来 toggle unit 了
- 虽然但是, 为啥每个区都会少一些 cyliner...


用 sfdisk script store/load 分区信息
```bash
label: dos
label-id: 0x9fa5ad85
device: hd80M.img
unit: sectors
sector-size: 512

hd80M.img1 : start=        2048, size=       30208, type=83
hd80M.img4 : start=       32256, size=      131040, type=5
hd80M.img5 : start=       34304, size=       16096, type=66
hd80M.img6 : start=       52448, size=       23152, type=66
hd80M.img7 : start=       77648, size=       13072, type=66
hd80M.img8 : start=       93744, size=       27216, type=66
hd80M.img9 : start=      123984, size=       39312, type=66
```
![part](http://img.phanium.top/20230526102431.png)
> 这个 S 是因为 收集 partition tag 的 list 忘了初始化, 导致把野指针的内容也反射出来了

### fs

格式化
<!-- ![formating](http://img.phanium.top/20230526153508.png) -->
![sys-open](http://img.phanium.top/20230528200006.png)
![format-over](http://img.phanium.top/20230526153345.png)
- 挂载第 1 个 分区, 其 root_dir_lba 为 0xa6b
- 用 hexdump 查看偏移, 0xa6b x 512 = 1365504 (0x14d600)
![rootdir-hexdump](http://img.phanium.top/20230528202043.png)
```hexdump
014d600 002e 0000 0000 0000 0000 0000 0000 0000
014d610 0000 0000 0002 0000 2e2e 0000 0000 0000
014d620 0000 0000 0000 0000 0000 0000 0002 0000
014d630 0000 0000 0000 0000 0000 0000 0000 0000
```
- 16 字节的文件名: 0x2e 为 '.' 的 ascii
- 中间 4 字节 `inode`: 0
- 最后 4 字节 `f_type`: 2 为 目录类型 `FT_DIRECTORY`
- 第二个目录项: ('..', 0, 2) 

下面测试部分 fs 的 API

`sys_open`
```c
sys_open("/file1", O_CREAT);
```
刚才位置的 hexdump 如下, 进一步找到创建的文件位置:
```
014d600 002e 0000 0000 0000 0000 0000 0000 0000
014d610 0000 0000 0002 0000 2e2e 0000 0000 0000
014d620 0000 0000 0000 0000 0000 0000 0002 0000
014d630 6966 656c 0031 0000 0000 0000 0000 0000
014d640 0001 0000 0001 0000 0000 0000 0000 0000
014d650 0000 0000 0000 0000 0000 0000 0000 0000
```
- 第三项多了个 inode 号为 1 的 regular file, ASCII 码为 'file' (66 69 6c 65)

`sys_close`

![open-close](http://img.phanium.top/20230529162615.png)

`sys_write`
![write](http://img.phanium.top/20230530091216.png)
- 文件 block 的位置: 0xa6c = 2668 = 0x14d800 bytes
![helloworld](http://img.phanium.top/20230530092226.png)
- 而 inode_table 的位置: 0x0000x80b = 2059 = 1054208 bytes = 0x101600
![inode](http://img.phanium.top/20230530092953.png)  
- 该项从 # 开始, 可以看到里面有个 'a6c' 指针
```
0101640 0000 0000 0000 0000 0000 0000#0001 0000
0101650 0024 0000 0000 0000 0000 0000 0a6c 0000
0101660 0000 0000 0000 0000 0000 0000 0000 0000
```

`sys_unlink`
- 简单来说就是只擦除 bitmap, 然后同步
![erase](http://img.phanium.top/20230530171600.png) 
<!-- ![erase](http://img.phanium.top/20230530170735.png) -->
- 从上到下(0x100400开始): inode bitmap, inode table, root dir, block (block bitmap 太远不放了)
    - inode 的 bitmap 只剩 1 bit: rootdir
    - rootdir 里也没有 entry 了
    - inode table 里的东西也不在了 (不过这里单纯是写代码多此一举, 其实可以不擦除, inode 和 block 都可以残留, 两个 hello, world 的内容还在

`sys_mkdir`
![mkdir](http://img.phanium.top/20230530182328.png)

![bitmap-and-inode-table](http://img.phanium.top/20230530192850.png)
- bitmap: 均为 0x1f, 目前每个 inode 都只有一块
- inode-table 一共五项, 人工 parse 如下
    - `i_no`: 1 2 3 4 5
    - `i_size`: 0x60(rootdir: ., .., file1, dir1), 0x18(file1: 两个"hello,world\nhello,world\n"), 0x48(dir1: ., .., subdir1), 0x48(dir2: ., .., file2), 0x60(file2: "fucking hungry!!!\n")
        - 可见目录大小是每项 0x18
        - 同时一个值得注意的问题, 这里 c-str 其实没有把尾部的 '0' 写进去(因为测试用例只传入 count)
    - `i_sectors[0]`: 0x0a6b 0x0a6c 0x0a6d 0x0a6e 0x0a6f

![dir-and-file](http://img.phanium.top/20230530194916.png)
- 目录和文件: root, file1, dir1, subdir1, file2
- fucing hungry!!! 后面的奇怪字节, 是因为 write 的 count 没设好, 过读了 stack buf 里面的内容, 一并复制进去了
    - 栈总是容易不干净的...

`sys_rmdir`
![rmdir](http://img.phanium.top/20230530213851.png)

`sys_cwd` `sys_chdir`
![cwd-chdir](http://img.phanium.top/20230530224236.png)

`sys_stat`
![stat](http://img.phanium.top/20230530225411.png)
- 好吧, helloworld 的 代言 file1 刚刚被删掉了

### userland

fork 和 init
![init-fork](http://img.phanium.top/20230531122905.png)
- NOTE: ide_init 前要开中断, 隐藏的 bug

tokenizer 和 path parser
![tokenizer-path-parser](http://img.phanium.top/20230531212520.png)

shell builtin
![shell](http://img.phanium.top/20230601144432.png)

shell external
![ex](http://img.phanium.top/20230601151819.png)
![ex](http://img.phanium.top/20230601151832.png)
![ex](http://img.phanium.top/20230601151906.png)
- 第一个问题是, 现在没有可以加载的 elf 文件
    - 现在, 要么人肉 sys_write 写 binary (当然不可能), 或者 dd 进裸盘, 然后由 kernel 搬进 fs 盘
- 第二个问题是, fork 之后的父进程成为 loop 僵尸, 需要设计一个 exit


加载 elf 文件
![load-elf](http://img.phanium.top/20230601173124.png)

执行 elf 文件
![exec-elf](http://img.phanium.top/20230602150818.png)

<!-- crt: -->
<!-- ![crt](http://img.phanium.top/20230602151544.png) -->

实现 exit, 构建简单的 crt, 实现 wait, 从而可以连续加载执行用户进程
![cat](http://img.phanium.top/20230602223611.png)

