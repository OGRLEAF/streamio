# StreamIO Userspace SDK

StreamIO的用户Linux库、SDK及说明文档。


## 目录结构

```
.
├── driver                  # 内核驱动
│   ├── axi_ctrl            # AXI寄存器读写驱动
│   ├── stream_dma          # DMA驱动
│   └── stream_dma_enhanced
├── lib                     # StreamIO操作库
│   ├── bbio
│   ├── buffer
│   ├── include             # 头文件
│   │   ├── bbio
│   │   ├── buffer
│   │   └── utils
│   └── utils
├── scripts                 # 常用脚本
└── userspace               # 用户应用软件
    ├── io_backend
    ├── io_client
    └── test_app            # 示例程序
```

## SD卡镜像和使用方法

将镜像写入SD卡后启动。

### 连接开发板

将开发板连接到有DHCP服务的局域网中，首先通过串口连接电脑，串口已经配置了自动登录。

使用串口连接开发板：

```bash
sudo tio /dev/ttyUSB3       # 一般是ttyUSB3，如果没有输出，尝试ttyUSB0、ttyUSB1……
```

使用`ip a`命令查看开发板的IP地址。

一般情况下，开发板能够自动通过DHCP关联自己的ip地址和主机名（`tq15egbase`），直接使用`tq15egbase`主机名就可以访问开发板。
但有时候也会出问题。

在`scrips/setup.sh`里，编辑`HOST=xxx.xxx.xxx.xxx`，设置开发板获取到的IP地址。

执行：

```bash
cd scripts
bash setup.sh
```

会自动生成SSH连接的密钥和配置。（如果提示`Overwrite (y/n)?`，建议输入`n`回车） 

配置完成后，将输出的最后一行内容粘贴至开发板串口并执行。

输出的最后一行内容如下例所示：
```bash
mkdir -p .ssh && echo "ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIHWVpj/2gD+j2MeGt6qNtIEl6TpnqNKPUHfrcCAAv/ML orgleaf@orgleaf-ThinkStation-K" > .ssh/authorized_keys
```

执行：
```bash
bash login.sh
```
登录到开发板。

## 下载FPGA和刷新驱动

因为FPGA工程有两个：带收发的验证工程和仅发送的射频工程。所以分别为两个工程分别设置了固件（bitstream和设备树）。

验证工程的固件：`/lib/firmware/xilinx/streamio`

射频工程的固件：`/lib/firmware/xilinx/adrv9002`

在`/home/root`目录下，准备了用于加载固件的脚本：`streamio-app.py`，脚本的用法如下：

```
root@tq15egbase:~# ./streamio-app.py --help
usage: StreamIO [-h] [-b BITSTREAM] [-o DTBO] [-m MODULE] [--firmware FIRMWARE] [--drv-probe] [--skip-pl] [--skip-drv] [--list-firmware] [--dry-run]

StreamIO driver and PL management

options:
  -h, --help            show this help message and exit
  -b BITSTREAM, --bitstream BITSTREAM
  -o DTBO, --dtbo DTBO
  -m MODULE, --module MODULE
  --firmware FIRMWARE   firmware (combined with PL and driver), default 'streamio', 4 available: streamio, openwifi-adrv9002, adrv9002, streamio.bin
  --drv-probe
  --skip-pl
  --skip-drv
  --list-firmware       list all firmware build in this image
  --dry-run
```

开机加载firmware：

```bash
./streamio-app --firmware adrv9002  # 加载射频固件
./streamio-app --firmware streamio  # 加载验证固件
```

### 重新编程FPGA

使用验证固件时，可以直接通过Vivado下载bitstream，下载完成后，在开发板上执行`./streamio-app.py --firmware streamio --skip-pl`（重新加载驱动，但不覆盖FPGA）

使用射频固件时，因为ADRV9002的驱动加载比较特殊，需要按照如下步骤编程FPGA：
1. 在`scripts/`目录下执行`bash deploy-pl.sh`，将新的bitstream上传到开发板
2. 执行`streamio-app.py --firmware adrv9002`，重新编程FPGA、重新加载驱动。


## DEMO使用说明

DEMO提供了一个包含基本读取文件、DMA传输和接收、写入文件的基本例子，用于演示将数据通过PL处理后再回收的过程。

