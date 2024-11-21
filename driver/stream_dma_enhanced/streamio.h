/**
 * Copyright (C) 2021 Xilinx, Inc
 *
 * Licensed under the Apache License, Version 2.0 (the "License"). You may
 * not use this file except in compliance with the License. A copy of the
 * License is located at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
 * License for the specific language governing permissions and limitations
 * under the License.
 */
 /* This header file is shared between the DMA Proxy test application and the DMA Proxy device driver. It defines the
 * shared interface to allow DMA transfers to be done from user space.
 *
 * A set of channel buffers are created by the driver for the transmit and receive channel. The application may choose
 * to use only a subset of the channel buffers to allow prioritization of transmit vs receive.
 *
 * Note: the buffer in the data structure should be 1st in the channel interface so that the buffer is cached aligned,
 * otherwise there may be issues when using cached memory.
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/platform_device.h>
#include <linux/completion.h>

#define BUFFER_SIZE (128 * 1024)	 	/* must match driver exactly */
#define BUFFER_COUNT 32					/* driver only */

#define TX_BUFFER_COUNT 	BUFFER_COUNT				/* app only, must be <= to the number in the driver */
#define RX_BUFFER_COUNT 	BUFFER_COUNT				/* app only, must be <= to the number in the driver */
#define BUFFER_INCREMENT	1				/* normally 1, but skipping buffers (2) defeats prefetching in the CPU */

// #define FINISH_XFER 	_IOW('a','a',int32_t*)
// #define START_XFER 		_IOW('a','b',int32_t*)
// #define XFER 			_IOR('a','c',int32_t*)
// #define SELECT 			_IOW('a','d',int32_t*)

#define IOCTL_FETCH_BUFFER	_IOW('a','a',int32_t*)
#define IOCTL_COMMIT_BUFFER	_IOW('a','b',int32_t*)

struct channel_buffer {
	unsigned int buffer[BUFFER_SIZE / sizeof(unsigned int)];
	enum proxy_status { PROXY_NO_ERROR = 0, PROXY_BUSY = 1, PROXY_TIMEOUT = 2, PROXY_ERROR = 3, PROXY_QUEUED } status;
	unsigned int length;
} __attribute__ ((aligned(1024)));		/* 64 byte alignment required for DMA, but 1024 handy for viewing memory */


#include <linux/dmaengine.h>
#include <linux/cdev.h>
/* The following data structures represent a single channel of DMA, transmit or receive in the case
 * when using AXI DMA.  It contains all the data to be maintained for the channel.
 */

struct buffer_desc {

};

struct proxy_bd {
	struct completion cmp;
	dma_cookie_t cookie;
	dma_addr_t dma_handle;
	struct scatterlist sglist;
};
struct dma_proxy_channel {
	struct channel_buffer *buffer_table_p;	/* user to kernel space interface */
	dma_addr_t buffer_phys_addr;

	struct device *proxy_device_p;				/* character device support */
	struct device *dma_device_p;
	dev_t dev_node;
	struct cdev cdev;
	struct class *class_p;

	struct proxy_bd bdtable[BUFFER_COUNT];

	struct dma_chan *channel_p;				/* dma support */
	u32 direction;						/* DMA_MEM_TO_DEV or DMA_DEV_TO_MEM */
	int bdindex;
};

struct dma_proxy {
	int channel_count;
	struct dma_proxy_channel *channels;
	char **names;
	struct work_struct work;
};

int dma_proxy_probe(struct platform_device *pdev);
int dma_proxy_remove(struct platform_device *pdev);

int __init dma_proxy_init(void);
void __exit dma_proxy_exit(void);