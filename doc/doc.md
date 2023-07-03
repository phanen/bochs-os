## basics

硬件接口
- MMIO: 地址映射后, 访存
- PIO: 使用 IO 接口 (访问寄存器)

内存的分段
- 目的: 地址隔离
- 早期没有虚拟地址的概念, 程序编译出来都是死的物理地址
- 要支持 多道程序, 必须将内存分成段, 不同的程序在不同段
- 具体实现: 重定位 + 段寄存器

程序的分段 (代码段/数据段..)
- 让逻辑更清晰, 这个意义上分段不是必要的
- 代码和数据分离还有很多好处
    - 设置属性(访问权限)/ 提高 cache 命中/ 节省内存(共享代码段)
- 属性是如何设置并生效的
    - 编译器编译成一系列段(elf) (高级语言自动分段, 汇编要手动分段)
    - OS 准备 GDT, CPU 用 段选择子访问
    - 还有 load 负责把 程序的段 加载到 内存中?
- section, segment (多个 section)

内存模式
- 平坦模式: 所有内存可用单独的一个地址访问, 或者说将所有内存分为一段
- 分段模式: 段(基)地址 + 偏移地址
- <https://zhuanlan.zhihu.com/p/136292913>
- <https://www.cnblogs.com/johnnyflute/p/3564894.html>

BIOS 中断
- 事件来源: CPU 内是异常, 外设是中断
- BIOS 中断: 调用中断向量表(IVT), 1024 字节/ 4 字节每项
    - 每项是一个地址, 指向一个 中断例程
    - IVT 指向的例程由 BIOS 添加
    - 目的: 提供硬件的访问方法, 简化硬件操作
- 没有例程, 也可以访问硬件: 地址映射/端口操作
    - 本质: 访问外设硬件自身的 ROM
    - ROM 的内容: 55 aa nhk ...
- IVT 是谁添加的? CPU 原生支持
- 中断描述符表

DOS 中断
- DOS 本身运行在实模式下, 可以调用 BIOS 的中断
- 也会建立一个自身的中断 0x21

Linux 中断
- 在保护模式下运行中断
- 不访问 IVT(保护模式下不存在), 而访问 IDT (中断描述符表)
- int 0x80 + exa 来确定子功能函数

section segment
- 汇编语境下: 两者一个意思, 逻辑区域, 汇编器的关键字, 编译出的结果是 elf 的 section
- OS 不关心 section 的数量, 只关心属性, 将同属性的节 merge 到一起, 称为 segment
- segment 也就是内存中所谓的代码段, 数据段
- section -> segment 是由 linker 完成的

magic number
- 7f 45 4c 46 01 01 01 00, 00 00 00 00 00 00 00 00
- 0x55 0xaa: 可加载/可引导
- 文件系统识别, 一般位于本分区的第 2 个扇区 (1 号扇区的 superblock)
- 本质: 不过是一种简单的约定/协议

cs, ip 为什么不能 mov
- case: cs 改了, ip 没改, 寄
- jmp call int ret 同时修改 cs 和 ip, 保证硬件级别的原子性

库函数
- 操作系统: 提供系统调用, 隔离硬件接口
- 库函数: 进一步封装, 并提供运行时库
- 用户程序: 链接运行时库, 使用库函数
    - 用户也可以直接"系统调用"

函数为什么要声明
- 函数返回值类型, 参数类型及个数, 用来确定分配的栈空间
- 分解出 链接的步骤, 现在无法得知地址, 由链接步骤完成

MBR -> OBR
- 分区时, 确定活动分区(约定: 分区表项标记 0x80)
- OBR: 内核 loader 的入口的所在
    - loader 入口再 jmp 到 loader
- MBR OBR DBR EBR 都包含引导程序, 都称为引导扇区
    - DBR 是 OBR 前身, DOS 只有 4 个主分区, EBR 是兼容
    - 分区表只在 MBR 和 EBR 中存在, DBR 或 OBR 中绝对没有分区表