DEMO的编译方法：在`example/`目录下执行

```bash
make && make install
```

如果网络配置正确，`make install`会自动将编译的程序上传至开发板。

在DEMO里，输入输出文件均为每行32bit十进制整数，`example/generate_test_file.py`提供了一个生成输入文件的示例。

DEMO的使用方法：

```bash
test_app -i test_wave_file.txt -o output.txt
```

### PL DEMO

PL的DEMO包含一个
RTL核和一个示例的数据处理和控制模块，将来自和输入DMA的AXIS数据包转换为连续的数据流。

数据流接口包括三个信号：

- xxx_strobe (xxx_valid) 主到从，数据有效信号
- xxx_data 主到从，数据
- xxx_ready 从到主，准备信号

若从模块准备好接受数据，将xxx_ready置为有效。

当有数据来临时，xxx_strobe（xxx_valid）变为有效，若此时xxx_ready也有效，则在时钟上升沿传递一次数据。

控制信号来自`baseband_sys_axi.v`，其中包含一系列32bit的寄存器。

## SDK使用说明

请参考example/src/tc_test.c

包含头文件的方式请参考`example/src/tc_test.c`

### 创建`io_context`

```c
#include <bbio_h.h>
```

所有API均已`io_*`开头。

可以通过AXI/AXI-Lite控制的设备称为`mapped_device`，AXIS读写的DMA称为`stream_device`，所有设备通过`io_context`结构体管理。

当程序直接在开发板上运行时，使用`void io_close_context(io_context *ctx);`直接创建一个`io_context`：

```c
io_context *ctx = io_create_context();
```

若程序在PC上运行，并通过网络连接开发板，则使用`io_context *io_create_net_context(char *host, uint16_t port);`

开发板上需要运行io_backend程序，程序启动后会输出网络信息，对应上面函数的参数`host`和`port`，例如：

```c
io_context *ctx = io_create_net_context("192.168.2.5", 12345);
```


执行`reload_fpga.sh`后，会创建`/dev/tc`、`/dev/streamio_rx_0`、`/dev/streamio_tx_0`，`/dev/streamio_tx_1`文件，对应PL中的一个AXI/AXI-Lite接口、一个DMA读AXIS接口、两个DMA写AXIS接口；

### `mapped_device`：

```c
io_mapped_device *map_dev = (io_mapped_device *)io_add_mapped_device(ctx, "/dev/tc");
```

通过以下函数读写设备的寄存器：

```c
uint32_t io_read_mapped_device(io_mapped_device *device, uint32_t addr);
void io_write_mapped_device(io_mapped_device *device, uint32_t addr, uint32_t value);
```

读写示例：通过寄存器向PL内的模块发送RESET、配置数据长度：
```c
// RESET累加器和FIFO
io_write_mapped_device(map_dev, 5, 3);

// 配置数据长度
io_write_mapped_device(map_dev, 6, TEST_SIZE);

// 释放RESET
io_write_mapped_device(map_dev, 5, 0);
```


### `stream_device`

添加`stream_device`：

打开一个接收通道：
```c
io_stream_device *dma_rx = (io_stream_device *)io_add_stream_device(ctx, "/dev/streamio_rx_0");
```

#### 从接收通道中读数据：

首先创建buffer以供读写：
```c
struct channel_buffer *buffer_rx_test = io_stream_get_buffer(dma_rx);
```

将数据流读取到Buffer：
```c
io_read_stream_device(dma_rx, buffer_rx_test, sizeof(iq_buffer) * test_size);
```

由于DMA的特性，io_read_stream_device函数是不会阻塞的，需要调用
```c
io_sync_stream_device(dma_rx);
```
等待所有buffer读取完成。

