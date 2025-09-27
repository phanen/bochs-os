## register

* 可见: 通用寄存器 gpr, 段寄存器 sr
* 不可见: gdtr idtr ldtr tr cr dr ip flags
* 通用寄存器的专用 (约定接口)
  * accumulator base count data
  * source dest
  * stack/base pointer ...

cs, ip 为什么不能 mov
* 原子性需求: cs 改了, ip 没改, 寄
* jmp call int ret 同时修改 cs:ip, 保证原子性
* trick: 原地跳转来修改寄存器 flush pipeline
```
jmp 0x90:xx
```

段超越前缀
* 对于堆栈和代码段, ss cs 是死的, 你只能用他们访问
* 但数据段的段前缀是可以显示更改的, 只是默认是 ds

## addressing

寻址实际是得到操作数的过程
> 寻址不是访存, 寻址不是得到内存的地址
* 寄存器寻址 `mul dx`
* 立即数寻址 `mov ax, 0x01`
* 内存寻址
  * 直接寻址 立即数作为访存地址
    * `mov ax, [0x1234]`
  * 基址寻址 寄存器作为起始地址
    * 实模式下只能用 bx bp
    * 保护模式下就没这个限制 (GPR)
    * `mov ax, [bx]`
  * 变址寻址 (实际上和基址寻址一样, 换了寄存器)
    * `mov [di], ax`
  * 基址变址寻址


## function & stackframe
* 任务可分为几个独立的子任务, 分配给不同的人(函数)
* 子任务之间具有独立性, 干活能互不打扰 (函数内部有独立的变量)
* 一个人能反复进行一项任务 (函数可重复调用执行)
* 子任务可继续再分

汇编里的函数
* 对象: cpu, 有限的 reg 和 mem
* api: 访存, 计算, 跳转

栈帧维护状态
* 函数需要返回 -> 返回地址是状态
* 函数需要传入值 -> 参数是状态
* 函数会调用子函数 -> 局部变量是状态
* 整个执行流的状态(变量/寄存器)用栈来维护
* 某个执行阶段, 上下文的状态的总和就是一个栈帧
* 当调用新的函数, 上下文发生改变, 需要压入栈帧
* FILO: callee 的栈帧在 caller 之下, callee 退出后可以恢复 caller 的数据状态

## calling conventions
* 作为 ABI 的一部分, 汇编层面函数的实现和使用标准

