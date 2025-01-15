# StreamIO Toolkit

ZYNQ Ultrascale+ PS端使用Xilinx DMA进行裸数据流传输的简易内核驱动和用户态库。


## 目录

- `driver`: 内核驱动模块
    - axi_ctrl: 寄存器映射驱动
    - stream_dma: DMA驱动
- `userspace`: 用户程序
    - test_app: 用户层读写DMA驱动
    - io_backend/io_client: 通过网络映射接口
- `lib`: DMA和寄存器操作库
- `scripts`: 脚本，主要用于连接开发板

## Credit

基于以下两个项目修改。

- ADI IIO
- Xilinx DMA Proxy 2.0