通过循环连续读取数据的示例`example/src/tc_test.c`：
```c
while (loop_times && !stop)
{
    for (j = 0; (j < test_buffers) && !stop; j++)
    {
        buffer_rx_test = io_stream_get_buffer(dma_rx);

        // io_sync_stream_device(dma_rx);
        if(validate_data(buffer_rx_test, test_size, j, 0) < 0) {
            goto exit;
        }else {
            if(((valid_ok_count % valid_interval) + 1) == valid_interval) {
                //  fputc('-', stderr);
            }
            valid_ok_count++;
        }

        if (!buffer_rx_test)
        {
            printf("Buffer request timeout\n");
            goto force_exit;
        }
        memset(buffer_rx_test->buffer, -1, sizeof(iq_buffer) * rx_test_size);
        io_read_stream_device(dma_rx, buffer_rx_test, rx_test_size * sizeof(iq_buffer));
        // buffer_rx_test = io_stream_get_buffer(dma_rx);
        // io_finish_stream_local(dma_rx);
    }

    loop_times--;
}
```

#### 从接受同道中读数据（select）

新的驱动中支持了select操作，使得读取不会因为DMA接收时没有数据而卡住。

使用方法可以参考`userspace/test_app/txrx_test_thread.c`中的`tx_thread`函数。相关片段摘录如下：

```c
    FD_ZERO(&readfds);
    FD_SET(dma_rx->fd, &readfds);
    for (int j = 0; ; j++)
    {
        // 当DMA没有数据需要读的时候，程序会在这里睡眠，可以随时CTRL+C退出程序。
        // 当DMA有数据可以读的时候，程序会继续运行
        ret = select(dma_rx->fd + 1, &readfds, NULL, NULL, &timeout);      
        
        if(ret < 0) {
            printf("Poll ret=%d\n", ret);
            continue;
        }else if(ret == 0) {
            printf("Timeout. ");
            break;
        }

        if(!FD_ISSET(dma_rx->fd, &readfds)) {
            printf("Poll rx not ready");
            continue;

        }


        buffer_test_rx = io_stream_get_buffer(dma_rx);


        io_read_stream_device(dma_rx, buffer_test_rx, 128 * 1024);

        // ...
    }
```


#### 向发送通道中写入数据

主要操作和读数据相似，参考`example/src/tc_test.c`的`main_tx`：

```c
int main_tx(int argc, char **argv)
{
    printf("sizeof pointer=%ld enum=%ld\n", sizeof(uint32_t *), sizeof(PROXY_BUSY));
    printf("Create context\n");
    int i, ret;
    io_context *ctx = io_create_context();
    printf("Add devices\n");

    io_stream_device *dma_tx = (io_stream_device *)io_add_stream_device(ctx, "/dev/streamio_tx_0");

    int test_times = 100, j = 0;

    int test_size = MAX_SAMPLES;
    for (int j = 0; j < test_times; j++)
    {
        struct channel_buffer *buffer_test = io_stream_get_buffer(dma_tx);      // 创建Buffer

        for (i = 0; i < test_size; i++) // 填充Buffer
        {
            buffer_test->buffer[i].I0 = i + j * test_size;
            buffer_test->buffer[i].Q0 = i + j * test_size;
        }
        io_write_stream_device(dma_tx, buffer_test, test_size * sizeof(iq_buffer));     // 开启DMA传输
    }

    io_close_context(ctx); 

    return 0;
}
```


### 关闭`io_context`

```c
io_close_context(ctx);
```

`io_close_context`函数会关闭所有打开的设备。


## 编译说明


需要先创建一个Makefile.variable文件, 在文件中指定编译器和架构

```makefile
CROSS_COMPILE = <编译器目录>/aarch64-linux-gnu-
TARGET_ARCH = arm64
```
在example/makefile的第一行

```makefile
MAKE_ENV_INCLUDE := ../../Makefile.variable
```

修改这一行为指Makefile.variable的相对目录.


## PL端工程说明


PL端工程主要部分为baseband_sys.v。


### AXI-Lite和AXIS的演示代码说明

工程包含了基本的功能的例子，在baseband_sys.v内，包含一个AXI-Lite读写的寄存器组、两个AXIS从、一个AXIS主。