MBR OBR DBR EBR
- 若该扇区位于整个硬盘最开始的扇区, 并且以 0x55 和 0xaa 结束, BIOS 就认为该扇区中存在 MBR
- 若该扇区位于各分区最开始的扇区, 并且以 0x55 和 0xaa 结束, BIOS 就认为该扇区中有操作系统引导程序 OBR
![brbr](http://img.phanium.top/20230407223005.png)

## MBR, 实模式

cpu
- 控制单元: 操作控制器, 指令寄存器, 指令译码器
- 存储单元 运算单元

寄存器
- 可见: 通用寄存器 GPR 段寄存器 SR
- 不可见: GDTR IDTR LDTR TR CR DR IP flags
- 通用寄存器的专用 (约定接口)
    - accumulator base count data
    - source dest
    - stack/base pointer ...

8086 之前: 设计上没提供方便重定位的手段

什么是寻址
- 首先明确什么是寻址, 消除一个误会, 寻址不是说给一个地址, 然后去访问内存, 寻址不是访存, 寻址不是得到内存的地址
- 寻址是得到操作数的过程
- 重申: 寻址 = 得到操作数, 寻址 != 拿到"地址"
- 建议改名为 寻操作数

寻址的方式
- 寄存器 mul dx
- 立即数 mov ax, 0x01
- 内存 mov ax, [0x1234]

内存寻址可以继续再分
- 直接寻址 立即数作为访存地址
    - mov ax, [0x1234]
- 基址寻址 寄存器作为起始地址
    - 实模式下只能用 bx bp
    - 保护模式下就没这个限制(GPR)
    - mov ax, [bx]
- 变址寻址 (实际上和基址寻址一样)
    - mov [di], ax
- 基址变址寻址

函数调用与 ebp
![ebp](http://img.phanium.top/20230408164233.png)
- caller: 压 arg -> 压 eip -> jmp
- callee: 压 ebp -> 存 esp -> 执行(local) -> 取 esp -> 弹 ebp -> ret
- caller: 弹 arg

### 函数调用

我们往往先从概念上认识函数, 他作为一种强大的编程范式而存在
- 我们可以将万事万物的程序给刻画成一颗树, 然后用函数调用去描述这颗树, 剩下只要交给计算机去完成, 而不需要思考函数本身原理, 只要函数是 work 的, 就能使用他去完成编程任务, 这正是抽象的魅力

计算机要求我们具有两种思想, 前面是不管不顾的抽象思想, 极大地降低人的心智负担, 也要有刨根问底的抽象思想, 看到一个巧妙的接口, 同样希望从实现原理上去思考他
- 我们知道, 即使在汇编/机器码层面, 也是有函数的概念的 (call + ret), 那么反过来, 给定计算资源的约束, 如何把实现函数的范式实现出来呢, 这个问题用建模的语言来描述, 也就是
- 给定, 熟悉冯诺依曼架构的你
- 一组对象: CPU, 有限的 REG, 和有限的 MEM
- 一组对象之上的 API: 寻址, 计算, 跳转...
- 我们如何来尽可能好地, 实现函数的抽象呢?

要点就是: 反思对函数的抽象, 用尽可能 minimize 的规则描述函数是什么, 函数能做什么
- 函数是什么: 有独立状态(局部变量)的执行过程, 执行结束前, 资源不能释放
- 函数做什么: 后执行的 callee 一定比 caller 先结束
- 最贴切描述的自然是 LIFO 的数据结构: 栈

启发: 把函数的状态(变量/寄存器)用栈来存储
- 每个函数的所有状态存放到栈表的每一项中
- callee 的状态, 将 append 到 caller 后面
- callee 先退出, 那么可以 dis-append

第一个问题: append 和 dis-append 是如何做的
- 只考虑数据的话, 这个很简单, 我们对数据维护栈指针, 在函数之间转移就挪动栈指针
- 但问题在于, 函数之间是如何转移的呢, caller 到 callee 可以通过简单的 jmp 实现, 但 callee 到 caller 就得有额外的信息

所以接下来, jmp 到一个新的函数之后要怎么 jmp 回去呢
- 答案是: 栈上的数据, 应该也要包括执行流的信息
- 也就是我们不仅记录纯数据, 也要记录返回的地址
    - 这个地址属于谁的状态信息呢: 当然是 caller
- 比如, 我们可以让 caller 在调用时, 总将返回地址放到自己要保存的所有数据之后, 这样 callee 执行完后, 清除掉自己所有的数据, 栈顶就是返回地址的信息, 就可以 jmp 回到原来的函数
    - 一切皆数据, 只要能抽象成数据的状态, 我们都能用栈来维护

但我们无形之中又使用了抽象, 隐式地假设了 callee 能够自动完成清理数据的任务
- 应该说明白 append 和 dis-append 是如何实现的
- 但这是显然的, 因为压栈用 push, 弹栈相应当然用 pop, 你来的时候用多少资源, 走的时候就释放多少就是了
- 嗯, 这确实是一种解决方案, 汇编我们完全可以采用这套范式(但这样还有 ebp 什么事呢


如果你还不明白?
- 移动指针当然很简单, 但一个函数可能有很多变量, 我们是否能知道他的状态到底要占据多大的空间
    - 答案: 不是能不能知道, 而是必须知道, 当然知道
- 必须知道: 我们要设计一种汇编的范式去实现函数的概念, 函数的状态大小从定义上是确定的, 因为你要用汇编完全实现函数的状态转移, 所以函数栈帧的大小要求必须是静态确定的, 你不是在运行时去确定他
- 当然知道: 写汇编的时候, 每个 push 都对应一个 pop, 所以大小这东西, 不言也可

### 栈帧, ebp

ebp 是否是必要的: 并不是
- 考虑一个模型是否是最简的, 只需要尝试删掉这个模型的组件, 看看是否能重新设计能达到逻辑上的自洽
- 而汇编的模型未必采用逻辑上最简的设计, 而是权衡, 性能效率成本等等, 要从应用上取向, 这不仅仅是数学
- ebp 的本质: 记忆函数执行前 esp 的位置, 也就是上次栈的状态, 从这个意义上, 如果没有 ebp, 就没法恢复栈的状态么?
    - 并不, 如果我们遵循更简单的范式, 在汇编上设计: 每个 push 都对应一个 pop. 也能回到 esp 原来的地方

那么 ebp 是用来提高效率的么?

直观上想一下如何提高效率
- push 和 pop 是有本质区别的, push 是分配局部变量, 要一个一个来
    - 实际上 push 可能长成别的样子: sub + store
- 而 pop 的时候, 如果这个值不用还原, 那么直接丢掉就好了, 或者说我们批量丢掉, 让 sp 直接减去一个值
    - (我现在先不考虑保存寄存器状态这一说)
- 那么 sp 减去的这个值从何而来: 当然是从 push 的数量上来, 从你之前写过的 code 你就知道要 pop 多少
- 从这个角度, ebp 可能也不是必要的... 甚至"教科书"地使用 ebp, 你还要多去访存

那么 ebp 到底有啥用呢.....
- 我开始以为作者随口说 ebp 是为了方便访问栈空间是偏见, 想来想去感觉非常有道理...
    - ebp 维护当前栈的底部, esp 来维护栈的顶部
    - ebp 下面是函数的 local, 上面是 ret, args, 和上一个栈帧?
- 因为没有必要性呢...
    - 汇编的时候完全可以确定 sp 移动多少
    - 考虑高级语言编译过程, 根据变量的多少, 每个函数栈的空间也是确定大小的, 所以也能把 sp 移动的量给确定...
- ebp 也没有相对单独 esp 的性能有时


参考 google
- [比较好的答案](https://stackoverflow.com/questions/3699283/what-is-stack-frame-in-assembly#answer-3700219)
- [关于 stack frame](https://stackoverflow.com/questions/3699283/what-is-stack-frame-in-assembly#answer-3700219)

结论是, 看来和我想的差不多, 确实不必要, 价值主要在于
- 方便调试, 只要看 ebp 就能确定栈从哪里开始
- C99 支持变长数组, 比较难确定栈大小, 这种 ebp 范式比较实用
- 也可以将 ebp 用于一般用途 `gcc -fomit-frame-pointer`


C99 变长数组 和 alloca
- 大小可以是变量, 也就是运行时确定大小, 且分配在栈上
- 有点逆天, 但这是可能实现的
- 我对实现方式的理解:
    - sp 加减的值都得是动态维护的
    - 如果有 ebp, sp 减去一个动态寄存器的值, 后面返回好说
    - 如果没有 ebp, 让 sp 减完数组的动态大小 size 后, 这个大小后面还是要维护的
- [可参考](https://blog.51cto.com/e21105834/2952198)


说了半天, bp 的全名叫啥呢
- stack-base pointer


我们进一步深入思考一下 ebp 的本质
- 现在忘掉类似 push 再 pop 的手段, 或者简单粗暴地约束一下, 我们不允许直接或间接在 sp 上做加法, 在这种约束下, 那么怎么用汇编范式来实现函数, 剩下的办法就是 ebp
- 本质在于: 我们将上一个函数栈帧的起点位置(局部变量的起点, 严格来说是上上函数压入 args 和 ret 后的位置), 也作为当前栈的状态来维护, 同时用一个寄存器 ebp 来记录最尾端的栈的起点
- 这么做的结果是: 我们得到了一个栈帧的链表, 通过最后一个 ebp, 我们能逐步回退到起点
- 链表增强的栈, 删除要快? 那直接减去 sp 不就完了, 还用着去访存?

<!-- 问题在于我们要让这个过程对所有的函数都统一地自动生效, 如果每个函数都有自己独立的计数器/起始栈指针, 这样 reg 里面肯定就装不下 -->
<!-- - 所以答案很简单, 计数器/起始地址本身也是函数的状态, 我们也将他存储到函数的栈帧里面, 假设 A -> B -> C, 把栈帧串起来, 就长这样: -->
<!--     - 元调用者的数据+A返回地址+A数据+A栈大小+B返回地址+B数据+B的栈大小+C返回地址+C数据+C栈大小 -->
<!-- 说实话, 我开始想把栈帧用括号括起来, 但是 B 的 ret 该算在 A 里还是 B 里, 有很大的歧义性, 或许都有道理... -->

<!-- 为什么一定是栈?  -->
<!-- - 我们当然可以思考用别的模型来实现函数, 比如我不喜欢栈, 我喜欢 hash table, 喜欢 -->

### call ret jmp


16 位实模式 call
- 相对 近 call
```
call near func_name
```
- 间接 绝对近 call
```
call [addr]
```
- 直接 绝对远 call
```
call (far) 0:far_proc
```
- 间接 绝对远 call
```
call far [addr]
```

直接/间接 近/远 相对/绝对
- 直接是操作数来自直接寻址的意思, 间接就是非直接寻址
    - 相对近调用没有"直接"的 prefix, 是因为虽然有直接的立即数, 但访存还是要 完整的操作数
- near 和 far 单纯作为修饰使用, 和 word dword 是同一个语义
    - near 默认是有符号(修饰相对调用), 用在相对调用的语境下, 因此直接用来 `call near [addr]` 会 warning
    - ret retf


16 位实模式 jmp
- relative short jmp (1 字节)
    ```
    jmp $
    jmp short short_fuc
    ; 可能 warning
    ```
- relative near jmp
    ```
    jmp near start_func
    ```
- indirective relative short jmp
    ```
    jmp near ax
    jmp near [ax]
    ```
- directive absolute far jmp
    ```
    jmp far [addr]
    ```


### flags

8086+
- carry flag
- parity flag
- auxiliary carry flag
- zero flag
- sign flag
- type flag
    - debug, 让 cpu 单步运行
- interrupt flag
    - 屏蔽外部中断, 但不屏蔽 cpu 内部异常
- direction flag
    - 字符串操作相关
- overflow flag

80286+
- input output privilege level
- nest task (8088)

80386+
- resume flag
- virtual 8086 mode (向保护模式过渡的产物)


80486+
- alignment check


80586+
- virtual interrupt flag
- virtual interrupt pending
- identification


### condition jmp

jz/je jne/jne
js jns
jo jno
jp/jpe
jnp/jpo

jcxz

above
below
carry
equal
great
jmp
less
not
overflow
parity

### IO 接口

任何不兼容的问题, 都可以加一层来解决

IO 接口, 是连接 CPU 和 外设的, 逻辑控制部件
- 就是我们所说的驱动? 分为硬件和软件
- 硬件就是一些芯片: 比如声卡, 显卡, 大多不用专门买, 集成在主板里, 分别驱动音响和显示器
- 有无软件, 意味着能否编程: 可编程/不可编程 接口芯片
- IO 接口有限, 能让多个外设通过一个 IO 接口芯片与 处理器连接
- IO 接口控制编程: in/out

先说硬件, 其实就有三类角色
- cpu 和 外设两方, 各自只要实现好自己的功能
- IO 则在中间扮演外设的桥梁, 来解决一切兼容问题, 通过总线(bus)分别与外设相连

另一个问题, 当很多外设都想和 CPU 通信, 如何协调好顺序呢
- 可以再分一层, 让仲裁 IO 接口来做
- 这就是南桥芯片, ICH, I/O control hub
    - 主要用来连接低速设备 pci, pci-express, agp
- 南桥又不止包括仲裁的部件, hub 意思是 公共, 集合, 内部干脆就集成了很多 IO 接口: SATA, USB, PCI...

而北桥芯片一般和南桥成对出现
- 用来连接高速设备(内存)
- (这部分工作可能直接放到 CPU 内部)

![南桥](http://img.phanium.top/20230409151002.png)

再说软件
- IO 接口的设计是: 通过其内部的寄存器, 与 CPU 通信
- 这些寄存器区别于 CPU 内部寄存器, 称为端口

那么如何访问端口
- 可以内存映射, cpu 透明的通过一般的指令来访问
- 也可能独立编址, 这种情况就要使用专门的 in/out 指令来访问

IA32 给端口从 0 开始编号, 有一个端口号寄存器是16 位, 0-65535

IA 32 的范式如下
```
in al, dx
al = port[dx]

out dx, al
port[dx] = al

out 123, al
port[123] = al

// AT&T
in port, %al
in port, %ax
out %al, port
out %ax, port
```

#### 显卡 显存 显示器

实模式下, mbr 能通过 0x10 BIOS 中断打印字符
保护模式下, 中断向量表就没了, 所以没法再用 BIOS 中断了

显卡
- 核心只有 amd/nvidia(apple?), 其他厂商, 只是本地化开发, 类比 android 和 miui
- 显卡是 pci 设备
- pci 总线: 并行架构, 但一起发数据效率看似挺高, 其实有顺序问题, 发送的位宽越大越难搞
- pcie(pci express): 串行传输就能保证顺序是没问题, 传输的频率快

显存
- 显卡的内存, 显卡的工作就是不断读取这块内存, 然后发送到显示器

显示器
- 显卡可以设置 显示器的工作模式: 图形模式/字符模式...
- 显示器眼里的数据不分类型, 没有高级解析, 就是纯粹的像素信息
- 只要有了显卡, 就没必要考虑背后的显示器的细节了

显存的地址
- 这种情况就要用地址映射了
- 映射后访存的过程到底是什么样的, 我不是很清楚, 但至少不能认为是访问"内存"的地址

显卡支持的模式
- 文本/黑白图形/彩色图形
    - 各自有不同的显存地址
- 文本模式又分不同的模式
    - 列数 * 行数, 默认是 80*25=2000
    - 每个字符 2 字节
        - 亮度+ascii+属性, 每屏共 4000 字节

段超越前缀
- 对于堆栈和代码段, ss cs 是死的, 你只能用他们访问
- 但数据段的段前缀是可以显示更改的, 只是默认是 ds

#### 磁盘

历史
- 1956 IBM 350 RAMAC 5MB
- 1968 winchester 技术, 磁头 盘面不接触
- 1968 IBM 3340 2*30MB
- SSD zookeeper

原理
- C H S 架构, 柱面存取
    - 一是为了减少寻道次数
    - 二是越往里越慢, 从外面开始存
- IO 接口: 硬盘控制器
    - 显卡和显示器分开, 硬盘控制器和硬盘不分开 (但很久以前是分开的)
    - 得益于: IDE 集成设备电路, ATA 标准
    - Serial ATA 的出现, ATA 改名叫 PATA
- PATA 接口线缆: IDE 线
    - IDE 线上挂两块, 主盘(master) 和 从盘(slave)
    - 主板支持 4 块IDE 硬盘, 就有两个接口/插槽: Primary 通道, Secondary 通道, 每个通道挂两块

硬盘控制器 端口
- 一个寄存器甚至有多用的情况, 节约成本和兼容目的
- command  block register
- control block register

具体端口
- data 读写 磁盘缓冲区 的数据
- error 读取失败的信息, 写的时候叫做 feature, 存储格外的命令参数
- sector count 待读取或写入的扇区数(号)
    - 如果中间失败, 就表示尚未完成的扇区号
- LBA 寄存器, low mid high 三个 8 bit, 加上 device 的低 4 bit, 构成 LBA 28
- device 高位: LBA 是否启用, 指定主盘 or 从盘
- status/command
    - 读时, 记录硬盘状态, error, data request, drdy, bsy
    - 写时, 命令: identify, read sector, write sector...
![top](http://img.phanium.top/20230410111339.png)

操作方法呢
- 因指令而异
    - 要么直接写 command
    - 要么有额外参数要写 feature
    - 详细参考 ATA 手册
- 抽象步骤范式
    - 选择通道?, 写 sector count
    - 选择地址, 写 LBA 地址, 设置 LBA 模式
    - 输入指令, 写 command
    - 获取硬盘状态, 读 stauts
        - 如果是写硬盘, 就完工了
        - 如果是读硬盘, 如何读数据?

传送数据的方式
- 无条件传送
    - reg 或 mem 就是这样, cpu 无脑直接取
- 查询传送, 又叫程序 I/O(PIO)
    - 先检查设备状态, 再取
    - 一般是低速设备(废话, 速度都比 cpu 低)
    - 要用程序去轮询?
- 中断传送, 又叫中断 I/O
    - 直接用中断通知 CPU 取数据
    - 缺点是 CPU 要压栈保护寄存器现场
        - 保护现场 -> 传输 -> 恢复现场
    - 所以中断到底是谁中断谁... 和 BIOS 中断是啥关系
- 直接存储器存取 (DMA)
    - 更爽, 一点都不花 CPU 资源
    - 传输不用 CPU 自己来做, 也就不用保护现场了, CPU 直接访内存拿数据就行了
    - DMA 是硬件实现, 需要 DMA 控制器
    - 疑问: DMA 要保证 CPU 访存的时刻数据是准备好的...
- I/O 处理机 传送
    - 更爽, CPU 拿到数据后都不用去数据处理, 校验之类的操作
    - DMA 控制器更强大一点, 干脆这些工作也替 CPU 干了

那么 bochs 硬盘如何传输数据呢
- 1 肯定不行, 4 5 bochs 不支持仿真出 DMA 硬件?
- 那么只有 轮询 和 中断

## 保护模式

动机: 保护
- 用户和内核权限相同, 访存纯物理地址, 随便访问, 不安全
- 一次只能跑一个, (没法调度), 且内存资源很有限

我们说实模式, 是 32 位 CPU 运行在 16 位模式的状态

32 位 gpreg
16 位 sreg, 存 selector
dc reg, 描述符 cache, 也用于 cache 实模式下移位后的段地址

### 运行模式反转

都说 实模式能 "利用" 保护模式的资源, 那么怎么利用呢
- 机器码层面: CPU 的指令集专门设计了一种前缀, 用于反转, 只要机器码加上这种前缀, 一种模式下就能使用一些另一种模式的指令
- 汇编的面: 当你写汇编的时候, 你完全不用考虑前缀这回事, 汇编器会帮你加上, 但你需要声明你写的代码当前是什么模式下的, 否则汇编器不认识(默认似乎是实模式下的, 所以之前完全没考虑)
    - 声明的方法是 `[bits 16] [bits 32]`
    - 但我们之前写 mbr 完全没考虑这个东西? 可能默认他认为你是在 16 位

那么会汇编器为什么不能区分出保护模式呢
- 进入保护的方式是千奇百怪的, 比如: 先打开 A20 -> 加载 gdt -> 设置 cr0 的 pe 位
    - 其中顺序甚至可以打乱, 每个步骤又很繁杂
- 这么设计已经算节约成本的最佳实践了

什么叫 "一种模式的指令"
- 一条指令是否有专门属于某种模式一说
- 按照书上的意思, 如果要加上前缀, 说明这就不算本模式的指令了, 更底层上这种指令肯定用到了"不属于"自己的资源(比如 reg, 默认的操作数位长也是资源)

比如, 我们具体看一下反转的类型
- 反转 (默认)操作数大小 0x66
    - 实模式下, `mov eax, 0x1234`, 用到了不属于自己的寄存器, 要反转默认的操作数大小, 来 fit 这个寄存器
    - 反过来, 保护模式用到 ax 也被认为 是越俎代庖... 不过你只要提供合适的操作数就行, (会不会被 cut 掉)
- 反转 寻址方式 0x67
    - 实模式下, `mov word [eax], 0x1234`, eax 不是自己的, 要反转
- 也可能连用两条前缀
    - 实模式下, `mov dword [eax], 0x1234`
    - dword 隐含了操作数默认位长要反转

总结: 汇编器看到一条指令, 不会设法搞明白是实模式还是保护模式, 但是只要你告诉他, 汇编器就能自动正确帮你, 把跨模式指令加上前缀

所以怎么反转我们根本不用记忆, 随便用 reg 就是了, 这正是 bits 为指令想帮我们抽象的

### 指令拓展
这个就是说, 保护模式因为多了寄存器, 指令也多了

add al, cl
add eax, ecx

重点说一下 mul
- 格式是: `mul reg/mem[i]`
- 如果 reg/mem[i] 是 8 位, 则另一个乘数是 al, 结果 ax
- 16 位, ax, 结果 eax
- 32 位, eax, 结果 edx:eax

等下这跟实模式不一样啊...
既然实模式能用保护模式的指令, 而之前的 16 位乘法, 结果存在 dx:ax 里...

这说明了啥, 存在保护模式和实模式下都有的指令
而且工作的逻辑接口是不一样...

push
- 立即数
    - 为了对齐, 实模式下 8 位拓展成 16 位
    - 保护模式下, 8 位拓展成 32 位, 16 位数不拓展
- 段寄存器
    - 实模式 16 位, 保护模式总 32 位
- 通用寄存器和内存
    - 均不拓展



### 全局描述符表 GDT

gdt 描述内存中的各个段, 从哪里开始, 到哪里结束, 有什么属性

gdt 本身存储在内存里面

gdtr 存储 gdt 的初始地址, 用 lgdt 来初始化

访存新范式
- 段寄存器 16 位存储选择子
- 选择子有 13 位是索引, 1 位表明选择谁(gdt/ldt), 2 位特权级
- 访存过程
    - 提供索引 -> gdt 查询 -> 返回段地址 -> 段地址 + 偏移地址 = 访存地址

关于 0 索引
- gdt 的 0 索引处不可用
- ldt 是啥, 不是很重要, 简单说就是希望硬件能原生支持多任务而设计的, 一个任务对应一个 LDT, 这样就原生隔离了
    - 和 gdt 很像, ldtr, lldt,
    - 也有区别: ldt 0 索引可用
- 所以索引 0 有什么深刻意义么:
    - 如果段选择子忘了初始化, gdt 访问的时候是全 0 的, 这样干脆禁止索引为 0, 发出一个异常来一定程度上 debug, 提醒你可能是忘了初始化
    - 而如果是 ldt 访问, ti 位一定会显式设置 1, 这样不太可能忘记把选择子也初始化,,, 这么说感觉有一定道理

除了全局和本地, 还有一个 中断描述符表
定义中断门, 陷阱门, 任务门

### 保护模式

开关: CR0 的第 0 位, PE 位(protection enable)

字段:
- s type a c dpl p
- x r c a
    - conforming: 如果该段是 jmp 的目标段, 那么该段特权级一定要高于 jmp 前的段的特权级, 同时如果 jmp 成功, 那么之后执行流的特权级应该不是 DPL 决定, 而是 遵从 jmp 前的特权级
    - 有种 setuid 的感觉
- x w e a
    - extend 栈的方向哪里拓展 0 上 1 下

### 处理器微架构

流水线

乱序执行
- CISC RISC 和 二八定律
- CISC 各个操作比较复杂, 容易相互关联, 不好乱序执行
- 后期的 0x86 虽然是 CISC 指令集, 但内部已经采用 RISC 内核, 而 RISC 操作比较微小, atom, 通常独立无关联, 因此适合乱序执行

缓存
- 寄存器和缓存差不多快的
- 局部性: 时间, 空间

分支预测
- 流水线 fetch 如果遇到分支, 会进行预测, 选择一条路径去预取
- 这种操作的合理性在于: 只要不到执行, 实际上对我的执行流是没有影响的, 就算预测错了, 我可以刷新流水线来扔掉预取的指令
    - 而如果不预测, 流水线只能卡着不动, 浪费很大
- 问题: 如何来预测, 流水线级数越大, 如果预测错了, 代价也会越大

naive 的预测: 2 位预测法
- 用 2 bit counter 来记录跳转状态, 每跳转一次就加 1,  直到加到最大值 3 就不再加; 如果未跳转就减 1, 直到减到最小值 0 就不再减了
- 当遇到跳转指令时,  如果计数器的值大于 1 则跳转;  如果小于等于 1 则不跳.

BTB 缓冲
- btanch target buffer
- 是一个表, 每项为 分支地址+预测分支+跳转统计
- 每次到分支, 先查表, 根据表的统计信息, 决定如何预测
- 表中没有, 则 fallback 到静态预测 (static predictor)


### 保护内存段

保护模式是如何对内存进行保护的?
本质上是访问控制

<!-- 主要分两步 -->
<!-- - 首先保证访问的 GTP 的表项是合法的 (也就是段本身是合法的, 或者说基地址是合法的 -->
<!-- - 其次保证, 访问段的时候不能跨段, (也就是具体的地址是合法的 -->

首先, 访问控制, 从加载选择子的时候就发生了

如果能随便加载选择子到 sreg, 那么保护模式保护个鸡毛

向 段寄存器(cs, ss, ds...) 加载 选择子 的过程
- 检查 index: gdt_base + selector_index * 8 + 7 <= gdt_base + gdt_limit
    - 如果 TI 是 1, 则比的是 ldt
- 检查 type, 有一些规则如下
    - 有 x 才能 load 到 cs, 同时只 x 不能 load 到其他
    - 有 w 才能 load 到 ss
    - 有 r 才能 load 到 ds, es, fs, gs
- 检查 P
    - 如果不在内存, 处理器先来个抛出异常, 自动处理异常, 取到内存, 然后 CPU 指令
    - 取到之后, 一般由 OS 设置 P = 1 (CPU 是设置 A)

完成上述步骤, 选择子就可装到 sreg 里, 同时 段描述符缓冲寄存器(也是 64 位) 会更新为相应的段描述符的内容

对代码段和数据段的保护
- 拿到段地址后, 每当去真正访问一个地址, CPU 会确认, 地址不能超过 段描述符的范围
- 这个时候其实就不用检查段地址了, 只检查偏移地址
- 数据段: off_addr + data_len - 1 <= 实际段界限: (sd_limit + 1) * granularity - 1
- 代码段: off_addr + inst_len - 1 <= 实际段界限: (sd_limit + 1) * granularity - 1
- Q: 而对于栈....
    - e, 向上增长和向下增长?
    - 如果栈是向上增长
    - 实际段界限 <= esp - 操作数 <= 0xffff_ffff

### 获取物理内存容量

linux 的 `detect_memory`, BIOS 0x15
- eax = 0xe820, 所有内存
    - 内存布局, dmesg
    - 迭代式查询, 每次只返回一种类型的内存
    - 返回格式: ARDS(base + len + type)
- ax = 0xe801, 最大 4G
- ah = 0x88, 最大 64MB

BIOS 中断是实模式下的方法, 只能在进入保护模式前调用
- 那么我们设计, 在实模式调用得到内存信息后, 再进入保护模式

ARDS 格式(address range desciptor structure)
- 共 20 字节, 每个字段 4 字节
- 前 16 字节: base low, base high, len low, len high(单位是 bytes)
- 最后 4 字节: type, 标记是否内存可被 OS 使用
    - 为什么可能不能使用? 比如: 系统 ROM, ROM 用到的内存, 设备内存映射, 特殊原因不适合标准设备使用等等


### BIOS 0x15

如何调用 BIOS 中断呢, 我们详细看看 0x15 的 spec

子功能号 eax=0xE820
- 其他输入
    - ebx ARDS 后续值(类似 next 指针的作用, 初始置 0, 后续不用管)
    - es:di 为输出准备的缓冲区
    - ecx 指定要输出的 ARDS 结构的字节大小(20 字节?)
    - edx 固定签名, "SMAP" 的 ASCII 码 0x534d4150 (S 是寄存器高位)
- 输出
    - cf 表明是否出错
    - es:di ARDS 的内存信息
    - eax "SMAP"
    - ecx 实际写入的 ARDS 字节数
    - ebx 后续值

子功能号 ax=0xE801
- 输入: 就只有 ax
- 输出
    - cf 是否出错
    - ax = cx, 单位 1KB, 15MB 以下的内存(最大 0x3c00 * 1024 = 15MB)
    - bx = dx, 单位 64KB, 16MB - 4GB 的连续单位, bx * 64 * 1024
- 为什么分两部分, 15MB-16MB 去哪了?
    - 如果你实际在内存大于等于 16 MB 的主机上尝试会发现, ax*1024+bx*64*1024 得到的结果正好比实际的少 1MB, 原因在于 BIOS 中断不会把这 1MB 算在里面
    - 祖传问题来源: 80286 寻址空间正好 16MB, 当初拿了 15MB - 16MB 去映射 ISA 设备, 后面就一直保留了, 形成了内存空洞(memory hole)
    - 不过现在 BIOS 可由用户选择是否开启
- 为什么有冗余, ax=cx, bx=dx? 鬼知道, 两个字段可能是不同含义, 不过结果的值恰好是相等的

子功能 ah=0x88
- 只显示 1MB 以上的内存量, 且最多到 64MB (也就是最大返回 63MB)
- 输入 ah, 输出 cf, ax (1KB 单位)


## 分页, 虚拟地址

### 页表

> 即使只有 512 MB 的内存, 也能让每个进程的内存空间达到 4GB

思考: 软件和硬件的关系? 某个功能是硬件提供的, 还是 OS 提供的?
- OS 有很多经典的概念/机制, 但这些机制可能不是一开始就有的, 是随着漫长岁月而发展起来的, 硬件没有在其诞生的时候就支持
- 但好在计算机是图灵机, 只要你能说得通的, 形式化的概念都能用软件的方式实现出来, 但软件归根是依赖硬件的, 实现同样的功能, 软件的抽象度高, 效率也低, 当一个概念经受住检验, 足够成熟, 硬件级的实现自然就会出现
- 如果问 OS 是否要比硬件起源更早, 很难说, 但有一点是确定的: OS 肯定是硬件之间是相互依赖,  相互推动,  相互促, 进而发展起来的, 页表就是其中之一吧

纯分段的内存管理的问题
- 换入换出粒度问题
    - 内存碎片化...
    - 换入换出的段太大, IO 成本问题
- 物理内存容量问题
    - 如果物理内存太小, 本身就是无法容纳一个程序, 那么就没法运行了...

分页机制
![分页](http://img.phanium.top/20230411185125.png)

每加载一个进程, OS 按照进程中各段的起始范围, 在进程自己的 4GB 虚拟空间中寻找可分配的内存段

页表的粒度不能太小, 否则页表数目过多, 对于 4G 空间
- 页表项数 1M, 页大小 4K, 页表大小 1M * 20 bit = 2.5MB (当然存到页表里 一般还是用三十二位, 所以是 4M)
- 页表项数 4K, 页大小 1M, 页表大小 4K * 12 bit = 6KB
- 页表项数 2^m, 页大小 2^(32-m), 页表大小 2^m * m bit (实际上 2^m * 32)
- 逐字节映射: 页大小 1B, 页表项数 2^32, 页表大小 2^32 * 32b = 16GB
- 当然页表项也有别的字段, 但总的来说, 页越小, 页表项目越多, 页表越大

关于启动和使用分页机制
- 分页打开前, 将页表地址(纯物理地址)加载到 cr3
- 分页打开后, 访问页表本身相当于裸访问纯正的物理地址
    - (否则递归: 访问页表要先转换页表的地址本身, 而我不知道页表真实物理地址, 也就没法找到页表来转换地址, 没法转换地址我就找不到页表, 逻辑闭环, 永远找不到页表)

如何用线性地址找到页表的对应项? (页大小 4K 的一级页表)
- 每次拿到一个线性地址, 先查找页表, 得到真实的物理地址
- 如果页表只是软件的逻辑实现, 同时页表存放在内存里, 那么每次访问一个地址的步骤是:
    - 软件实现思路是: parse 线性地址, 得到页表索引(取高 20 位, 乘以 4) -> 访存查页表, 取出结果 -> 拼凑得到物理地址(高 20 位拼接 offset) -> 访存
    - 这样太麻烦, 所以 CPU 至少会集成页部件, 过程是: 线性地址 -> 通过页部件得到物理地址 -> 访存
- 但无论如何: 就算是一级页表, 现在也需要至少两次访存, 简直不可理喻, 后面会加上 cache

###  二级页表

一级页表的局限
- 每个页表 4M, 看上去挺好, 但每个进程创建的时候就都得有自己的页表
- 于是: 再小的进程至少也要 4M

二级页表
4G 空间 = 1K 个页表, 每个页表 1K 项, 每页 4K
- 每个页表大小 1K * 4B, 正好占据 1 页
    - 页表可能位于内存的任意位置, 用 10 bit 作为索引查找 PD, 得到 20 bit 以 4K 为单位的地址
- 页目录表(PD) 作为页表(PT) 的表, 也正好占一页..
    - 总大小 1K * 4K + 4K = 4M + 4K
    - 最差情况有千分之一的损失....
- 访问过程: 线性地址 -> 通过页部件, 得到页的物理地址 -> 访存, 得到页 -> 通过页部件, 得到物理地址 -> 访存

说到底, 这到底好在哪里了?
- 每个进程都会分配一个页目录表 4K
- 但页目录表上的每个页表可以不立即分配
- 空间上: 最差情况有千分之一的损失.... 但最好情况有: 千分之九百九十九的获益
    - 剩下的都交给统计了
    - 访存的短板: 用 TLB 来弥补

![PD-and-PT](http://img.phanium.top/20230411200433.png)
- Present: 是否在 内存中
- Read/Write: ? (看了 intel 文档, 没错, 页表和段表的机制是有重合的)
    - 其实也没啥奇怪的, 段表我们基本只用到 平坦模型
    - 硬件厂商考虑的要多得多, 段表在我们的 OS 里没有必要性, 但别的 OS 未必就不用
- User/Supervisor
- PWT (Page-level Write-Through)
- PCD (Page-level Cache Disable)
- Accessed
- Dirty
- PAT (Page Attribute Table)
- Global
- Available: OS 来维护, 我猜可能做一些页面置换算法

### 开启分页

步骤
- 准备好 页目录表 PD 和页表 PT
- 将页目录表地址写入 cr3 (又称 pdbr, 页目录基址寄存器)
- 寄存器 cr0 的 PG 位置 1

CR3, PDBR
![PDBR](http://img.phanium.top/20230411201846.png)
- 页目录表也存在一个 "自然页" 里, 所以只要 20 bit 就能索引到
- 属性只有 PWT PCD
- 如何设置? 简单的 mov cr3, r32

一个简单的问题: 透明访存
- 注意开启分页后的访存总是有 cr3 的参与, 但这件事对我们来说是完全透明的, 借助 cr3 中的物理地址和页部件的配合, 我们能隐式地直接访问 PD,  进一步隐式地直接访问 PT
- 那么能否直接通过 cr3 进行访问呢?
    - 比如 mov ax, [cr3 的高 20 位 << 12]
- 取决于分页开启没开启
    - 如果没开启, 那么访问的就是未来的页表
    - 如果开启了, 那取决于: cr3 作为 vaddr 指向那个 paddr
- 所以 如果你没映射好, 页表就访问不了了

<!-- 删: 傻逼的论证, 事实上, 我的想法太 naive 了, 页表肯定要由 OS 管理, OS 必须能访问到页表, 这个透明是对用户透明, 而不是对 OS 透明 -->

### 页表管理

页表的维护
- 硬件本身提供页表的机制, 之后由 OS 来完善这套机制
    - 硬件的支持本质上来自 软件的需求, 高效率的页表是软硬件协同的结果
- 页表是 local to 进程的, OS 需要做的是动态维护
- 然而在分页后和进程在看待内存上是平等的
    - 关键: 为了管理内存, OS 需要能访问自身的页表

另外还有个问题, 用户程序运行的时候, 经常得用到 OS 提供的服务(用户进程永远无法完整做事)
- 假设每个进程的虚拟地址空间都是独占的, 0-4G 都是自己的, 这样每次 call OS, 应该先切换到 OS, OS 得有自己的地址, 这样换来换取的成本比较大
- 那干脆
    - 让 OS 存在于所有用户进程的虚拟地址里面, 0-3G 是 user, 3G-4G 是 os
    - 让 os 被所有 user "共享"
- 如何实现呢: 只要让进程的 3-4G 的页表项目相同就完事了

看看作者的设计
- PD 位置: paddr 1M 开始
- PD 先将 vaddr 0 和 vaddr 3G 开始的 1M 都映射到 paddr 0-1M
    - (假设 OS kernel 暂时不超过 1M? 前者是临时保留的? 当然 PD 分配的最小粒度是 4M
    - 通过将两者映射到同一个 PT 来实现, 然后 PT 分配前 256 项即可
- 第一个 PT(PT1) 位置: 紧邻 PD 后, paddr 1M + 4K 开始
- vaddr 3G-4G 剩余部分
    - 随后 254 个 PDE, 分配剩余的页表项 (需要 254 个 PT, 直接从 PT1 后面开始分配
    - 最后一个 PDE, 自指 PD: 目的是让 vaddr top 4K 指向 PD

回顾: 分页后, OS 需要拿到 PD 的 vaddr
- 通过分页前的设置: 正常分配就做得到
    - 先指定一个希望映射的 PD 的 vaddr (4K align 的随便某个空闲地址)
    - 分配对应位置的 PDE: 索引为该 vaddr 所在的 4M align, 字段指定一个 PT
    - 然后, PT 取对应偏移处的 PTE, 将其映射到 PD
- 但这里作者没有这么做, 而用了自指的技巧, 更优雅, 简洁

### 魔法与自指

> 破坏约定, 但符合规则

假设: 我们目的是希望通过 top 的 4K vaddr 来访问我们的 PD

回顾下, 正常人的思路是, OS 就该像访存一样访问页表, 那么把 PD vaddr 映射到 PD paddr 就完事了
- 首先, 因为是 top 4K, 必须拿 PD 的最后一项, 随便指向一个页表的地址
- 然后, 这个页表也要映射他的最后一项, 然后指向 PD 的内存地址

这个做法没有任何问题, 我们甚至还空出来 4M-4K, 他们是自由的, 可以随便存别的东西
然而可以不这么想: 既然 PDE 指向的东西就是 4K, 直接用他装一个 PD 不就完了...
看似荒唐, 但如上面所见, 奇迹般地殊途同归
- 首先也是拿 PD 的最后一项, 这里原本能指向一个 (最终能指向 4M 物理地址的)页表, 而魔法师只用来指向一个 PD
- 然后自指发生了, "分页机制"会把这个指向的 PD 当作是 "PT", 然后 "PT" 的最后一项, 被认为是 top 4K vaddr 到 PD paddr 的映射
- 本质在于
    - 注意到, vaddr 本身就已经完全确定了其将来在 PT 和 PD 中的索引的位置
        - PD 的粒度是全局 4M align 的 vaddr 结构, 全局的 top 4K 在全局的 top 4M 中, 所以会访问最后一项
        - PT 的粒度是局部 4K align 的 vaddr 结构, 全局的 top 4K 在局部的 top 4M 中也是 top 4K, 所以会再次访问最后一项
    - 进一步, 又因为 PD 和 PT 的结构恰好一致 (大小, 项数)
    - 那么, 只要设计 vaddr 两部分索引相等, PD 的相应位置指向自己, 就能保证 两次查表的结果都是 PD 自身

当然, 如果你认为: top 4M 的 top 4K 映射恰好是没问题的, 但 4M - 4K (剩下 1023 项) 的映射被浪费了
- 这种做法也不全是优点吗?  或许是, 毕竟这种做法总会浪费掉一个 PDE
- 但实际上: 通过 top 4M - 4K vaddr, 你还能访问其他任意 PT
    - 如果使用普通的做法来映射所有的 PT: 你同样会浪费掉一整个 PDE, 此外还有它实指的 PT
- 这才是自指魔法的强大之处, 既优雅, 又高效

看看魔法的结果吧:

info tab 可显示页表
```python
cr3: 0x000000100000
# 这两项挺正常
0x00000000-0x000fffff -> 0x000000000000-0x0000000fffff
0xc0000000-0xc00fffff -> 0x000000000000-0x0000000fffff
# 这三项看上去很奇怪 但实际确实是正确的
0xffc00000-0xffc00fff -> 0x000000101000-0x000000101fff
0xfff00000-0xffffefff -> 0x000000101000-0x0000001fffff
0xfffff000-0xffffffff -> 0x000000100000-0x000000100fff
```
bochs 当然是是遵守约定的: 总认为每一项 PDE 的内容是一个 PT
- 然而我们是用 PDE 映射了 PD, 而不是 PT
- 根据我们前面的推理, 这一定是正确的结果

不妨人工 parse 一下后面三行
- 首先索引到到 PD 的第 1023 项 (vaddr 最高 10 位全 1), 得到的"页表"还是 PD
- 然后分类讨论 vaddr 中间的 10 位:
    - 如果全 0, 那么就会索引到"页表"的第 0 项, 得到 0x101000 被认为是物理地址, 所以再加上 offset:
        - 实际上物理地址就是第一个页表
    ```
    0xffc00000-0xffc00fff -> 0x000000101000-0x000000101fff
    ```
    - 如果为 11_0000_0000b, 那么就会索引到"页表"的第 768 项, 得到 0x101000
        - 类似 769 -> 0x102000, 直到 1022(11_1111_1110b) -> 0x1ff000
        - 实际上物理地址就是第 769 - 1022 个页表
    ```
    0xfff00000-0xffffefff -> 0x000000101000-0x0000001fffff
    ```
    - 如果为 11_1111_1111b, 那么就会索引到"页表"的第 1023 项, 这一项正好再次指向 PD 自身, 得到 0x100000
        - 实际上物理地址就是页目录自身
    ```
    0xfffff000-0xffffffff -> 0x000000100000-0x000000100fff
    ```

<!-- 勘误: -->
<!-- - 这里原书的第二项映射的不全, 估计是没编译后面添加的 254 个 pde -->

### TLB
translation lokkaside buffer

多级页表代表着多级访存, 要想加速这个过程, 自然就想到缓存, TLB 应运而生

TLB 条目, 例子
- 高二十位虚拟地址 -> 高二十位物理地址
    - 如果 hit, 那么不用访存
    - 如果 miss, 要逐级查页表, 得到物理地址, 并填充到 TLB
    - 如果查页表没找到(P=0), 那么就是缺页中断
- 缓存同步问题: TLB 必须实时更新(依靠 OS 的策略)
- 刷新 TLB
    - 重新加载 cr3
    - 或, 使用 invlpg (invalidate page)
        - `invlpg [0x1234]` 更新 0x1234 对应的条目
        - (而不是 `invlpg 0x1234`)


## elf 文件格式与加载内核

这里内核要编译成 elf 格式的文件, 有必要学习下 elf 是怎样的, 如何解析和加载

加载 和 格式
- 最简单的加载: load 到内存, 然后 jmp
- naive 格式: header + body
    - header 来描述程序的 metadata (len, entry)
    - 加载过程: 先加载 head, 再加载 body, 跳转到 entry
- 标准化格式: elf, pe, coff

elf 格式
![elf](http://img.phanium.top/20230413092130.png)
- relocatable, shared object, executable
- program header table
- section header table

elf header
```c
typedef struct
{
  unsigned char	e_ident[EI_NIDENT];	// Magic number and other info(32/64, endian, ver...)
  Elf32_Half	e_type;			    // Object file type: dyn, exec, rel
  Elf32_Half	e_machine;		    // Architecture
  Elf32_Word	e_version;		    // Object file version

  Elf32_Addr	e_entry;		    // Entry point virtual address
  Elf32_Off	    e_phoff;		    // Program header table file offset
  Elf32_Off	    e_shoff;		    // Section header table file offset
  Elf32_Word	e_flags;		    // Processor-specific flags

  Elf32_Half	e_ehsize;		    // ELF header size in bytes
  Elf32_Half	e_phentsize;		// Program header table entry size
  Elf32_Half	e_phnum;		    // Program header table entry count
  Elf32_Half	e_shentsize;		// Section header table entry size
  Elf32_Half	e_shnum;		    // Section header table entry count
  Elf32_Half	e_shstrndx;		    // Section header string table index
} Elf32_Ehdr;
```
- e_ident: magic + 类型(32/64) + 端序 + 版本 + ...

```c
typedef struct
{
  Elf32_Word	p_type;			// Segment type: load, dyn, interp, note, phdr
  Elf32_Off	    p_offset;		// Segment file offset
  Elf32_Addr	p_vaddr;		// Segment virtual address
  Elf32_Addr	p_paddr;		// Segment physical address
  Elf32_Word	p_filesz;		// Segment size in file
  Elf32_Word	p_memsz;		// Segment size in memory
  Elf32_Word	p_flags;		// Segment flags
  Elf32_Word	p_align;		// Segment alignment
} Elf32_Phdr
```

实例分析

e_entry     0xc0001500
e_phoff     0x00000034
e_shoff     0x0000055c
e_flags     0x00000000
e_ehsize    0x0034 (phdr ttable 跟在 elfhdr 后面)
e_phentsize 0x20 (32B)
e_phnum     0x0002
e_shentsize 0x0028 (40B)
e_shnum     0x0006
e_shstrndx  0x0003 (sh 的第三项)

第一个 program header
p_type      0x00000001 (PT_LOAD)
p_offset    0x00000000 (?)
p_vaddr     0xc0001000
p_paddr     0xc0001000 (reserved for old arch)
p_filesz    0x00000505
p_memsz     0x00000505
p_flags     0x00000005 (PF_R+PF_X)
p_align     0x00001000

如何在 file 中找到 e_entry 的位置呢?
- 基本思路: 找到 e_entry 所在的 segment, 计算 e_entry 在该 segment 的偏移, 加上该 segment 在文件中的起始位置
- 首先遍历可用的 phdr
- 如果找到 p_vaddr <= e_entry < p_vaddr + p_filesz, 说明入口在里面
    - e.g. 0xc0001500 在 [0xc0001000, 0xc0001000 + 0x00000505)
- (e_entry - p_vaddr) + p_offset
    - 0x500 + 0


此时的内存布局
![a-mem-map](http://img.phanium.top/20230414104913.png)
- 所有的地址都在 1MB 下, 这在之前设置的页表中是能查到的
- 加载执行 MBR -> 加载执行 LOADER(设置 GDT, 开启 PAGE) -> 加载 kernel -> 映射 kernel

## 特权级机制

相当绕, 心路历程: 先速览感受下, 去 RTFM, 再回来
- <https://zhuanlan.zhihu.com/p/27419639>
- [中文 doc](https://12785576289919367763.googlegroups.com/attach/bd6b58d6f50fef4f/IA-32架构软件开发人员手册_卷3: 系统编程指南.pdf)
    - 翻译问题很多, 但大体能看懂
- [英文 doc](https://cdrdv2-public.intel.com/774493/325384-sdm-vol-3abcd.pdf)

### ring

![特权级别](http://img.phanium.top/20230414110751.png)
- ring0: OS
- ring1-2: 系统服务, 如驱动程序, 虚拟机
- ring3: 用户程序

> 敬告: 因为兼容问题和历史遗留问题....
x86 的特权级机制很大程度上建立在 段机制上

执行流特权级转移的一般规则 (可简单理解为: load cs)
- 平级转移
- 低级 -> 高级: 所谓..陷入内核
    - 需要通过 中断门, 调用门等机制
- 高级 -> 低级: 仅当返回时, 否则不允许

数据访问权限的一般规则 (可简单理解为: load ds fs gs 等)
- 低级访问高级: 所谓..陷入内核, 禁止
- 高级访问低级: 允许

同时不同的特权级在执行时有不同的栈(为什么)
- 防止高特权 proc 因为栈空间不足崩溃
- 防止低特权 proc 干扰高特权程序执行

### TSS 简介

TSS (task state segment)
- 处理器在硬件上原生支持多任务的一种实现方式
- 描述/存储 一个任务 (进程) 需要的 status
- 通过 TR 来找到, 同时在 GDT 里有 TSS desc

实现 OS 概念前, task 机制实际可以看作是硬件级的 proc, 有相当强的指向性如下
- TSS 硬件支持的系统级数据结构 -> OS 维护的 PCB
- 重新加载 TR -> OS 切换任务
- TSS 提供用于 stack switch 的字段 -> OS 实现 proc 用户态到内核态的切换
- 很自然的, 可以基于 TSS 相关机制来实现 OS 层面的 proc
    - 但 linux 没这么用...
    - 很大程度上, TSS 是配合 段机制使用的

> intel: 我们设计了 TSS, 顺便一提, 他可以 balabala
> linux: 你在教我做事?

这里主要谈 stack switch
![TSS](http://img.phanium.top/20230414153538.png)
- 每个任务最多有四个栈, 但 TSS 最多只需要维护三个
- 因为只有低到高的 stack switch, (比如)彼时可将 当前 ss/esp 压到新栈, 之后返回的时候通过上下文就好了
- 另外 高级栈 是只读的, 不会自动把该高特权级栈指针更新到 TSS 中

### RPL CPL DPL

RPL: 请求的特权级
- SELECTOR = INDEX + TI + RPL
- 选择子低两位字段
    - 可以是 eip 指向的内存的 选择子
    - 也可以是 sreg (比如 cs ds) 中的 选择子
- 本质意义
    - RPL 在 指令里, 作为请求资源者的特权级的软约束?
    - RPL 在 cs 里, 是 ... CPL
    - RPL 在 ds 里, 是 ... 最近使用当前资源者(可 fake...)?

DPL: 标识一段 代码/数据 的特权级
- 选择子指向的具体描述符的 field

CPL: 当前执行代码的特权级
- 具体的, CPL 就是 CS.RPL, 只不过是一种特殊的 RPL
- 抽象的, 表示当前执行者的身份
- 当发生执行流的转移(加载 cs), 会发生特权级的检查, CPL 也会跟着变化

如何理解 CPL 是执行者的身份
- 直接执行者永远是 CPU, 这不错
- 但要知道: CPU 也是有不同状态的, RPL 即对其特权状态的建模
- 当你通过 IO (键盘, 显示器) 与 CPU 交互时, 本质上你就改变了 CPU 的状态
- 你的用户身份, 通过很多层抽象, 变为用户程序, 最终变为 sreg 里 RPL
- 一个有趣的观点:
    - 如果没有中断, CPU 没有 IO, 那么他就是决定论的机械, 一切未来的状态都可以靠数学推演
    - 而有了中断和外设, 进一步通过人的交互, CPU 才有了可能性和变化

误区警示: 一定区分 RPL 和 CPL
- 后面要谈到的所谓 RPL 大多是指 jmp/call cs:offset 这类指令蕴含的 RPL, 要注意这个 RPL 不是 CS.RPL
    - 执行这条指令后, RPL 并不立即加载到 CS 里
    - 可理解为: 先执行权限检查, 然后才写 CS.RPL
- 另外其他 sreg 的 RPL 不是 CPL (DS.RPL 不叫 CPL)
    - 只有代码段是能动, 可执行的, 只有 "能动" 行为才具有访问能力, 能标识一段代码的身份, 归属者
    - 那么 DS.RPL 这种有啥用
        - 目前, 只在 retf 里发现
        - 上下文清理, 内核执行完后, 得把 ds 这类状态也清理下, 因为检查只发生在准入阶段
        - 清理的办法也特别"机制化": 置为空

CPL 的生命周期
- 进入 MBR(jmp 0x7c00) 后: 初始 CS.RPL 为 0
- 进入保护模式后: `jmp SELECTOR_CODE:p_mode_start`, 进行特权级检查, 才将 sel 加载到 cs

特权级检查 (假设要访问的资源是 nonconforming code segment, data segment, 而不涉及门)
- 受访者为代码段: DPL = CPL, RPL <= CPL(DPL), 只允许平级访问
- 受访者为数据段: DPL <= CPL, DPL <= RPL (虽然 RPL 可以随便改)
- 为啥要用 RPL, 这种可篡改的东西?
    - ring3 直接 访问自己数据的时候肯定没用
    - 但 ring3 要是先 syscall, ring0 代理的时候就有用了, ring0 通过 RPL 知道自己在代理谁办事 (另外要注意: 用 arpl 事先覆盖掉 RPL 的不诚实
    - 理解方法: 把 RPL 约束去掉, 想一下 syscall 后会发生什么 -- 可以代理执行任何指令
    - 虽然 syscall 的内容是软件的, 只要 OS 编写者足够细心, 也能设计出安全的机制, 但说不准就出现了访问内核数据的逻辑, RPL 能在必要时保证彻底禁止这种事情的发生

> 例子: 假设 某调用门 只需要一个参数, 就是用户程序用于存储系统内存容量的缓冲区所在 数据段的选择子和偏移地址
> 安全起见, 在该内核服务程序中, 操作系统将这个用户所提交的选择子的 RPL 变更为用户进程的 CPL, 也就是指向 缓冲区所在段的选择子的 RPL 变成了 3

代码段只允许平级访问
- 高 -> 低: 低特权级能做的事, 高特权级也能做
    - 除非通过门来跳转后的返回...
- 低 -> 高: 当然是安全
    - 需要引入其他机制来 提高特权级, 实现系统调用
    - conform 也可以实现 低到高, 但 CPL 不变

特权级检查仅仅发生在访问的时刻 (加载选择子)
- 而不是 持续的检查 (比如 键盘的外设事件发送...)

conforming, 穿过特权屏障的一种方法
- code 段的 conforming = 1, 则允许低级到高级
    - 特权级检查变为要求 DPL <= CPL
    - 逻辑上可以理解为: 临时修改 DPL, 让 DPL 去 conform CPL
        - 如果 DPL > CPL, DPL = DPL
        - 如果 DPL <= CPL, DPL = CPL
- 痛点
    - 这种"提权" 不会修改 CPL 的值
        - 所以就算到了系统段代码也还是凡人(没有特权指令, IO, 没法调别的系统函数)
    - 同时 貌似可以 CPL 到达 目标段的任意位置 (只要设置 epi)
        - 难怪不给权限

### 门

(调用)门: 提权的另一种方式
- 门也是 一种 描述符 (描述描述符的描述符/元描述符)
    - S 字段为 0 (此外 S=0 的还有 TSS desc, IDT desc)
    - 描述 中断处理程序描述符 的描述符 (中断门/陷阱门)
    - 描述 例程描述符 的描述符 (调用门)
    - 描述 TSS 描述符 的描述符 (任务门)
- 可见(调用)门是一层新的 间接, 作用为 代理提权
    - 或者说是一种 转接员
    - 比如, 用户程序读一个文件, 但一般 OS 下用户本身对硬盘是无访问权限的, 因此发起系统调用来提升特权级, 在提升处理器的 CPL 后才能控制硬盘, 读写文件
- 但(调用)门不一定提权...

![desc](http://img.phanium.top/20230414171033.png)

不同的门所在位置
- 任务门: GDT, LDT, IDT
- 调用门: GDT, LDT
- 中断门, 陷阱门: IDT

门的用途和使用方式
- 调用门: 实现系统调用, call 提权, jmp 平级
- 中断门: int 指令/中断触发
- 陷阱门: int3 指令 (编译器调试)
- 任务门: 任务切换, 中断, 中断指令, 或 call/jmp

### 调用门

用途: 提权, 系统调用
- 调用门一般用于多段编程模型, 但出于兼容等原因, 现在的系统调用很少用 调用门实现 syscall (linux 使用中断门)

调用门描述符的字段如下
![调用门](http://img.phanium.top/20230414183019.png)

调用门是如何提权的
- 一次通过门的访问, 会使用不同权限检查机制
    - 简单理解: 蹦床
    - 首先, 访问调用门/TSS 本身规则其实和数据段一样 (高权限就允许)
    - 其次, 调用门 指向的系统代码段, 将允许提权
- 当然调用门不一定提权
    - 内核程序也可以调用, "调用门", 完成平权转移


使用调用门的特权级检查流程 (简化省去分页)
- `call/jmp selector_gate:offset` (`offset` 会被忽略)
- 门的选择子 -> 找到门描述符 -> 权限检查 -> 找到系统代码段 -> 权限检查 -> 系统代码段地址 + 系统代码段偏移

![执行流程](http://img.phanium.top/20230414191822.png)

![调用门的特权级检查](http://img.phanium.top/20230519173243.png)

调用门的特权级检查
- Conforming = 1 || Conforming = 0 && call
    - DPL_GATE >= CPL >= DPL_CODE (之前说的 DPL 都是说 DPL_CODE)
    - DPL_GATE >= RPL
    - jmp 本身只能平权转移
    - 因此 jmp && C=0 时, 不能 按上述规则 jmp, 条件如下
- Conforming = 0 && jmp
    - DPL_GATE >= CPL = DPL_CODE
    - DPL_GATE >= RPL

(跨特权级时)不同特权级有不同的栈
- 如何切换栈
    - 实际上: 如果通过特权检查(不出问题), 那么会用目标的 DPL 来索引 TSS 段, 得到对应的 栈入口(入口初始化由 OS 负责),
- 如何向例程传参
    - 正常压栈即可: 处理器会帮你搞好的
    - 门描述符有一个字段: "参数个数"
    - 处理器在固件上能实现参数的自动复制, 将用户进程压在 3 特权级栈中的参数自动复制到 0 特权级栈中

call 调用门的完整过程 (ring0 -> ring3)
- 事先压栈 ring3 栈参数
- 执行前面说的两次准入检查
- 确定 ring0 栈
    - 用 ring0 code 的 DPL 索引 TSS 得到 ss 和 esp
    - 执行界限检查
    - 执行 ss 的 TYPE 和 DPL 检查 (虽然已经知道 DPL 是用 ring0 索引的, 但 CPU 没这个上下文...)
- 如果 ss 是 DPL <= CPL (低 -> 高), 则切换到 ring0 栈
    - 切换新栈并压旧栈 (你可以认为这是一个 原语)
        - 临时保存 ss_old, esp_old (这一步 CPU 有办法做到, 别细究)
        - 切换到 ss_new, esp_new
        - 在新系统栈, 压栈 ss_old, esp_old
    - 复制用户栈参数到系统栈 (根据门描述符的参数个数)
- 压入旧的 cs_old, eip_old
- 加载新的 cs_new, eip_new (同时刷新段描述符缓冲寄存器)
![call 调用门](http://img.phanium.top/20230414195607.png)

> 不知道你还记不记得你就 call 了一下, 但状态转移说完都快裂开了

与其他的 call 比较
- near call(不跨段): 压栈 args, eip
- far call(跨段, 不跨 ring): 压栈 args, cs, eip
    - 如果一开始是平级转移, 那么 ss 是 DPL = CPL (平级转移), 则直接把原栈当成新栈, 不必切换栈
- far call(跨段, 且跨 ring): 压栈 args; stack switch; 压栈 cs, eip
    - 所以最简单的理解方式是: 把 stack switch 当原语理解
    - stack switch: 切换新栈压旧栈, 保存参数

关于如何返回: 万众瞩目的高特权级回低特权级
- 首先, 不能用 ret, 要用 retf, 之前的 call 虽然没明确标注出来, 但他不是一般意义的同段内的 call
- 其次, 虽然 retf 没有上下文, 但 CPU 不知道自己是从 ring 几来的, 但只要使用 retf 就能返回
- 此外, retf 只允许特权 高到低, 也会检测特权级

从调用门 retf 的过程 (ring3 -> ring0)
- 检查 cs_old.RPL (未来的 CPL), 判断是否改变特权级 (只允许高到低)
- 弹 cs_old, eip_old, 再次触发检查 (毕竟没有上下文)
- 如果 retf n, 就 esp + n (毕竟回去的时候没有门描述符)
    - 一般让 n = 参数个数 * 参数大小
- 如果改变特权级, 也会检查 {ds,es,fs,gs}.DPL < cs.RPL (高权限), 将高权限的 sreg 直接填充 0
    - (防止用户访问到内核数据)
    - 为啥不用栈的方案: 处理器不相信一切软件, 包括 OS
    - 这操作其实很 hack: ds 允许 空加载, 同时 gdt 不允许 空访问, 绝
- esp 跳过参数 (通过 retf args)
- 恢复旧栈: 弹 esp_old, ss_old


区分: 调用门和 conform
- conform 的本质: DPL "迁就" 弥补低特权的落差, 不改变 CPL
    - 当特权级别更低的 DPL 访问, DPL 去 conform(迁就) CPL (可以想象成令 DPL=CPL, 所以之后的 CPL 不会改变)
    - 其他见 [doc: intel-doc]
- 调用门: 通过代理来弥补落差, CPL 会发生改变, RPL 可以不变
- 两者结合使用: 实现 CPL RPL 均不改变下的 控制流转移
- 另外: jmp 只允许平级转移, 因此也对 判断的语义 有一定影响
    - 因为: 只管去, 不管回来, 如果允许了反而糟糕了

> 说实话: 写到现在, 逻辑已经足够清晰了, 所以这么乱的东西耐心总会克服 (你说的都对, 别急着自我感动, 今天 intel 宣布 门概念将死... 2023.5.21

<!-- ### RPL CPL DPL -->
<!---->
<!-- 不通过调用门, (user/sys)直接访问一般数据和代码的检查规则 -->
<!-- - 对代码段 -->
<!--     - Conforming=0: CPL = RPL = DPL -->
<!--     - Conforming=1: CPL >= DPL && RPL >= DPL -->
<!--     - 触发条件: 转移特权级, 如 call, jmp, int, ret, sysexit 等, 也就是改变 eip 或 eip + cs -->
<!-- - 对数据段 -->
<!--     - CPL <= DPL && RPL <= DPL -->
<!--     - 触发条件: 加载选择子, 改变 ds, es, fs, gs -->
<!--     - 如 mov ds, ax -->

### IO 特权级

访问有特权级, 指令也有特权级 (Priviledge Instruction)
- 特权指令: 只有 ring0 能用的 lgdt lidt ltr popf...
- I/O 敏感指令 (I/O Sensitive Instruction): in, out cli, sti
- I/O 读写控制: eflags 的 IOPL + TSS 的 IO bitmap

IO 的执行
- 先看 IOPL 允不允许
- 再看 I/O bitmap 放不放行

eflags 的 IOPL
- 限制执行 IO 敏感指令的 最低特权级
- 决定任务是否允许操作所有的 IO 端口 (all or none)
    - 进一步, 细粒度的控制使用 IO bitmap
- 如何设置
    - pushf + 修改栈中内容 + popf (ring0)
    - iretd, 从中断返回的时候 pop 栈中数据 (ring0)
- OS 可成功执行, 其他特权级也能执行, 不会异常, 但不能成功

如果 IOPL 禁止, 通过 IO bitmap
- 进一步设置 65535 个端口
- 整体关闭, 再局部打开
    - 类似于 防火墙规则, gitignore 的 !

IO 特权级的价值
- 实际上, 直接全面禁止用户 IO 访问也没啥, 用户要使用 IO 直接 syscall 就好了
- 但是: syscall 是需要保存上下文环境的, 与其全盘禁止, 不如在 可控范围内放权一下, 从而免除一些陷入内核的麻烦
- 但这种放权一定要处处留心, 时时小心, 因此设计了非常细粒度的 IO 特权机制(限制指令 + 端口)
    - 例如, ring1 驱动程序, 但可以使用 in, out 等直接访问硬件

I/O bitmap
- 位于 TSS, 也可以没有
- 长度可以不足 8K, 但最后字节总要求是 0xff
- 1 表示禁止端口, 0 表示允许端口
- 生效条件: 只有 CPL > IOPL 才会生效; CPL <= IOPL, 所有端口都会打开
- 具体位置: 通过 TSS 的 IO bitmap offset 字段
    - 如果 offset (相对于 TSS 起点的偏移地址) 大于 TSS limit 那么就没有 IO bitmap (TSS limit 在 TSS desc 里)
    - offset 不在 limit 里 表明没有 IO bitmap
- TSS 大小
    - 包含 IO bitmap: offset + 8192 + 1 B (intel 65536 个端口)
    - 不包含 IO bitmap: 104

![I/O-bitmap](http://img.phanium.top/20230521112700.png)

所以末尾的 0xff 是啥
- IO 的访问可能是一次多字节的...
    ```asm
    in ax, 0x234
    # 等价于
    in ah, 0x234
    in al, 0x235
    ```
    - 约定末尾是 1, 防止发生过读了尾部
- 同时, 允许 bitmap 中不映射所有的端口


## 混合编程

说到内核那么一定有 C 和 asm 互调的技术
在此之前, 我们先重新标准化一下 汇编语言中所谓 的函数

两大类
- 单独的汇编, 单独的 C, 分别编译, 最后链接到一起 (汇编和 C 可以互相引用)
- C 中嵌入汇编 (内联汇编)

### 函数调用约定

混合编程的地基是 一套 contract: call conventions

函数调用约定 calling conventions
- 参数的传递方式
- 参数的传递顺序
- 谁来负责保存寄存器

其实之前我们讨论了如何在汇编中构造 "函数" 的范式, 但我们着重说明白传参的道理

当一个函数本身没有任何参数, 一般就说明他自身不会受到任何外部状态的影响, 不如叫做 过程更 合适
- (我记得有个编程语言, BASIC 还是 FORTRAN 么...就是这么规定的)

为啥需要 calling conventions
- 函数很多情况是一种交互式的, 有输入有输出, 输入的参数该按照什么规则传递, 返回值又按照什么规则传出, 栈又如何分配和清理...
- 函数自身如果调用函数该如何解决呢
- 这些都是写汇编应该考虑到的 (高级编程语言让编译器代理掉了)
- 因此我们需要完备的协议, 也就是范式: calling conventions
    - 一方面, 这是 caller 和 callee 的协议, 双方达成共识, 在接口的基础上完成解耦
    - 另一方面, 这也是合作的需要, 大家都这么做, 也方便写出统一可行的代码
- c 和 asm 都遵守同样的规则, 就能实现混合编程

calling conventions 的种类 (https://en.wikipedia.org/wiki/X86_calling_conventions)
- 调用者清理: cdecl syscall oplink
- 被调用者清理: pascal register(borland fastcall) stdcall fastcall
- thiscall
- 挑典型
    - stdcall: 右 -> 左 入栈; callee 清理栈 (ret argnum, 能同时弹出 eip 和参数)
    - cdecl: 右 -> 左 入栈; caller 清理栈 (允许函数中参数的数量不固定)

### 系统调用/库函数

linux 系统调用
- 类似BIOS 中断/ DOS 中断, 但入口只有 0x80
    - bios 入口很多: 0x0 - 0x20
- 为什么只有一个入口?
    - IDT 中很多中断项是预留的 (preserved), 莫得办法
    - 而 BIOS 走的是 IVT, 可使用的中断号多
- /usr/include/asm/unistd_32.h
- man 2 write


linux 系统调用提供的 API
- 封装的 c 库函数; 或者, 直接使用 int
- 区别: 前者是封装的 C 函数, 而也意味着必须遵循 calling conventions, 会额外的压栈操作
- 关于传参 (当输入的参数小于等于 5 个时, Linux 用寄存器传递参数)
    - eax 子功能号 (write 为 4)
    - 其余参数: ebx ecx edx esi edi


### 相互引用

```bash
nasm -f elf -o syscall_write.o syscall_srite.S
ld -o syscall_write.bin syscall_write.o -m elf_i386
```

asm 使用 c
```asm
extern c_print
_start:
    push str
    call c_print
    add esp, 4
```
- extern 汇编引用外部符号
- global 汇编导出外部符号

c 使用 asm
```c
extern void asm_print(char* ,int);
```
- 引用外部函数: 只需声明
- 引用外部变量: extern 修饰变量
- 导出外部符号: 全局定义即可


### 内联汇编

为什么有 c 了还想内联汇编
- c 有更友好的语法
- 汇编有 c 不提供的指令 细粒度的控制

https://gcc.gnu.org/onlinedocs/gcc/Extended-Asm.html

inline assembly 由编译器支持

AT&T 语法
- 最早在 UNIX 使用 语法接近 PDP-11 等处理器特性...
- 数字优先作为地址 操作数顺序相反

基本内联汇编
```c
#define asm __asm__
#define volatile __volatile__

asm("movl $9, %eax;""pushl %eax")
// 防止优化
asm volatile("movl $9, %eax;""pushl %eax")
```
- 指令必须在 "" 中
- 跨行用 \ 转义
- 指令之间分割: `\n`, `\t`, `;`

```c
  asm("pusha;\
      movl $4, %eax;\
      movl $1, %ebx;\
      movl str, %ecx;\
      movl $12, %edx;\
      int $0x80;\
      mov %eax, count;\
      popa\
");
```

### 扩展内联汇编
https://zhuanlan.zhihu.com/p/395130640

扩展内联汇编
- 汇编是人工分配寄存器, 高级语言是自动分配寄存器
- 人要是做不好备份操作, 就会和编译器的规则冲突
    - 漏掉一个 reg 可能有不可预料的后果
- 我们希望
    - 更省心的寄存器上下文恢复
    - 更简洁地描述我内联的目的
- 本质: 写一组约束, 描述输入, 输出, 改变的上下文
- 上下文: 内联汇编调用函数时, gcc 未必追踪得到, 因此要显式告诉

```c
asm [volatile] ("assembly code": output: input: clobber/modify)

// output/input
"[操作数修饰符] 约束名" (C 变量名)

// 寄存器 约束
asm("addl %%ebx, %%eax":"=a"(out_sum):"a"(in_a),"b"(in_b));
// eax = in_a  ("a" 相当于 eax 的 alias)
// out_sum = eax (执行 inline asm 后)


// 通用约束
// 只用在 Input 部分, 表示与 Output 和 Input 中第 n 个变量使用相同的寄存器或内存
asm("addl %1, %0":"=a"(out_sum):"b"(in_b),"0"(in_a));
// 这里 in_a 和 out_sum 使用相同寄存器

// 内存约束
asm("movl %0, %1"::"a"(in_a),"m"(in_b)); // in_b 改变
asm("movl %0, %1"::"a"(in_a),"b"(in_b)); // in_b 不变
asm("movl %0, %1":"=a"(out_a),"=b"(out_b):"a"(in_a),"b"(in_b));

// 立即数约束
asm volatile ("movl %0, %%eax"::"i"(100));
// 只能作为 右值(input)

// 序号占位符
// - %0 ~ %9, 所以...前面 %%eax 要转义
// - 就跟 format print 那种差不多
// 名称占位符
asm ("addl %[inb], %[out]":[out]"=a"(out_sum):"0"(in_a),[inb]"b"(in_b))

// 修饰符: 针对操作数
// output 中
//   =: 表示操作数是只写的, 一般存在 Output 中
//   +: 表示操作数是可读可写的, 相应的寄存器或内存先被读再被写入
//   &: 表示该操作数要独占相应的寄存器
// input 中
//   %: 该操作数可以和下一个输入操作数互换
asm("addl %%ebx, %%eax":"+a"(io_a):"b"(in_b))
// 这样就不用写 io_a 在 input 段的声明 (太 DSL 了)
```

clobber/modify
- 通知 gcc: 改变了寄存器/内存...
    - 若改变了寄存器 ebx 就可以申明 "bx", bl, ebx 都行, 都代表的是一个寄存器, 其他的寄存器同样如此申明
    - 若改变了 eflags 寄存器, 可以申明 "cc"
    - 若改变了内存, 可以申明 "memory"
- 另一个作用: 使 gcc 放弃缓存到寄存器
    - 编译器编译程序时, 将变量的值缓存到寄存器
    - 程序在运行时, 当变量所在的内存有变化时, 寄存器中的缓存还是旧数据, 运行结果肯定就错了

组合技, 实现一个 C 的 ins
```c
static inline void insl(int port, void *addr, int cnt)  // 从端口port读4*cnt个字节到地址addr
{
  asm volatile("cld; rep insl" :                    // 清零 DF 位, 重复指令 insl
               "=D" (addr), "=c" (cnt) :            // addr 目的地址绑定寄存器edi, cnt 循环次数绑定 ecx
               "d" (port), "0" (addr), "1" (cnt) :  // port 端口绑定 dx, addr, cnt 同上
               "memory", "cc");                     // 改变了内存, 改变了 eflags 寄存器
}
```

机器模式
- 当我们前面使用 "a" 时, 并没有说明用到的是 eax, ax 还是 al
- gcc 支持的内联汇编约束, 不能确切表达具体操作数对象, 因此 引用了机器模式
- 机器层面上指定数据的大小和格式

```bash
# pacman -Fl | ag machmode | head
gcc usr/lib/gcc/x86_64-pc-linux-gnu/12.2.1/plugin/include/machmode.def
gcc usr/lib/gcc/x86_64-pc-linux-gnu/12.2.1/plugin/include/machmode.h
aarch64-linux-gnu-gcc usr/lib/gcc/aarch64-linux-gnu/12.2.0/plugin/include/machmode.def
aarch64-linux-gnu-gcc usr/lib/gcc/aarch64-linux-gnu/12.2.0/plugin/include/machmode.h
arm-none-eabi-gcc usr/lib/gcc/arm-none-eabi/12.2.0/plugin/include/machmode.def
arm-none-eabi-gcc usr/lib/gcc/arm-none-eabi/12.2.0/plugin/include/machmode.h
```
h 输出寄存器高位部分中的那一字节对应的寄存器名称, 如 ah,  bh,  ch,  dh.
b 输出寄存器中低部分l字节对应的名称,  如 al,  bl,  cl, 'dle
w 输出寄存器中大小为2个字节对应的部分,  如ax,  bx,  ex,  dx.
k 输出寄存器的四字节部分, 如 eax,  ebx,  ecx,  edx.

## print

当我们谈论 print 时, 我们在谈论什么

### 显卡寄存器

如何打印
- 过去的打印: bios 中断, syscall, 写显存
- 所以 my print...

显卡寄存器 (非常之多), 分为不同寄存器组
- graphics (addr + data)
- sequencer (addr + data)
- attribution controller (addr + data)
- CRT controller (addr + data)
- color
    - dac addr write mode reg
    - dac addr read mode reg
    - dac data reg
    - dac data reg
- external (general)
    - miscellaneous output reg
    - feature control reg
    - input status #0 reg
    - input status #1 reg

为啥分 data 和 addr
- 要是所有的外设寄存器用 端口搞出来, 早晚不够用
- 所以: 用一个寄存器(addr)指定 array, 用另一个寄存器(data)指定 index

特别的是: miscellaneous output reg 的 I/OAS field
- 影响其他一些寄存器的 port 地址
    ![I/OAS](http://img.phanium.top/20230415152329.png)
- 如 默认 I/OAS = 1 下
    - CRT controller: addr 0x3d4, data 0x3d5
    - input status #1 0x3da (read port)
    - feature contorl 0x3da (write prot)

### 实现 print

首先实现 put_char
- 获取光标位置
- 处理不同字符, 分类讨论
    - 其中控制字符要自己定义行为
- 打印并移动光标

获取光标位置
- (pos 是 1D 线性地址, 80 * 25)
- CRT controller 的 0x3d4 写入 0x0e
- CRT controller 的 0x3d5 读取高 8 bit pos
- CRT controller 的 0x3d4 写入 0x0f
- CRT controller 的 0x3d5 读取低 8 bit pos

光标位置和显存地址转换
- 光标的位置是按照字符计数的: 2000 个
- 但显存地址每个字符占两个: 4000B
- 光标 -> 地址: addr = pos << 1

字符打印
- \b -> pos--, then fill ' ' to fake
- \n or \r -> \r\n
- pos > 2000 -> ?? 滚屏

滚屏的实现
- 显示的空间: 4000B
- 方案一
    - 控制显示的起始地址 (默认是 0, 显示: 0-4000)
    - 获取: CRT Controller Data 的 0xc 0xd
- 方案二
    - 固定显示的起始地址, 搬运内容... (我怎么觉得更麻烦...)

汇编的实现巧妙利用了 fallback...
- 如果触发滚屏, 那么光标一定要定位到最后一行行首部
- 因此逻辑是: 字符类型 ->
    - 回退 -> 直接设置 cursor
    - 普通字符 -> 是否滚屏 ? 换行 : 设置光标
    - 换行符 -> 是否滚屏 ? 滚屏 : 设置光标
- 滚屏 -> 设置光标

## 中断

> 操作系统 = 死循环 + 中断处理...

> 更权威系统的定义或许应该看 intel manual

(而 arm 架构都叫异常...)

外部中断 (硬件, 被动, 异步)
- 来源: 两条信号线 INTR NMI
- INTR(interrupt): 外设 可屏蔽中断 (设置 eflags 的 IF)
    - 理论上, 可以不予理会(当然仅仅是理论上)
    - linux 处理方式: 上半部 + 下半部
    - 上半部: 刻不容缓, 关中断执行; 下半部: 可被中断
- NMI(non maskable interrupt): 灾难性错误 不可屏蔽中断
    - 基本软件是解决不了, 只分配一个中断向量号 2
    - 掉电, 内存读写错误, 奇偶校验错误

内部中断 (软件, 主动, 同步)
- 软中断: 软件主动发起的
    - int
    - int3, into, bound, ud2 (异常, 主动 + 错误)
    - 本质上就是 软件 主动使用指令集 来引发中断
- 异常/程序错误异常
    - fault: 极易修复, 甚至有益, 会回去重试
    - trap: int3, 回到下一条指令
    - abort: 无法修复, 一般的处理是 kill 掉进程

(除了 INTR 中断, 都不受 IF 影响?)

按另一种分类方案
- 中断: 主动的, 被动的
- 异常: 主动的, 被动的

![intr-excep](http://img.phanium.top/20230521193950.png)

中断向量号 vector no. -> IDT (无 RPL)
- 异常和不可屏蔽中断的向量号: CPU 自动提供
- 外设的可屏蔽中断的向量号: 中断代理提供(e.g. 8259A)
- 软件中断的向量号: 软件提供
- 而且只提供号, handler 都要自己写么...

### IDT 和 中断处理

回顾: 中断描述符表 IDT
- 表项: 中断门, 陷阱门, 任务门,
- 任务门: 指向 TSS (GDT, LDT, IDT)
    - (多数 OS 不用 TSS 进行任务切换...)
- 中断门: 指向中断处理程序 (IDT only)
    - IF 自动置 0, 关中断, 避免嵌套
    - linux 系统调用经典实现: int 0x80
    - 加载: IDTR (GDTR, LDTR); 无 dummy
- 陷阱门: 指向..中断处理程序 (IDT only)
    - IF 不会置 0
- 调用门: 指向..一段高级例程 (GDT, LDT)
    - 用于用户进程提权

对比: 中断向量表 IVT
- 实模式下的中断查找
- 地址: 0x0 - 0x3ff, 4B/ent, 256 项 (IDT: 地址不受限, 8B/ent)
- (当时是 BIOS 负责实现 handler?)

- 根据中断向量号定位中断门描述符
- 特权级检查

中断发生和处理过程
- CPU 外: 有中断代理芯片(PIC)代收外设中断
    - 外设的 中断不是直接到 CPU 的
- 然后 PIC 和 CPU 通信
- CPU 内: CPU 执行中断向量号对应的程序

CPU 内的中断处理
- 总过程: 得到中断向量号 -> 查询中断描述符, 特权级检查 -> 执行中断处理程序 -> iret
- 特权级检查 (中断不允许同级转移..意思是)
    - 软中断(同调用门类似): DPL_CODE <= CPL <= DPL_GATE
    - 外设中断/异常: DPL_CODE <= CPL?
- stack 变化
    - 如果 stack switch, 通过 TSS 索引 ss esp, 压入新栈
    - push eflags cs eip (error code)
- 描述符的多样性: 中断门/任务门/陷阱门
    - 中断门: 置 0 NF TF IF
    - 任务门/陷阱门: 置 0 NF TF
![intr-handle](http://img.phanium.top/20230521200933.png)

进入 IDT handler 的 stack switch
![idt-stack-switch](http://img.phanium.top/20230521210118.png)

关于中断嵌套 和 IF
- IF 关闭时, 同一种中断未处理完时又来了一个, 会导致 GP
- 默认情况下, CPU 会在无人打扰的方式下执行中断处理例程
- 任务门或陷阱门的话, CPU 是不会将 IF 位清 0 的
- IF 如何手动改变
    - cli sti (原子修改)
    - pushf popf (万能法, 慎用)
- IF 位只能限制外部设备的中断
    - 对那些影响系统正常运行的中断 (异常, 软中断, int n, 不可屏蔽中断) 都无效

中断返回 iret (iretd)
- 一般的中断返回 (NF 为 0)
    - 过程: pop cs eip eflags; stack switch back
    - 期间会 检查数据段寄存器 DS ES FS GS 的内容
    - 注意: CPU 并不会主动跳过 error code, 必须手动将其跳过
- 此外, iret 也有另外的用途 (NT 为 1): 通过 TSS 返回上一个任务
    - 实际上: 执行新任务前, 写旧 TSS 选择子到 新 TSS 的上个任务字段, NT 置 1
    - iret 通过索引 TSS 的上个任务字段 旧任务

中断错误码, 但不是所有中断都有?
- 一种...中断的返回值
- 选择子 13 位索引 + TI + IDT + EXT
- EXT: 外部事件

### 8259A 及其编程

中断代理
- 多个设备发出中断, CPU 只能处理一个
- 中断代理(仲裁)决定哪个中断先被受理
- Intel 8259A 是流行的中断代理芯片 (可编程中断控制器 PIC)

Intel 8259A 结构
![8259A](http://img.phanium.top/20230419222332.png)
- 与 CPU 通信: INT 和 INTA 信号/接口
- IMR: mask, 屏蔽外设
- IRR: request, 等待队列
- PR: priority, 优先级仲裁
- ISR: in-service, 正在处理的中断

级联(cascade)机制
- 单片只支持 8 个中断, 对应 8 个 IRQ 接口
- 占主片 IRQ 但不消耗从片的 IRQ
- 级联至多 9 个
    - 1 个(master), 剩下至多 8 个从片(slave)
    - n 级: 7n + 1, 从片不被占用, 主片被占用
        - 8  - (n - 1) + 8(n - 1)

![cascade](http://img.phanium.top/20230419221939.png)


外设发生中断 -> 8259A 接收中断
- 外设发出中断信号, 经 IRQ 接口到 8259A

8259A 处理中断
- 检查 IMR(对应位), 该 IRQ 是否被屏蔽
    - 若屏蔽: 直接丢掉
    - 未屏蔽: 放行, 送入 IRR (相当于中断处理队列)
- 仲裁: 仲裁器 PR 从 IRR 中挑选优先级最高的
    - IRQ 号越小, 优先级越高 (这么看优先级是插入的时候就定死的, 静态的)

8259A 和 CPU 通信 (类比四次挥手..?)
- 8259A 向 CPU 发 INTR 信号 (INT 接口)
- CPU 执行完未完成的指令, 回复 8259A (INTA 接口)
- 8259A 将该中断对应 ISR bit 置为 1, 并从 IRR 中去掉
- CPU 再次发送 INTA 信号 (表明想获取中断号)
- 8259A 发送 中断号 = 起始中断向量号 + IRQ 接口号

中断号对外设是透明的
- 外设本身对自己 被代理这件事是没有感知的
- 外设也不知道中断向量号这回事, 只管发信号
- 所谓的中断号都是代理来做的

CPU 中断处理
- 8259A 的 EOI(end of interrupt)
- 如果设置为 非自动模式
    - 中断程序处理结束处, 必须有向 8259A 发送 EOI 的代码
    - 8259A 收到 EOI 后, 将当前正在处理的中断在 ISR 寄存器中的对应 bit 置 0
- 如果设置为 自动模式
    - 第二次发送 INTA 信号后, 8259A 会将中断在 ISR 中对应的 bit 置 0

新高优先级别中断的抢占
- 就算某个中断进入 ISR, 如果优先级更高的中断发生, 就会发生抢占

### 8259A 编程

过程
- 设置主片和从片的级联方式
- 指定起始中断号
    - 实模式下, BIOS 指定过 IRQ 0-7 的中断号 0x8-0xf
    - 保护模式后, 原来的中断号被分配给了异常, 需要重分配
- 设置中断结束模式
- 设置各种工作模式

寄存器 (寄存器既是对象又是接口)
- 初始化命令寄存器(ICW), 有写入顺序要求(设置间有依赖性)
- 操作命令寄存器(OCW), 顺序无所谓
- 要点: 不是所有的位都是有价值的, 很多历史遗留问题也在这种芯片上体现了

![icw-reg](http://img.phanium.top/20230420103136.png)

ICW1: 中断信号触发, 连接方式
- 中断信号: 电平触发/边沿触发 (LTIM)
- 连接方式: 单片/多片 (SNGL)
- 是否使用 ICM4 (IC4)

ICW2: 起始中断号 (也就是 IRQ0 的)
- 只需设置起始, 后面根据 IRQ 号自动加上 offset
- 因此只设置高 5 位, 低 3 位为 IRQ 号


ICW3: 级联下才需要 (SNGL = 0)
- 对主片: bitmap, 对应 8 个 IRQ 接口 (1 连从片, 0 连设备)
- 对从片: 自己在主片上的 IRQ 号 (只要低 3 位)
- 所以主片发送信号的时是广播?

ICW4: 用于设置一些工作模式
- SFNM: 特殊全嵌套模式?
- BUF: 缓冲模式
    - M/S: 缓冲模式下, 是主片/从片
- AEOI: 是否自动 EOI (中断结束信号)
    - 8259A 收到 EOI, 才能继续处理下一个中断
    - AEOI 可以让其不用等待信号自动结束中断
    - 否则要在中断处理程序中手动发送信号

![ocw-reg](http://img.phanium.top/20230420133919.png)

OCW1: 屏蔽外设信号, 0x21 0xa1
- 是否屏蔽 外设中断发送给 CPU
- 实际先考虑 eflags.IF, 如果 CPU 本身就屏蔽了, 这个放行也没用

OCW2: 中断结束方式和优先级, 0x20 0xa0
- 有个 EOI 信号位表明结束中断 (当 AEOI 有效)
- SL 指定特定 中断
    - 若 SL 为 1, 则用指定低三位指定 ISR 哪一个中断终止
    - 若 SL 为 0, 则低三位没用, 直接默认终止正在处理的中断
- R: 指定优先级别
    - 为 0, 始终固定优先级
    - 为 1, 轮转动态变换优先级, SL 指定的为最高优先级, 处理完成后轮转

![ocw2-bool](http://img.phanium.top/20230420135435.png)

OCW3: 设置特殊屏蔽方式 和 查询方式
- ESMM SMM enable special mask mode
    - 启动/禁用 特殊屏蔽模式
- P poll command
- RR + RIS: 选择读 ISR or IRR

![ocw3](http://img.phanium.top/20230420135536.png)


每个 8259A 只分配 2 个端口, 但要供 7 个 reg 依次使用
- ICW1 OCW2 OCW3 用 0x20 0xa0
- ICW2 ICW3 ICW4 OCW1 用 0x21 0xa1
- 因此 reg 和端口不总是 1 对 1 的关系, 一个 port 可以对应多个 reg, 而一个 reg 也可以对应多个 port
- 本质在于: 端口不是一般意义的内存(可以无限读取), 而属于一种生产--消费模型

从片不止有一个, 要则怎么识别呢?
- 前面已经回答了, 主片和 CPU 不用识别
- 但只给了 0xa0 端口...
- 那么到底怎么分别设置不同的从片呢?

编程方法论
- 写 ICW1, ICW2 来初始化
- 如果级联, 那么写 ICW3
- 如果 IC4, 那么写 ICW4

### CPU 的中断处理

CPU 中断处理 (e.g. 特权级 3 -> 0)
- 压栈 ss esp, eflags, cs eip
- 如果中断有 error code, 也压栈
    - 不是所有的中断都有 error code

### 时钟

时钟只是同一设备内的节奏, 不同的设备可能有不同的时钟
- 内部时钟: 纳秒级, 晶振(晶体振荡器), 位于主板上, 分频后就是主板的外频 (处理器和南北桥)
- 外部时钟: 毫秒级, 处理器和外设/外设和外设
- 内外的桥梁: IO 接口/ IC
    - 定时计数器: 软件(空转)/ 硬件实现
    - 正计时/ 倒计时

定时器 8253
- 三个相同的独立计数器(通道)
    - 时钟信号/ DRAM 定时时钟信号/ 扬声器声调
- 计数器结构: 初值寄存器 减法计数器 输出锁存器
- 引脚
    - CLK 频率, 时钟源
    - GATE 门控, 是否计数
    - OUT 每次计数值为 0 就输出
- 控制字寄存器
    - 选择计数器 (三个计数器共用)
    - 选择读写方式
    - 选择工作方式
    - 选择数制格式
- 计数的发生条件 (数电知识)
    - 硬件条件: GATE 高电平
    - 软件条件: 初值写入减法计数器(out 指令)
    - 发生: CLK 下降沿
- 工作方式
    - 软件启动, 硬件启动
    - 强制终止, 自动终止

[控制字](http://img.phanium.top/20230513150502.png)

六种工作方式
- 计数结束中断方式
    - 对8253 任意计数器通道写入控制字,  都会使该计数器通道的 OUT变为低电平,
- 硬件可重触发单稳方式
[六种工作模式](http://img.phanium.top/20230513144917.png)

输出频率: 由 初值控制
结论就是 1193l80/中断信号的频率=计数器0的初始计数值

初始化步骤:
往控制字寄存器端口 0x43 中写入控制字
在所指定使用的计数器端口中写入i十数初值

若初值是8位,  直接往i十数器端口写入即可.  若初值
为l6位, 必须分两次来写入,  先写低8位' 再写高8位.


学习 8253的目的就是为了给 IRQ0 引脚上的时钟中断信号 ′′提这驾

要采取循环计数的工作方式

要为计数器0赋予台弓置的计数初值.


## 内存管理系统

### utiltity

makefile
- make 如何判断时间?
    - atime: access time, 读取文件内容时修改, cat less 都会更改, 但 ls 不会
    - ctime: change time 文件属性或数据修改
    - mtime: modify time 文件数据修改
- make 定义的系统级变量: 用于 隐含规则
    - 注意 CXXFLAGS CPPFLAGS
    [make-sysvar](http://img.phanium.top/20230513154945.png)
- 自动化变量
    ```
    $@: all target
    $<: first dep
    $^: all deps
    $?: all newer mtime deps
    ```
- 模式匹配
    ```
    % 任意非空字符
    dep 的 % 会复刻 target
    g%.o
    ```

assert 断言 要点
- 两种断言: 内核级 + 用户级
- 关中断: 在输出报错信息时 屏幕输出不应该被其他进程干扰

实现一些库函数
- string
    - memset memcpy memcmp
    - strcpy strcmp strcat
    - strchr strrchr strchrs
- bitmap
    - get set init scan

### 内存管理系统

概述
- 隔离内核和用户的分配
    - 将物理内存划分成两部分: 一部分只用来运行内核, 另一部分只用来运行用户进程
    - 用户物理内存池 + 内核物理内存池
    - 方便实现, 咱们把这两个内存池的大小设为一致
- 粒度: 从内存池中获取的内存大小至少为 4KB 或者为 4KB 的倍数
- 内核有能力不通过内存管理系统申请内存, 但让内核也通过内存管理系统申请内存
- 内存管理系统所管理的是那些空闲的内存, 所以不管 已使用的 (位于低端 1MB 之内的..)

先分配虚拟内存池, 再分配物理内存池
[addr-pool](http://img.phanium.top/20230514110337.png)

只要保证PCB 的起的地址是自然页就好?
- 0xc009f000 esp
- 0xc009e000 pcb for main
- 0xc009a000 bitmap
- 0xc0100000 heap

和 pool 相比,  virtual_addr 中没有 pool_size 成员
- 尽管虚拟地址空间最大是 4GB, 但相对来说是无限的, 不需要指定地址空间大小.

malloc_page
- 分配 vpage
- 分配 ppage
- 修改 page table
    - PDE 和 PTE


## 线程

其实处理器原生支持 "任务" 并提供了相关的机制
- 比如 TSS 结构和任务门

其实任何代码块都可以独立, 只要在它运行的时候,  我们给它准
备好它所依赖的上下文环境就行,

调度单元 调度器

进程与线程的关系是进程是资源容器, 线程是资源使用者.

要回答的问题 (PCB: Process Control Block)
- 调度器从哪里找任务 -- OS 维护 PCB 表
- 任务所需要的资源从哪里获得 -- PCB 寄存器
- 调度器如何进行调度
    - 进程应该运行多久呢 -- PCB 时间片
    - 任务的资源存放在哪里 -- PCB 寄存器
    - 进程被换下的原因是什么? 下次调度器还能把它换上处理器运行吗 -- PCB 进程状态
- 进程独享地址空间 它的地址空间在哪里 -- PCB 页表

栈 栈指针?
进程使用的栈也属于 PCB 的一部分
不过此栈是进程所使用 的 0 特权级下内核栈 〈并不是3特权级下的用户栈〉
PCB 一般都很大. 通常以页为单位

"寄存器映像" 的位置并不固定

进程或线程被中断时, 处理器自动在 TSS 中获取内核栈指针
通常是 PCB 的顶端

API: 库函数和操作系统的系统调用之间的接口
只要操作系统和 应用程序都遵守同一套ABI规则,  编译好的应用程序可以无需修改直接在另一套操作系统上运行

callee 应该保存好 ebp ebx edi esi esp (属于 caller)

ABI 规则限定的是汇编(二进制)的规则?
c 编译器需要遵守一套 ABI, 意味着生成的汇编都是 按照 ABI 规则
也得按照ABI 的规则去写汇编才行.

![thread-tag-list](http://img.phanium.top/20230516112406.png)

## IO 系统

naive print 问题
- 有些字符串看似少了字符
- 大片空缺
- GP 异常

分析 print 的原理
- 先获取 cursor 的 pos
- 将 pos 转化为 addr, 写字符到 addr
- 更新 cursor 的 pos

如果执行完 1, 没执行 3, 调度发生
- 可能 少字符/覆盖/大片空缺?

### 实现锁

信号量
- 信号灯
- V up: release lock
    - signal++ and weak thread
- P down: get lock
    - if signal > 0, signal--
    - if signal = 0, wait


如何 wait/weak (block/unblock)
- wait: 线程主动暂停自己的执行
    - 改 PCB status, 然后直接调度
- weak: 其他线程来唤醒
    - 加到 rdy list 里


### 终端 IO

终端输出
- alt f1 的本质: 改变 显存地址
    - 切换不同的 tty 进程 (who 指令查看)
- 我们不需要多个终端
- 但是我们把终端当成设备来对待


终端输入
- 键盘分类: PS/2, USB, bluetooth
- PS/2, 按键 -> 键盘编码器(键盘内部, inter 8048)  -> 报告 scancode 向键盘控制器(主板 intel 8042)
![8042-8048-8059A](http://img.phanium.top/20230517133137.png)

扫描码 scancode?
- 按键按下: 通码 makecode (接通内部电路)
- 按键松开: 断码 breakcode (断开内部电路)
- 键盘中断处理程序: 处理 scancode
- scancode != ascii
- 规范: scan code set 1, scan code set 2, scan code set3
    - 随着键盘的发展而扩充?
    - XT 键盘 -> AT 键盘 -> PS/2
- 不管键盘用的是何种键盘扫描码, 8042 永远只报告第一套标准: 作为兼容层
- 大多数惰况下 第一套扫描码中的通码和断码都是 1 字节大小
    - 通码 + 0x80 = 断码
    - 最高位也就是第 7 位的值决定按键的状态
    - 第二套: 一般的通码是 1 字节, 断码是在通码前再加 1 字节的 0xf0
    - 并不是一种键盘就要用一套键盘扫描码: extend, 左 alt (0x38 0xb8)和 右 alt (0xe0 0x38 0xe0 0xb8)

过程
- 8048 -> 8042
- 8042 按字节处理, 每处理一个字节, 存储到自己的输出缓冲区寄存器
- 向中断代理 8059A 发中断信号
    - 右 alt, 每按一次将产生 4 次中断
- 中断处理程序 读取 8042 的 输出缓冲区寄存器

![scancode](http://img.phanium.top/20230517140202.png)

8042
8042 存在的目的是=
为了处理器可以通过它控制 8048 的工作方式
然后让 8048 的工作成果通过 8042 回传给处理器
8042 就相当于数据的缓冲区,  中转站,
- 寄存器 (cpu 眼中的)
    - 输出缓冲区: cpu 读 scancode, 8048 的命令应答, 8042 的应答,
        - 8042  通过状态寄存器记录有无内容物, 键盘中断处理程序中必须要用 in 读取输出, 否则 8042 无法响应键盘操作
    - 输入缓冲区: cpu 写 控制命令, 参数
    - 状态寄存器:
    - 控制寄存器:


### 键盘驱动
> 驱动就是 cpu 操纵其他硬件的 手段

通过简单的 加一个 handler, consume 掉 8042 里的东西, 就能做到连续输出的效果

硬件级别的驱动, 只暴漏 IO 接口
进一步完善 handler, 实现复杂的软驱


不可见字符的实现
转义字符: 谁来解释? compiler
设备只看到 字节

```bash
echo -en "\046"
echo -en "\x26"
```

### 环形缓冲区

shell 需要,

## 用户进程

Linux 任务切换未采用 Intel 的做法, 只是用了 TSS 的一小部分功能
虽说厂商才是功能提供者, 但需求方才是促进硬件发展的原因和动力

### LDT 和 TSS
> 在项目中并不会用到 LDT, 仅作为时代的眼泪

LDT 和 GDT
- GDT 是全局唯一的结构, 它的位置是固定且己知的
    - lgdt "16位内存单元" 或  "32 位内存单元"
- LDT 属于任务私有的结构, 每个任务都有的, 位置自然就不固定
    - "为每个程序单独赋予一个结构, 来存储其私有的资源" (代码/数据隔离)
    - LDT 描述符, 存储在 GDT 里
- LLDT "16 位通用寄存器" 或 "16 位内存单元"
    - 操作数是 LDT 在GDT中的选择子
    - 先用选择子拿到 LDT 的地址, 然后再加载到 LDTR, 可套用介绍过的 引用段描述符时的特权级检查

![LDTR](http://img.phanium.top/20230518154123.png)
- 一个任务最多可定义 8192 个内存段, 如果任务是用 LDT 来实现的话 最多可同时创建 8192 个任务.
- 每加入一个任务: 需要在 GDT 中添加新的 LDT 描述符
- 切换任务: 要重新加载 LDTR

任务切换机制 的 硬件支持
- 采取轮流使用 CPU 的方式运行多任务, 当前任务在被换下 CPU 时, 任务的最新状态也就是寄存器中的内容, 应该找个地方保存起来,
- Intel 的建议是给每个任务 "关联" 一个任务状态段, 这就是 TSS' (Task State Segment).
    - 称为 "关联",  是因为 TSS 是由程序员 "提供" 的,  由 CPU 来维护
    - TSS 是程序员为任务单独定义的一个结构体变量
    - CPU 自动用此结构体变量保存任务的状态, 自动从此结构体变量中载入任务的状态

任务切换机制 的 软件实现
- "在 cpu 眼里" 任务切换的实质就是 tr 寄存器指 向不同的 tss, 而这只是cpu 的美好愿景, linux 并未这么做
- 人类理解的任务切换, 就是让 CPU 执行不同任务的代码段中的指令
- 说白了就是让 cpu 的cs:[e]ip 指向不同任务的代码, 即使是所有任务共享 一个TSS 也无所谓, 这就是 linux 的作法

![tss](http://img.phanium.top/20230518170335.png)
- B 位是由 CPU 来维护的, 不需要咱们人工干预
- B 表示 busy 位, B 位为 0 时, 意义不是单纯为了表示任务忙不忙, 是说任务是不可重入的
- 原因是如果任务可以自我调用的话就混乱了
    - 在同一个 TSS 中保存后再载入, CPU 会自动把老任务的 TSS 选择子写入到新任务 TSS 中的 "上一个任务的 TSS 指针" 字段中
    - 如果任务重入的话,  此链则被破坏
- 并不是只有当前任务的 B 位才为 1
    - 当前任务通过 call 指令嵌套调用的新任务 TSS 的 B 位会被置为 1 , 老任务 TSS 的 B 位不会被清 0

嵌套任务调用的情况还会影响 eflags 寄存器中的 NT 位

![tss](http://img.phanium.top/20230518171250.png)
除了从中断和调用门返回外, CPU 不允许从高特权级转向低特权级

![tss-ldt-gdt](http://img.phanium.top/20230518172807.png)
- ltr 和 lldt

任务切换的实现
- 换下任务: 由 CPU 自动地把当前任务的资源状态 保存到该任务对应的 TSS 中
- 新任务: CPU 通过 TSS 选择子加载新任务时, 会把该 TSS 中的数据载入至 CPU 的寄存器中, 同时用此 TSS 描述符更新寄存器TR.

### 硬件原生任务切换机制

提供了 LDT 及 TSS 这两种原生支持, 要求为每一个任务分别配一个 LDT 及 TSS
- LDT 中保存的是任务自己的实体资源, 也就是数据和代码
- TSS 中保存的是任务的上下文状态及三种特权级的栈指针, IO 位图等信息

#### 中断 + 任务门

线程切换中用的就是时钟中断
- 中断发生时, 处理器一定会通过中断向量号检索 IDT 中的描述符
- 所以 若想通过中断的方式进行任务切换, 该中断对应的描述符中必须要包含 TSS 选择子, 而唯一包含 TSS 选择子的描述符便是任务门描述符

![任务门](http://img.phanium.top/20230518174840.png)

中断描述符表中的任务门
- 一个完整的任务包括用户空间代码及内核空间代码
- 只要 TR 寄存器中的 TSS 信息不换, 无论执行的是哪里的指令, 也无论指令是否跨越特权级(从用户态到内核态), CPU 都认为还是在同一个任务中.
- iretd 一个指令在不同环境下具备不同的功能
    - 从中断返回到当前任务的中断前代码处
    - 当前任务是被嵌套调用时, 它会调用自己TSS 中 "上一个任务的 TSS 指针" 的任务

NestTask Flag
- 中断发生时 处理器要把NT 和 TF 置为零
- 如果 对应中断门描述符 IF 置零啊(避免中断嵌套)
- retd 退出中断时这些值会被恢复

通过任务门进行任务切换的过程
- 获取新任务的 TSS: 任务门描述符 -> 任务的 TSS 选择子 -> TSS 描述符
- 保存旧任务的 TSS: TR 中获取旧任务的 TSS 位置 保存旧任务的状态到旧 TSS
- 加载, 切换到新任务
    - 把新任务的 TSS 中的值加载到相应的寄存器中
    - 更新 TR, 指向新任务的 TSS
    - 将新任务的 TSS 描述符中的出 B 位置 1
    - 将新任务标志寄存器中 NT 位置 1
    - 将旧任务的 TSS选择子写入新任务 TSS 中 "上一个任务的 TSS指针′′ 字段中
- 开始执行新任务

旧任务 TSS 描述符中的B 位为 1
在调用新任务后也不会修改 因为它尚未执行完, 属于嵌套调用别的任务, 并不是单独的任务

iretd 指令返回到旧任务的过程
- 此时处理器检查 NT 位 若其值为 1, 便进行返回工作
- 将(当前任务)新任务 标志寄存器中 NT 置0
- 将当前任务 TSS 描述符中的 B 位置 0
- 获取当前任务 TSS 中 "上一个任务的TSS指针" 字段的值, 将其加载到 TR 中
- 执行上一个任务, 从而恢复到旧任务

#### call jmp 切换任务

任务门描述符除了可以在 IDT 中注册, 还可以在 GDT 和 LDT 中注册
- 只要包括 TSS 选择子的对象都可以作为任务切换的操作数
- 因此另一种切换任务的方式是用 call/jmp TSS 选择子/任务门选择子

任务门选择子, GDT 中第 2 个描述符
call 0x0010:0x1234
TSS 选择子, GDT 中第 3 个描述符位置
call 0x0018:0x1234
偏移量 0x1234 都会被处理器忽略

`call 0x0018:0x1234` 任务切换的步骤
- 0x0018 在GDT 中索引到第 3 个描述符
- 检查描述符中的 P 位,  若 P 为 0, 表示该描述符对应的段不存在, 这将引发异常. 同时检查该描 述符的 S 与TYPE 的值,  判断其类型, 如果是TSS 描述符,  检查该描述符的B位,  B位若为l将抛出 GP 异常,  即表示调用不可重入
- 特权级检查,  数值上 "CPL 和 TSS 选择子中的RPL" 都要大于等于 TSS 描述符的 DPL,否 则抛出 GP异常
- 特权检查完成后, 将当前任务的状态保存到寄存器TR指向的TSS 中.
- 加载新任务 TSS 选择子到TR寄存器的选择器部分,  同时把 TSS 描述符中的起始地址和偏移量 等属性加载到TR寄存器中的描述符缓冲器中.
- 将新任务 TSS 中的寄存器数据载入到相应的寄存器中, 同时进行特权级检查,  如果检查未通过, 则抛出 GP异常.
- 把新任务的标志寄存器 e且ags中的NT位置为 l.
- 将旧任务 TSS 选择子写入新任务 TSS 中的字段"上一个任务的 TSS 指针" 中,  这表示新任务是被 旧任务调用才执行的.
- 然后将新任务 TSS 描述符中的B位置为 l 以表示任务忙.  旧任务 TSS 描述符中的B位不变,  依 然保持为l, 旧任务的标志寄存器 enags 中的NT位的值保持不变,
- 开始执行新任务,  完成任务切换.

jmp指 令以非嵌套的方式调用新任务,  新任务和旧任务之间不会形成链式关系
旧任务 TSS 描述符中的B位会被 CPU 清0.

### mordern os 的的任务切换

为什么不用 硬件专门支持的机制?
- 效能很低
- CISC 固然强大, 但需要更多的时钟周期作为代价
- 只是开发效率上的提升,  执行效率却下降了,
- 一个任务需要单独关联一个 TSS, TSS 需要在 GDT 中注册
- 随着任务的增减, 要及时修改 GDT, 修改过后还要重新加载 GDT


使用 TSS 唯一的理由
- 为 0 特权级的任务提供栈(这个机制另一方面也是不得不用的, 因为和中断 stack switch 捆绑在一起)
- ring3 -> ring0: 处理器从当前 tss 的 ss0 和 esp 0 成员中获取用于处理中断的堆栈
- 因此, 我们必须创建一个tss, 并且至少初始化 tss 中的这些字段
    - 之前一直没用, 因为 CPL 一直在 ring0

看看Linux 是怎样做的
- 各 CPU 的 TR 寄存器保存各 CPU 上的 TSS
- 用 LTR 指令加载 TSS 一次后, 之后再也不会重新加载, 该 TR 寄存器永远指向同一个TSS
- 只是换了 TSS 中的部分内容, 而 TR 本身没换
    - 和修改 TSS 中的内容所带来的开销相比, 在 TR 中加载 TSS 的开销要大得多
- 另外, Linux 中任务切换不使用call和jmp 指令


## 系统调用

### 原理
中断门实现系统调用 (int 0x80 和 syscall 库函数)
- IDT 添加对应 0x80 的描述符, 指向自定义的 `syscall_handler`
- kernel.S 中实现 `syscall_handler`, 作为入口, 进一步跳转到 `syscall_table`
- syscall-init.c 维护 `syscall_table`, 负责实现 ring0 的 syscall 实现
- syscall.c 作为 ring3 的 wrapper, 用 eax 索引对应 handler
- macro 实现用户接口

### printf = write + vsprintf

变长参数
- gcc 内置的功能
```c
#include <stdarg.h>

// 使得 ap 指向
void va_start(va_list ap, last);
// 检索函数参数列表中类型为 type 的下一个参数
type va_arg(va_list ap, type);
//
void va_end(va_list ap);
void va_copy(va_list dest, va_list src);
```

printf = write + vsprintf
```c
uint32_t printf(const char* str, ...) {
  va_list args;
  va_start(args, format);
  char buf[1024] = {0};
  vsprintf(buf, format, args);
  va_end(args);
}
uint32_t vsprintf(char* str, const char* format, va_list ap);
```

### 堆内存分配 malloc

arena 机制
- 一大块内存, 被划分成无数小内存块的, 内存仓库
- 不同粒度:  16 字节/32 字节...
- 根据 malloc 申请的内存大小来选择不同规格的内存块

malloc 的过程
```c
void* sys_malloc(uint32_t size) {

    struct mem_block_desc* descs;

    enum pool_flags PF;
    struct pool* mem_pool;
    uint32_t pool_size;

    struct task_struct* cur_thread = running_thread();

    // kernel or user
    if (cur_thread->pgdir == NULL) {
        PF = PF_KERNEL;
        pool_size = kernel_pool.pool_size;
        mem_pool = &kernel_pool;
        descs = k_block_descs;
    }
    else {
        PF = PF_USER;
        pool_size = user_pool.pool_size;
        mem_pool = &user_pool;
        descs = cur_thread->u_block_desc;
    }

    // invalid size
    if (!(size > 0 && size < pool_size)) {
        return NULL;
    }

    struct arena* a;
    struct mem_block* b;

    lock_acquire(&mem_pool->lock);

    // large block -> whole page
    if (size > 1024) {
        uint32_t page_cnt = DIV_ROUND_UP(size + sizeof(struct arena), PG_SIZE);

        a = malloc_page(PF, page_cnt);

        if (a != NULL) {
            memset(a, 0, page_cnt * PG_SIZE); // TODO: or not?

            // the header
            a->desc = NULL;
            a->cnt = page_cnt;
            a->large = true;

            lock_release(&mem_pool->lock);
            return (void*)(a + 1); // after header
        }
        else {
            lock_release(&mem_pool->lock);
            return NULL;
        }
    }
        // small block -> adaptive
    else {
        uint8_t desc_i;

        // find the first 2^k > size
        for (desc_i = 0; desc_i < DESC_CNT; desc_i++) {
            if (size <= descs[desc_i].block_size) {
                break;
            }
        }

        // if no free block in current arena
        //      create a new arena mem_block
        if (list_empty(&descs[desc_i].free_list)) {

            a = malloc_page(PF, 1); // new arena
            if (a == NULL) {
                lock_release(&mem_pool->lock);
                return NULL;
            }
            memset(a, 0, PG_SIZE);

            a->desc = &descs[desc_i]; // common desc
            a->large = false;
            a->cnt = descs[desc_i].blocks_per_arena;

            uint32_t block_i;

            enum intr_status old_status = intr_disable();

            // divided arena into small blocks
            // update free_list (must iterate to fill the free_list... seems dummy)
            for (block_i = 0; block_i < descs[desc_i].blocks_per_arena; block_i++) {
                b = arena2block(a, block_i);
                ASSERT(!elem_find(&a->desc->free_list, &b->free_elem));
                list_append(&a->desc->free_list, &b->free_elem);
            }

            intr_set_status(old_status);
        }

        // time to alloc
        //      alloc: list_tag -> block
        b = elem2entry(struct mem_block, free_elem, list_pop(&(descs[desc_i].free_list))); // type -> namae -> target
        memset(b, 0, descs[desc_i].block_size);
        //      update the arena header: block -> arena
        a = block2arena(b);
        a->cnt--;

        lock_release(&mem_pool->lock);
        return (void*)b;
    }
}
```
- 思想
    - arena 针对不同大小的块, 有两种语义
    - 每个 arena 有自己的单独部分
        - 比如 cnt 来计数是否满了(对小块)
    - 每个 arena 也有 common 的 desc 来节约空间
        - comon desc 来记录 arena 的规格, 和该类 arena 的 free_list
- 对于大块的申请: 直接按整页分配 malloc_page
    - 因为页内会记录 arena header, 对超过 2K 的块, 一个 page 至多只能装一个块, 页内用 arena 去 trace 一个就 naive 了
    - 因此 arena 在该语境下的含义为: 使用 cnt 去计数用到的 page 数量, 同时不需要 common 的 desc 头去记录 arena 的具体规格
    - 而实际分配的大小为
    ```c
    size + sizeof(struct arena)
    ```
- 对于小块的申请: 先确定所属 arena 的粒度, 然后分配一个固定粒度的块
    - 先预处理(e.g. 只允许 2 的幂次大小), 归约到确定的某类 arena 粒度
    - 然后去相应 arena 的 desc 中寻找空闲块
    - 如果没有空闲块(初始情形/或当前所有的 arena 都满了), 则申请一 page 作为新的 arena
        - 然后更新 desc 的 free_list
    - 如果有空闲块, 就直接弹 free_list, 并将 list_tag 转换得到一个 block
        - 然后更新 对应 arena 的 header


free 的过程
```c
void sys_free(void* ptr) {
    ASSERT(ptr != NULL);

    if (ptr != NULL) {
        enum pool_flags PF;
        struct pool* mem_pool;

        // kernel thread or user process
        if (running_thread()->pgdir == NULL) {
            ASSERT((uint32_t)ptr >= K_HEAP_START);
            PF = PF_KERNEL;
            mem_pool = &kernel_pool;
        } else {
            PF = PF_USER;
            mem_pool = &user_pool;
        }

        lock_acquire(&mem_pool->lock);

        struct mem_block* b = ptr;
        // get the meta info of blk
        struct arena* a = block2arena(b);
        ASSERT(a->large == 0 || a->large == 1);

        // large blk
        if (a->desc == NULL && a->large == true) {
            mfree_page(PF, a, a->cnt);
        }
        // small blk
        else {
            // free list
            list_append(&a->desc->free_list, &b->free_elem);

            // arena has been empty?
            // clean the free_list (seems a little dummy...)
            //      if alloc a new blk, we remove free_list
            //      if free the arena, we also remove free_list
            if (++a->cnt == a->desc->blocks_per_arena) {
                uint32_t block_i;
                for (block_i = 0; block_i < a->desc->blocks_per_arena; block_i++) {
                    struct mem_block*  b = arena2block(a, block_i);
                    ASSERT(elem_find(&a->desc->free_list, &b->free_elem));
                    list_remove(&b->free_elem);
                }
                mfree_page(PF, a, 1);
            }
        }
        lock_release(&mem_pool->lock);
    }
}
```
- 对于大块, 直接释放对应的几页即可(因为没有使用 desc)
- 对于小块
    - 添加回 common desc 的 free_list 中
    - 并修改对应 arena 的计数
        - 从 block 指针得到 arena header
    - 如果计数满了(arena 完全空闲), 直接释放掉这个 arena
        - 需要完全删掉这个 arena 在 free_list 的所有 block
        - 此外, 这里也可以用别的释放策略

## 硬盘驱动

```c
ata0: enabled=1, ioaddr1=0x1f0, ioaddr2=0x3f0, irq=14
ata0-master: type=disk, path="hd60M.img", mode=flat, cylinders=121, heads=16, spt=63
ata0-slave: type=disk, path="hd80M.img", mode=flat, cylinders=161, heads=16, spt=63
```
- 硬盘容量 = 单片容量 x 磁头数
- 单片容量 = 每磁道扇区数 x 磁道数(柱面数) x 扇区大小
- 80M = 83607552B = (63 x 162 x 512) x 16
- 柱面数 x 磁头数 = 硬盘容量 / 63 / 512

### DPT
DPT 分区的历史
- 早期: 限制 4 个分区
    - 想法: 一台机器上顶多安装 4 个操作系统, 各占 1 个分区
    - 64 字节的分区表总共可容纳 4 个表项, 必须存在于 MBR 引导扇区或 EBR 引导扇区中
- 兼容, 发展: 区分拓展分区和主分区
    - 描述符增加 id 属性 5
    - 该分区(拓展分区)可被再次分出更多的子分区(逻辑分区)
    - 4 个分区中至多一个拓展分区, 但拓展分区理论无限可分
    - 限制转为硬件: ide 支持 63 个, scsi 支持 15 个


DPT: disk partition table
![dpt](http://img.phanium.top/20230525162308.png)
- DPT 位于 MBR 中, 16 x 4
    - 记录各个分区的 类型, 从哪开始, 到哪结束, 容量等
- id = 5 类型的分区是 拓展分区, 可进一步再分为多个子拓展分区
![brbr](http://img.phanium.top/20230407223005.png)
- 每个(包括首个)子拓展分区 起始是 EBR, 也存储 DPT, 但语义不同于 MBR 的 DPT, 只用两个表项
    - 第一分区表项: 描述所包含的逻辑分区的元信息(OBR)
    - 第二分区表项: 描述下一个子扩展分区的地址(EBR)

MBR EBR OBR
- MBR 和 EBR 是不归 DPT 管辖的
- MBR 是总分区划分的元数据
- EBR 是一个具体的子拓展分区的元数据 (实际不可再分)
- OBR 为数据分区的起始, OBR 中没有分区表

DPT 的分区起始偏移扇区
- 方便期间, 把扇区当 LBA 地址的粒度
- 该偏移地址 + 基准的绝对地址 = 分区的绝对地址
    - MBR DPT 中(主分区或总扩展分区): 基准为 0
    - EBR DPT 第一项(逻辑分区): 基准是子扩展分区的起始扇区 LBA 地址
        - (也就是上一个子扩展分区中 EBR 记录的下一个子扩展分区的偏移扇区)
    - EBR DPT 第二项(下一个子扩展分区): 基准是总扩展分区的起始扇区 LBA 地址
        - (也就是 MBR 中记录的扩展分区的偏移扇区)
- 可参考: https://zhuanlan.zhihu.com/p/388384142
![dpt-off](http://img.phanium.top/20230525190423.png)

磁盘命名的规则: `/dev/[x]d[y][n]`
- type, disk, dev_no, par_no
- ide: /dev/hda, scsi: /dev/sda
- 比如: 这里让主分区占据 sdb[1~4], 逻辑分区占据 sdb[5~]

磁盘的块读, 块写
- 虽然, 到头来还是老一套的寄存器读写
    - 选磁盘, 选扇区, 写命令, 轮询, 读写磁盘 (字节搬运)
- 但不同的是, 我们一在 OS 上写程序, 有中断机制和进程隔离, 如果进程还是使用轮询就是一种倒退
- 实现进程的 磁盘 I/O, 首先要用锁来防止竞争, 其次可以将轮询磁盘状态替换为磁盘中断通知

分区扫描

## 文件系统

inode
![inode](http://img.phanium.top/20230526102751.png)

layout
![layout](http://img.phanium.top/20230526103359.png)

file descriptor
- 进程视角: 拿到 fd, 就拿到文件
- OS: 用 fd 检索运行时的文件信息 (off, flag, inode)
- 进程有独立的 "fd table", 存在 PCB 里
    - 为减少 pcb 的空间占用, local table, 只负责映射到 global table fd
    - global table 在 kernel 里面 (这个 os 是在固定的栈内存里)
- 如果已经拿到 fd 那么
    - local fd -> pcb 映射表 -> global fd
    - global fd -> global 文件表 -> inode

![fd-inode](http://img.phanium.top/20230526163245.png)

fs 的初始化
- 首先, 确保 fs 的存在: 分区完成格式化并挂载好
    - 此时内存中应可以访问到 inode bitmap, block bitmap
    - 这一步只需要 磁盘 I/O 的 API 即可
- 其次, root dir 内容应该是可读取的, 毕竟这是后续文件系统搜索的基础
    - 我们在格式化时, 事先约定好 root dir 的 inode no 为 (e.g. 0)
    - 只需一个 inode no -> inode 的 API: `inode_open`
- 为了降低磁盘 I/O 频率, 容易想到维护一个 inode 的缓存 cache
    - `inode_open` 会先从 cache list 中查找 inode, 失败才读盘
    - inode 的 cache list 应当是全局的, 因此维护在 kernel heap

现在基本预设具备了, 对于一个只需要正确性的文件系统, 剩下的就是体力活

防止思路混乱的方法, 就是自顶而下叙述实现, 其实系统编程的核心需求其实只有两个:
- 通过 filename 打开/创建文件, 得到 fd
- 通过 fd 对文件进行各种剩余操作

首先: 给定 filename + flag, 如何拿到 fd (open)
- 先判断 file 是否存在, 需要进行 file_search, 从 根目录开始循环: inode->内容->name 匹配->inode no->inode...
    - 需要实现: 路径解析, 目录打开, 目录内容读取解析和遍历
- 如果 file 不存在(且 flag 为 create), 就从磁盘进行 file_create, 最终拿到 inode no, 再当成存在处理
    - 涉及 inode 分配, 以及进一步的诸多同步包括: bitmap, file inode, file block, dir_entry block 
    - (必要时目录需要扩容, 我们总使用穷搜, 基本上是遇到空闲就扩容)
- 如果 file 已存在(且 flag 非 create), 只要用 inode_open 就能拿到 inode, 只需要建立必要的运行时映射即可
    - inode_open 后, 只需要建立两个映射: global file_table 和 pcb->table
- 同时, 注意对于 flag 不合法情况做出相应处理, 就算搞完了
- 总体来说至多三大步: file_search -> file_create -> file_open
- 数据流: filename -> inode no -> inode -> glojjjjjjjjjj

<!-- 其他操作 -->
<!-- - close: 清理 inode(inode_close), 清理 file table(字段置空), 清理 fd table(同样字段置空) -->
<!-- - write -->

文件的打开和关闭: sys_open, sys_close
- 看上去很简单, 实际上背后有很多细节
- sys_open: filename 到 fd
    - 用 filename 找到 inode no
        - 根目录必须是打开的, 或者能获取其内容, 同时需要逐级读取目录, 因此需要实现一个 dir_open, 根据 dir
        - 需要递归访问目录, 得实现 dir_open, dir_open 又依赖 inode_open
        - 
        遍历目录项, 直到完整的 filename 匹配 (最后类型可能还不匹配)
    - 用 inode no 得到 inode
        - 为了避免反复 disk I/O, 设计了 cache, 先从 cache list 中遍历
        - 没有 cahce, 再从 disk 里面找, 确定 inode 在磁盘的位置, kernel heap 里分配 inode 块, 然后 block read (inode 块可能还有跨 sectors 的问题)
    - 建立 global/local file table 的表项, local 只负责映射到 global, global 里面指向 inode 实体


## 用户程序

进程有什么
- PCB
- 程序体
- 用户栈 内核栈
- 页表 虚拟地址池
