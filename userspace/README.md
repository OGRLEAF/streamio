# StreamIO Userspace SDK

StreamIO的用户Linux库、SDK及说明文档。


## 目录结构

```
.
├── example        # 示例程序
│   ├── makefile 
│   └── src
│       ├── arg_parse.c
│       ├── tc_test.c
│       └── tc_test.h
├── README.md      # 本文档
├── sdk            # 编译所用的头文件和动态库
│   ├── bbio
│   │   ├── bbio_backend.h
│   │   ├── bbio_backend_net.h
│   │   ├── bbio_dma_proxy.h
│   │   ├── bbio_h.h
│   │   ├── bbio_net.h
│   │   └── bbio_private_h.h
│   ├── buffer
│   │   └── buffer.h
│   ├── libstreamio.so      # 开发板所用的动态库，与user/lib下的libstreamio.so是同一文件
│   └── libstreamio-x64.so  
└── user         # 需要放置在Linux用户目录的文件
    ├── io_backend
    └── lib/libstreamio.so 
```

## 镜像和使用方法

将镜像写入SD卡后启动。

在初次启动后用户目录可以看到如下文件：

```
lib
io_backend
```

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

工程包含了基本的功能的例子，在baseband_sys.v内，包含一个AXI-Lite读写的寄存器组和一个AXIS接口的累加器，并演示了通过读写寄存器复位累加器和外部的FIFO，并能够通过寄存器配置累加器的上限。

将寄存器引出的示例（`baseband_sys_axi.v`文件的末尾处）
```verilg
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
	// User logic endsS
```

如上代码使寄存器0~8可供读写，其中寄存器5、6分别用于写入复位信号和数据流长度。

输入输出接口使用AXIS协议，示例中的`baseband_sys_axis.v`文件是一个数字累加器的示例。



### 注意事项

1. 受Xilinx DMA驱动的限制，每次DMA读出长度需要大于一次AXIS传输的长度，否则会造成DMA核卡住。因此需要特别注意提前确定读出的长度。
2. Address Editor不要更改。

**任何不明确的地方、问题和BUG请及时沟通。**