将寄存器引出的示例（`baseband_sys_axi.v`文件的末尾处）
```verilog
	// Implement memory mapped register select and read logic generation
	  assign S_AXI_RDATA = (axi_araddr[ADDR_LSB+OPT_MEM_ADDR_BITS:ADDR_LSB] == 8'h0) ? slv_reg0 : 
	                       (axi_araddr[ADDR_LSB+OPT_MEM_ADDR_BITS:ADDR_LSB] == 8'h1) ? slv_reg1 : 
	                       (axi_araddr[ADDR_LSB+OPT_MEM_ADDR_BITS:ADDR_LSB] == 8'h2) ? slv_reg2 : 
	                       (axi_araddr[ADDR_LSB+OPT_MEM_ADDR_BITS:ADDR_LSB] == 8'h3) ? slv_reg3 :
	                       (axi_araddr[ADDR_LSB+OPT_MEM_ADDR_BITS:ADDR_LSB] == 8'h4) ? slv_reg4 :
	                       (axi_araddr[ADDR_LSB+OPT_MEM_ADDR_BITS:ADDR_LSB] == 8'h5) ? slv_reg5 :
	                       (axi_araddr[ADDR_LSB+OPT_MEM_ADDR_BITS:ADDR_LSB] == 8'h6) ? slv_reg6 :
	                       (axi_araddr[ADDR_LSB+OPT_MEM_ADDR_BITS:ADDR_LSB] == 8'h7) ? slv_reg7 :
	                       (axi_araddr[ADDR_LSB+OPT_MEM_ADDR_BITS:ADDR_LSB] == 8'h8) ? slv_reg8 :
	                       0;  
	// Add user logic here
    assign reg_state_example = slv_reg5;
    assign axis_packet_length = slv_reg6;
	// User logic ends
```

如上代码使寄存器0~8可供读写，其中寄存器5、6分别用于写入复位信号和数据流长度。


AXIS从模块将DMA发送的AXIS数据转换为连续数据流
- `axis0_dout`：32bit数据
- `axis0_dout_strobe`：数据有效信号
- `axis0_bb_data0_ready`：准备接收数据信号，AXIS从模块在`axis0_bb_data0_ready`有效时，才会输出数据


```verilog
   /*
     * Add your module here
     */

    // DSPs DDS example
    dds_compiler_0 dds_compiler_0_inst_0 (
      .aclk(s00_axis_aclk),                       // input wire aclk
      .aresetn(s00_axis_aresetn),
        
      // from DMA
      .s_axis_phase_tready(axis0_bb_data0_ready),
      .s_axis_phase_tvalid(axis0_dout_strobe),  // input wire s_axis_phase_tvalid
      .s_axis_phase_tdata({axis0_dout[8:0], 8'b0} ),    // input wire [15 : 0] s_axis_phase_tdata
      
      // output to FIFO
      .m_axis_data_tvalid(bb_data0_out_strobe),    // output wire m_axis_data_tvalid
      .m_axis_data_tdata(bb_data0_out),      // output wire [31 : 0] m_axis_data_tdata
      .m_axis_data_tready(bb_data0_fifo_ready)           
    );
    
    // Reverse bits example
    assign bb_data1_out[C_S00_AXIS_TDATA_WIDTH-1 -: 16] = ~axis0_dout[15:0];
    assign bb_data1_out[15:0] = axis0_dout[15:0];
    assign bb_data1_out_strobe = axis0_dout_strobe;
    assign axis0_bb_data1_ready = bb_data1_fifo_ready;
    
    // Select
    assign bb_data_out        = bb_data_sel == 4'h0 ? bb_data0_out:
                                               4'h1 ? bb_data1_out:
                                              {C_S00_AXIS_TDATA_WIDTH{1'bz}};      
                                                                   
    assign bb_data_out_strobe = bb_data_sel == 4'h0 ? bb_data0_out_strobe:
                                               4'h1 ? bb_data1_out_strobe:
                                                   1'bz;
    
    assign bb_data0_fifo_ready = bb_data_sel == 4'h0 ? bb_data_fifo_ready : 1'b0;
    assign bb_data1_fifo_ready = bb_data_sel == 4'h1 ? bb_data_fifo_ready : 1'b0;
    
    assign bb_data_fifo_ready = !fifo_ch0_full; 
```


### 注意事项

1. 受Xilinx DMA驱动的限制，每次DMA读出长度需要大于一次AXIS传输的长度，否则会造成DMA核卡死。因此需要明确读出长度。
2. 不要修改Address Editor。

**任何不明确的地方、问题和BUG请及时沟通。**
