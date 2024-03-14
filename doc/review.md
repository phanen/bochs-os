
总线地址 -> 内存地址 + 外设地址

bochs -> showint -> addr -> nm + grep -> symbol
bochs -> showint -> addr -> b -> sba -> asm code -> c code

为了突显"整合"之意, 硬盘控制器和硬盘终于在一起了, 这种接口便称为集成设备电路(Integrated Drive Electronics, IDE)
随着IDE 接口标准的影响力越来越广泛, 全球标准化协议将此接口使用的技术规范归纳成为全球硬盘标准, 这样就产生了ATA(Advanced Technology Attachment)

数据传输
* 无条件传送方式.
* 查询传送方式.
* 中断传送方式.
* 直接存储器存取方式(DMA).
* I/O 处理机传送方式
