```
ata0: enabled=1, ioaddr1=0x1f0, ioaddr2=0x3f0, irq=14
ata0-master: type=disk, path="hd60M.img", mode=flat, cylinders=121, heads=16, spt=63
ata0-slave: type=disk, path="hd80M.img", mode=flat, cylinders=161, heads=16, spt=63
```
* 硬盘容量 = 单片容量 x 磁头数
* 单片容量 = 每磁道扇区数 x 磁道数(柱面数) x 扇区大小
* 80M = 83607552B = (63 x 162 x 512) x 16
* 柱面数 x 磁头数 = 硬盘容量 / 63 / 512

## disk partition table
* 早期限制 4 个分区 (MBR-DPT 64 总共可容纳 4 项)
* 后来描述符增加 id 属性 5 的拓展分区, 可被再次分出更多的逻辑分区 (分区的新的瓶颈限制转为硬件: ide 支持 63 个, scsi 支持 15 个)

DPT 结构
![img:dpt](https://i.imgur.com/fdT8ixN.png)
![img:dpt](https://i.imgur.com/fdT8ixN.png)

* 位于 MBR/EBR 中, 16 x 4
* MBR-DPT: 每项记录各个分区的类型, 从哪开始, 到哪结束, 容量等
* EBR-DPT: 类似, 但只用分区表的两项 (因此 EBR 是链表结构)
  * 第一项: 描述所包含的逻辑分区的元信息 (OBR)
  * 第二项: 描述下一个子扩展分区的地址 (EBR)
* MBR 是总分区划分的元数据
* EBR 是一个具体的子拓展分区的元数据
* OBR 为数据分区的起始, 其中没有分区表

DPT 的分区起始偏移扇区 (方便期间, 把扇区当 LBA 地址的粒度)
* 该偏移地址 + 基准的绝对地址 = 分区的绝对地址
  * MBR DPT 中(主分区或总扩展分区): 基准为 0
  * EBR DPT 第一项(逻辑分区): 基准是子扩展分区的起始扇区 LBA 地址
    * (也就是上一个子扩展分区中 EBR 记录的下一个子扩展分区的偏移扇区)
  * EBR DPT 第二项(下一个子扩展分区): 基准是总扩展分区的起始扇区 LBA 地址
    * (也就是 MBR 中记录的扩展分区的偏移扇区)
* https://zhuanlan.zhihu.com/p/388384142
![dpt-off](http://img.phanium.top/20230525190423.png)

磁盘命名的规则: `/dev/[x]d[y][n]`
* type, disk, dev_no, par_no
* ide: /dev/hda, scsi: /dev/sda
* 比如: 这里让主分区占据 sdb[1~4], 逻辑分区占据 sdb[5~]

磁盘的块读, 块写
* 虽然, 到头来还是老一套的寄存器读写
    * 选磁盘, 选扇区, 写命令, 轮询, 读写磁盘 (字节搬运)
* 但不同的是, 我们一在 OS 上写程序, 有中断机制和进程隔离, 如果进程还是使用轮询就是一种倒退
* 实现进程的 磁盘 I/O, 首先要用锁来防止竞争, 其次可以将轮询磁盘状态替换为磁盘中断通知