ebp (stack-base pointer)是额外引入用于维护栈帧寄存器
![ebp](http://img.phanium.top/20230408164233.png)
```asm
caller
  push argn
  push arg1
  push eip # call func
callee
  push ebp
  ebp = esp
  # 执行 (local)
  esp = ebp
  pop ebp
  ret # (pop eip)
caller
  pop arg
```

ebp 方便之处在于: 将栈表进一步实现为栈链表
* 把栈帧串成了链表, 可以做 backtrace
* ebp 之上为参数, 之下为本地变量, 方便调试
* ebp 便于恢复栈: 如 c99 支持变长数组, 比较难确定栈大小
  * <https://blog.51cto.com/e21105834/2952198>

注意: ebp 并不一定要这么用, gcc 可以通过 flag `-fomit-frame-pointer` 把 ebp 当成一般寄存器使用
* <https://stackoverflow.com/questions/3699283/what-is-stack-frame-in-assembly#answer-3700219>

## unconditional jump

实模式 call
```asm
# (direct?) relative near
call near func_name
# indirect absolute near
call [addr]

# direct absolute far -> retf
call (far) 0:far_proc
# indirect absolute far -> ref
call far [addr]
```
* direct/indirect: addressing mode
* near/far: 单纯修饰, 用 word dword 语义

实模式 jump
```asm
# relative short jmp (1 byte)
jmp $
jmp short short_fuc
# time db 127 0
# short_func:

# relative near jmp (2 byte)
jmp near short_fuc
# time db 128 0
# short_func:

# indirective relative near jmp
jmp near ax
# indirective absolute near jmp
jmp near [ax]

# directive absolute far jmp
jmp far 0000:0000
# indirective absolute far jmp
jmp far [addr]
#section call_test vstart=0x900
#jmp far [addr]
#times 128 db 0
#addr dw start, 0
#start:
#     mov ax,  0x1234
#     jmp $

```

## condition jmp


![img:flags](https://i.imgur.com/Iuxhk6Q.png)
* 8086+
  * carry flag
  * parity flag
  * auxiliary carry flag: 低4 位的进, 借位
  * zero flag
  * sign flag
  * trap flag: debug, 让 cpu 单步运行
  * interrupt flag: 屏蔽外部中断, 但不屏蔽 cpu 内部异常
  * direction flag: 字符串操作相关
  * overflow flag
* 80286+
  * input output privilege level
  * nest task (8088)
* 80386+
  * resume flag
  * virtual 8086 mode (向保护模式过渡的产物)
* 80486+
  * alignment check
* 80586+
  * virtual interrupt flag
  * virtual interrupt pending
  * identification

condition jmp
* jz/je jne/jne
* js jns
* jo jno
* jp/jpe
* jnp/jpo
* jcxz

flag
* above
* below
* carry
* equal
* great
* jmp
* less
* not
* overflow
* parity

## IO interface


ia32 PIO
* 给端口从 0 开始编号, 有一个端口号寄存器是16 位, 0-65535
```asm
in al, dx
# al = port[dx]

out dx, al
# port[dx] = al

out 123, al
# port[123] = al

// AT&T
in port, %al
in port, %ax
out %al, port
out %ax, port
```

### graphic card

打印
* 实模式下, mbr 能通过 0x10 BIOS 中断打印字符
* 保护模式下, 中断向量表就没了, 所以没法再用 BIOS 中断, 直接用显卡的 IO 接口编程


显卡: pci 设备
* 显存: 显卡的内存, 显卡的工作就是不断读取这块内存, 然后发送到显示器
* 显示器: 显卡可以设置 显示器的工作模式 图形模式/字符模式...
  * 显示器眼里的数据不分类型, 没有高级解析, 就是纯粹的像素信息
  * 只要有了显卡, 就没必要考虑背后的显示器的细节了

![img:graphic-mem](https://i.imgur.com/KdyyJTz.png)

显卡支持的模式
* 文本/黑白图形/彩色图形, 各自有不同的显存地址
* 文本模式下: 列数 * 行数, 默认 80*25=2000
* 每个字符 2 字节, 亮度+ascii+属性, 每屏共 4000 字节

### disk
* 1956 IBM 350 RAMAC 5MB
* 1968 winchester 技术, 磁头 盘面不接触
* 1973 IBM 3340 2*30MB
* SSD zookeeper

C H S 架构
* 柱面存取(为了减少寻道次数)
* 越往里越慢, 从外面开始存

IO 接口: 硬盘控制器
* 硬盘控制器和硬盘很久以前是分开的, 得益于: IDE 集成设备电路的 ATA 标准, 集成到一起
* Serial ATA 的出现, ATA 改名叫 PATA

PATA 接口线缆: IDE 线
* IDE 线上挂两块, 主盘(master) 和 从盘(slave)
* 主板支持 4 块IDE 硬盘, 就有两个接口/插槽: Primary 通道, Secondary 通道, 每个通道挂两块

硬盘控制器 PMIO
* 有复用/多用的情况, 节约成本和兼容目的
![img:disk-controler-pmio](https://i.imgur.com/5aN3gWI.png)
* `CtlBR[4]` -> 0 为主盘, 1 为从盘
* data: 读写 磁盘缓冲区 的数据
* error: 读取失败的信息,
* feature: 存储格外的命令参数
* sector count 待读取或写入的扇区数(号), 如果中间失败, 就表示尚未完成的扇区号
* LBA 28: lba-low + lba-mid + lba-high + device.low4
* device.high4: LBA 是否启用, 指定主盘 or 从盘
* status: 记录硬盘状态, error, data request, drdy, bsy
* command: 命令, identify, read sector, write sector...
![img:status-command](https://i.imgur.com/hJCrGAa.png)

准备工作 refer to ATA manual
* 选择通道, 写对应的 sector count 寄存器
* 选择地址, 写 LBA 地址, 设置 LBA 模式
* 输入指令, 写 command
* 获取硬盘状态, 读 status


传送数据的方式
* 无条件传送: reg 或 mem 就是这样, cpu 无脑直接取
* 查询传送 Programming I/O
  * 先检查设备状态, 再取, 比如要用程序去轮询
  * 一般是低速设备 (废话, 速度都比 cpu 低)
* 中断传送/ 中断 I/O
  * 直接用中断通知 CPU 取数据
  * CPU 要压栈保护寄存器现场 (保护现场 -> 传输 -> 恢复现场)
* 直接存储器存取 (DMA)
  * 传输不用 CPU 自己来做, 也就不用保护现场了, CPU 直接访内存拿数据就行了
  * DMA 是硬件实现, 需要 DMA 控制器
* I/O 处理机 传送
  * CPU 拿到数据后都不用去数据处理, 校验之类的操作, DMA 控制器更强大一点, 干脆这些工作也替 CPU 干了


