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

/* DMA Proxy
 *
 * This module is designed to be a small example of a DMA device driver that is
 * a client to the DMA Engine using the AXI DMA / MCDMA driver. It serves as a proxy
 * for kernel space DMA control to a user space application.
 *
 * A zero copy scheme is provided by allowing user space to mmap a kernel allocated
 * memory region into user space, referred to as a set of channel buffers. Ioctl functions
 * are provided to start a DMA transfer (non-blocking), finish a DMA transfer (blocking)
 * previously started, or start and finish a DMA transfer blocking until it is complete.
 * An input argument which specifies a channel buffer number (0 - N) to be used for the
 * transfer is required.
 *
 * By default the kernel memory allocated for user space mapping is going to be
 * non-cached at this time. Non-cached memory is pretty slow for the application.
 * A h/w coherent system for MPSOC has been tested and is recommended for higher
 * performance applications.
 *
 * Hardware coherency requires the following items in the system as documented on the
 * Xilinx wiki and summarized below::
 *   The AXI DMA read and write channels AXI signals must be tied to the correct state to
 *    generate coherent transactions.
 *   An HPC slave port on MPSOC is required
 *   The CCI of MPSOC must be initialized prior to the APU booting Linux
 *   A dma-coherent property is added in the device tree for the proxy driver.
 *
 * There is an associated user space application, dma_proxy_test.c, and dma_proxy.h
 * that works with this device driver.
 *
 * The hardware design was tested with an AXI DMA / MCDMA  with scatter gather and
 * with the transmit channel looped back to the receive channel. It should
 * work with or without scatter gather as the scatter gather mentioned in the
 * driver is only at the s/w framework level rather than in the hw.
 *
 * This driver is character driver which creates devices that user space can
 * access for each DMA channel, such as /dev/dma_proxy_rx and /dev/dma_proxy_tx.
 * The number and names of channels are taken from the device tree.
 * Multiple instances of the driver (with multiple IPs) are also supported.

 * An internal test mode is provided to allow it to be self testing without the
 * need for a user space application and this mode is good for making bigger
 * changes to this driver.
 *
 * This driver is designed to be simple to help users get familiar with how to
 * use the DMA driver provided by Xilinx which uses the Linux DMA Engine.
 *
 * To use this driver a node must be added into the device tree.  Add a
 * node similar to the examples below adjusting the dmas property to match the
 * name of the AXI DMA / MCDMA node.
 *
 * The dmas property contains pairs with the first of each pair being a reference
 * to the DMA IP in the device tree and the second of each pair being the
 * channel of the DMA IP. For the AXI DMA IP the transmit channel is always 0 and
 * the receive is always 1. For the AXI MCDMA IP the 1st transmit channel is
 * always 0 and receive channels start at 16 since there can be a maximum of 16
 * transmit channels. Each name in the dma-names corresponds to a pair in the dmas
 * property and is only a logical name that allows user space access to the channel
 * such that the name can be any name as long as it is unique.
 *
 *	For h/w coherent systems with MPSoC, the property dma-coherent can be added
 * to the node in the device tree.
 *
 * Example device tree nodes:
 *
 * For AXI DMA with transmit and receive channels with a loopback in hardware
 *
 * dma_proxy {
 *   compatible ="xlnx,dma_proxy";
 *   dmas = <&axi_dma_1_loopback 0  &axi_dma_1_loopback 1>;
 *   dma-names = "dma_proxy_tx", "dma_proxy_rx";
 * };
 *
 * For AXI DMA with only the receive channel
 *
 * dma_proxy2 {
 *   compatible ="xlnx,dma_proxy";
 *   dmas = <&axi_dma_0_noloopback 1>;
 *   dma-names = "dma_proxy_rx_only";
 * };
 *
 * For AXI MCDMA with two channels
 *
 * dma_proxy3 {
 *   compatible ="xlnx,dma_proxy";
 *   dmas = <&axi_mcdma_0 0  &axi_mcdma_0 16 &axi_mcdma_0 1 &axi_mcdma_0 17> ;
 *   dma-names = "dma_proxy_tx_0", "dma_proxy_rx_0", "dma_proxy_tx_1", "dma_proxy_rx_1";
 * };
 */

/**
 * https://github.com/Xilinx-Wiki-Projects/software-prototypes/tree/master/linux-user-space-dma
 */

#include "asm-generic/poll.h"
#include "dma_tx.h"
#include "linux/atomic/atomic-instrumented.h"
#include "linux/cleanup.h"
#include "linux/irqreturn.h"
#include "linux/printk.h"
#include "linux/wait.h"

#include <linux/module.h>
/* #include <linux/version.h> */
#include <linux/kernel.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/of_dma.h>
#include <linux/ioctl.h>
#include <linux/uaccess.h>
#include <linux/poll.h>

MODULE_LICENSE("GPL");

#define DRIVER_NAME "dma_proxy"
#define TX_CHANNEL 1
#define RX_CHANNEL 1
#define ERROR -1
#define TEST_SIZE 1024

/* The following module parameter controls if the internal test runs when the module is inserted.
 * Note that this test requires a transmit and receive channel to function and uses the first
 * transmit and receive channnels when multiple channels exist.
 */
static unsigned internal_test = 0;
/* module_param(internal_test, int, S_IRUGO); */

static int ch_timeout_ms = 3000;
module_param(ch_timeout_ms, int, S_IRUGO);

static int total_count;

/* Handle a callback and indicate the DMA transfer is complete to another
 * thread of control
 */
static void sync_callback(void *completion)
{
	/* Indicate the DMA transaction completed to allow the other
	 * thread of control to finish processing
	 */
	complete(completion);
}

/* Prepare a DMA buffer to be used in a DMA transaction, submit it to the DMA engine
 * to ibe queued and return a cookie that can be used to track that status of the
 * transaction
 */
static void start_transfer(struct dma_proxy_channel *pchannel_p)
{
	enum dma_ctrl_flags flags = DMA_CTRL_ACK | DMA_PREP_INTERRUPT;
	struct dma_async_tx_descriptor *chan_desc;
	struct dma_device *dma_device = pchannel_p->channel_p->device;
	int bdindex = pchannel_p->bdindex;

	/* A single entry scatter gather list is used as it's not clear how to do it with a simpler method.
	 * Get a descriptor for the transfer ready to submit
	 */
	sg_init_table(&pchannel_p->bdtable[bdindex].sglist, 1);
	sg_dma_address(&pchannel_p->bdtable[bdindex].sglist) = pchannel_p->bdtable[bdindex].dma_handle;
	sg_dma_len(&pchannel_p->bdtable[bdindex].sglist) = pchannel_p->buffer_table_p[bdindex].length;

	/* pr_info("DMA buffer %d length %d", bdindex, pchannel_p->buffer_table_p[bdindex].length); */

	chan_desc = dma_device->device_prep_slave_sg(pchannel_p->channel_p, &pchannel_p->bdtable[bdindex].sglist, 1,
												 pchannel_p->direction, flags, NULL);

	if (!chan_desc)
	{
		pr_err("dmaengine_prep*() error\n");
	}
	else
	{
		chan_desc->callback = sync_callback;
		chan_desc->callback_param = &pchannel_p->bdtable[bdindex].cmp;

		/* Initialize the completion for the transfer and before using it
		 * then submit the transaction to the DMA engine so that it's queued
		 * up to be processed later and get a cookie to track it's status
		 */
		init_completion(&pchannel_p->bdtable[bdindex].cmp);

		pchannel_p->bdtable[bdindex].cookie = dmaengine_submit(chan_desc);
		if (dma_submit_error(pchannel_p->bdtable[bdindex].cookie))
		{
			pr_err("Buffer %d submit error\n", bdindex);
			return;
		}

		/* Start the DMA transaction which was previously queued up in the DMA engine
		 */
		pchannel_p->buffer_table_p[bdindex].status = PROXY_QUEUED;
		dma_async_issue_pending(pchannel_p->channel_p);
	}
}

/* Wait for a DMA transfer that was previously submitted to the DMA engine
 */
static void wait_for_transfer(struct dma_proxy_channel *pchannel_p)
{
	unsigned long timeout = msecs_to_jiffies(ch_timeout_ms);
	struct dma_tx_state state;
	enum dma_status status;
	int bdindex = pchannel_p->bdindex;
    int value; 
	struct channel_buffer *current_buffer = &pchannel_p->buffer_table_p[bdindex];
	struct proxy_bd *current_bd = &pchannel_p->bdtable[bdindex];

	/* Wait for the transaction to complete, or timeout, or get an error
	 */
	// wait_for_completion(&pchannel_p->bdtable[bdindex].cmp); //
	// if (ch_timeout_ms < 0)
    current_buffer->status = PROXY_BUSY;
    /* timeout = wait_for_completion_interruptible(&pchannel_p->bdtable[bdindex].cmp); */
    timeout = wait_for_completion_interruptible_timeout(&pchannel_p->bdtable[bdindex].cmp, timeout);
	// else
	// timeout = wait_for_completion_timeout(&pchannel_p->bdtable[bdindex].cmp, timeout);
	status = dma_async_is_tx_complete(pchannel_p->channel_p, pchannel_p->bdtable[bdindex].cookie, NULL, NULL);

	if (timeout == 0)
	{
		pchannel_p->buffer_table_p[bdindex].status = PROXY_TIMEOUT;
		if (status == DMA_IN_PROGRESS)
		{
			status = pchannel_p->channel_p->device->device_tx_status(pchannel_p->channel_p, pchannel_p->bdtable[bdindex].cookie, &state);
			// dmaengine_terminate_sync(pchannel_p->channel_p);
		}
		dev_err(pchannel_p->proxy_device_p, "DMA %d timed out with status=%d compete=%d res=%u\n", bdindex, status, current_bd->cmp.done, state.residue);
	}
	else if (status != DMA_COMPLETE)
	{
		pchannel_p->buffer_table_p[bdindex].status = PROXY_ERROR;
		dev_err(pchannel_p->proxy_device_p, "DMA returned completion callback status of: %s(%d)\n",
			   status == DMA_ERROR ? "error" : "in progress", status);
	}
	else
	{
		status = pchannel_p->channel_p->device->device_tx_status(pchannel_p->channel_p, pchannel_p->bdtable[bdindex].cookie, &state);
		pchannel_p->buffer_table_p[bdindex].status = PROXY_NO_ERROR;

        value = atomic_dec_if_positive(&pchannel_p->wait_transfer_count);
		pr_debug("DMA %d finished with status=%d compete=%d res=%u r=%d->%d\n",
          bdindex, status, current_bd->cmp.done, state.residue, value, atomic_read(&pchannel_p->wait_transfer_count));
	}
}

static void tasklet_handle_transfer(void)
{
    // handle transfer
}

/* The following functions are designed to test the driver from within the device
 * driver without any user space. It uses the first channel buffer for the transmit and receive.
 * If this works but the user application does not then the user application is at fault.
 */
static void tx_test(struct work_struct *local_work)
{
	struct dma_proxy *lp;
	lp = container_of(local_work, struct dma_proxy, work);

	/* Use the 1st buffer for the test
	 */
	lp->channels[TX_CHANNEL].buffer_table_p[0].length = TEST_SIZE;
	lp->channels[TX_CHANNEL].bdindex = 0;

	start_transfer(&lp->channels[TX_CHANNEL]);
	wait_for_transfer(&lp->channels[TX_CHANNEL]);
}

static void test(struct dma_proxy *lp)
{
	int i;

	printk("Starting internal test\n");

	/* Initialize the buffers for the test
	 */
	for (i = 0; i < TEST_SIZE / sizeof(unsigned int); i++)
	{
		lp->channels[TX_CHANNEL].buffer_table_p[0].buffer[i] = i;
		lp->channels[RX_CHANNEL].buffer_table_p[0].buffer[i] = 0;
	}

	/* Since the transfer function is blocking the transmit channel is started from a worker
	 * thread
	 */
	INIT_WORK(&lp->work, tx_test);
	schedule_work(&lp->work);

	/* Receive the data that was just sent and looped back
	 */
	lp->channels[RX_CHANNEL].buffer_table_p->length = TEST_SIZE;
	lp->channels[TX_CHANNEL].bdindex = 0;

	start_transfer(&lp->channels[RX_CHANNEL]);
	wait_for_transfer(&lp->channels[RX_CHANNEL]);

	/* Verify the receiver buffer matches the transmit buffer to
	 * verify the transfer was good
	 */
	for (i = 0; i < TEST_SIZE / sizeof(unsigned int); i++)
		if (lp->channels[TX_CHANNEL].buffer_table_p[0].buffer[i] !=
			lp->channels[RX_CHANNEL].buffer_table_p[0].buffer[i])
		{
			printk("buffers not equal, first index = %d\n", i);
			break;
		}

	printk("Internal test complete\n");
}

/* Map the memory for the channel interface into user space such that user space can
 * access it using coherent memory which will be non-cached for s/w coherent systems
 * such as Zynq 7K or the current default for Zynq MPSOC. MPSOC can be h/w coherent
 * when set up and then the memory will be cached.
 */
static int mmap(struct file *file_p, struct vm_area_struct *vma)
{
	struct dma_proxy_channel *pchannel_p = (struct dma_proxy_channel *)file_p->private_data;
	/* pr_info("Memory mapped size %d phy_addr=%px\n", vma->vm_end - vma->vm_start, pchannel_p->buffer_phys_addr); */
	return dma_mmap_coherent(pchannel_p->dma_device_p, vma,
							 pchannel_p->buffer_table_p, pchannel_p->buffer_phys_addr,
							 vma->vm_end - vma->vm_start);
}

/* Open the device file and set up the data pointer to the proxy channel data for the
 * proxy channel such that the ioctl function can access the data structure later.
 */
static int local_open(struct inode *ino, struct file *file)
{
	file->private_data = container_of(ino->i_cdev, struct dma_proxy_channel, cdev);

	int i = 0;
	struct dma_proxy_channel *pchannel_p = (struct dma_proxy_channel *)file->private_data;
	struct dma_device *dma_device = pchannel_p->channel_p->device;

	/* Stop all the activity when the channel is closed assuming this
	 * may help if the application is aborted without normal closure
	 * This is not working and causes an issue that may need investigation in the
	 * DMA driver at the lower level.
	 */
	dma_device->device_terminate_all(pchannel_p->channel_p);
	for (i = 0; i < BUFFER_COUNT; i++)
	{
		pchannel_p->buffer_table_p[i].status = PROXY_NO_ERROR;
	}
	return 0;
}

static ssize_t buffer_write(struct file *file_p, const char __user *buff, size_t size, loff_t *off)
{
	struct dma_proxy_channel *pchannel_p = (struct dma_proxy_channel *)file_p->private_data;
	if (pchannel_p == NULL)
	{
		pr_err("pchannel_p is NULL\n");
		return -EINVAL;
	}
	pr_info("Copy to buffer %d + %ld size %d\n", pchannel_p->bdindex, *off, size);
	if (copy_from_user(&pchannel_p->buffer_table_p[pchannel_p->bdindex].buffer, (void *)buff, size))
		return -EINVAL;
	wmb();
	return size;
}

/* Close the file and there's nothing to do for it
 */
static int release(struct inode *ino, struct file *file)
{
#if 1
	int i = 0;
	struct dma_proxy_channel *pchannel_p = (struct dma_proxy_channel *)file->private_data;
	struct dma_device *dma_device = pchannel_p->channel_p->device;

	/* Stop all the activity when the channel is closed assuming this
	 * may help if the application is aborted without normal closure
	 * This is not working and causes an issue that may need investigation in the
	 * DMA driver at the lower level.
	 */
	// dma_device->device_terminate_all(pchannel_p->channel_p);
	for (i = 0; i < BUFFER_COUNT; i++)
	{
		pchannel_p->buffer_table_p[i].status = PROXY_NO_ERROR;
	}

#endif
	return 0;
}

/* Perform I/O control to perform a DMA transfer using the input as an index
 * into the buffer descriptor table such that the application is in control of
 * which buffer to use for the transfer.The BD in this case is only a s/w
 * structure for the proxy driver, not related to the hw BD of the DMA.
 */
static long ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    /* int ret; */
	struct dma_proxy_channel *pchannel_p = (struct dma_proxy_channel *)file->private_data;
	if (pchannel_p == NULL)
	{
		pr_err("pchannel_p is NULL\n");
		return -EINVAL;
	}
    pchannel_p->bdindex = arg;
	/* Get the bd index from the input argument as all commands require it
	 */
	// pr_info("dma proxy io_ct %d  l %p\n", &pchannel_p->bdindex, arg);
    /* ret = copy_from_user(&pchannel_p->bdindex, (int *)arg, sizeof(pchannel_p->bdindex)); */  
    /* if (ret <= 0) */
    /* { */
    /*     pr_err("Failed to copy from user ret=%d\n", ret); */

		/* return -EINVAL; */

    /* } */
	/* pr_info("ioctl %d bdindex %d\n", cmd, pchannel_p->bdindex); */
	/* Perform the DMA transfer on the specified channel blocking til it completes
	 */
	switch (cmd)
	{
	case START_XFER:
		start_transfer(pchannel_p);
		break;
	case FINISH_XFER:
		wait_for_transfer(pchannel_p);
		break;
	case XFER:
		start_transfer(pchannel_p);
		wait_for_transfer(pchannel_p);
		break;
	}

	return 0;
}

static __poll_t dma_proxy_buffer_poll(struct file *file, struct poll_table_struct *p_wait_table)
{
	uint t_mask = 0;
	int i = 0;
	struct channel_buffer *current_buffer;
	struct dma_proxy_channel *pchannel_p = (struct dma_proxy_channel *)file->private_data;
	struct dma_device *dma_device = pchannel_p->channel_p->device;
	for (i = 0; i < BUFFER_COUNT; i++)
	{
		poll_wait(file, (struct wait_queue_head *)&(pchannel_p->bdtable[i].cmp.wait), p_wait_table);
	}
	for (i = 0; i < BUFFER_COUNT; i++)
	{
		current_buffer = &pchannel_p->buffer_table_p[i];
		if (pchannel_p->bdtable[i].cmp.done)
		{

			t_mask = POLLOUT | POLLRDNORM;
			break;
		}
	}
	/*pr_info("Poll mask 0x%X at channel %d\n", t_mask, i);*/
	return t_mask;
}

static __poll_t dma_proxy_poll(struct file *file, struct poll_table_struct *p_wait_table)
{
    uint t_mask = 0;
	struct dma_proxy_channel *pchannel_p = (struct dma_proxy_channel *)file->private_data;
    poll_wait(file, &pchannel_p->wait_q_h, p_wait_table);
    if(atomic_read(&pchannel_p->wait_transfer_count)> 0) {
        t_mask |= POLLIN;
        // pr_info("rx available")
    }
    
    // TODO: Poll for TX_CHANNEL
    t_mask |= POLLOUT;

    return t_mask; 
}


static struct file_operations dm_fops = {
	.owner = THIS_MODULE,
	.open = local_open,
	.write = buffer_write,
	.release = release,
	.unlocked_ioctl = ioctl,
	.mmap = mmap,
	.poll = dma_proxy_poll
};


irqreturn_t irq_handler(int irq, void *ptr)
{
    struct dma_proxy_channel * channel = (struct dma_proxy_channel *) ptr; 
    // pr_info("IRQ! %d\n", atomic_inc_return(&channel->wait_transfer_count));
    atomic_inc_return(&channel->wait_transfer_count);

    wake_up_interruptible(&channel->wait_q_h);


    return IRQ_RETVAL(IRQ_HANDLED);
}

/* Initialize the driver to be a character device such that is responds to
 * file operations.
 */
static int cdevice_init(struct dma_proxy_channel *pchannel_p, char *name)
{
	int rc;
	char device_name[32] = "dma_proxy";
	static struct class *local_class_p = NULL;

	/* Allocate a character device from the kernel for this driver.
	 */
	rc = alloc_chrdev_region(&pchannel_p->dev_node, 0, 1, "dma_proxy");

	if (rc)
	{
		dev_err(pchannel_p->dma_device_p, "unable to get a char device number\n");
		return rc;
	}

	/* Initialize the device data structure before registering the character
	 * device with the kernel.
	 */
	cdev_init(&pchannel_p->cdev, &dm_fops);
	pchannel_p->cdev.owner = THIS_MODULE;
	rc = cdev_add(&pchannel_p->cdev, pchannel_p->dev_node, 1);

	if (rc)
	{
		dev_err(pchannel_p->dma_device_p, "unable to add char device\n");
		goto init_error1;
	}

	/* Only one class in sysfs is to be created for multiple channels,
	 * create the device in sysfs which will allow the device node
	 * in /dev to be created
	 */
	if (!local_class_p)
	{
		local_class_p = class_create(DRIVER_NAME);

		if (IS_ERR(pchannel_p->dma_device_p->class))
		{
			dev_err(pchannel_p->dma_device_p, "unable to create class\n");
			rc = ERROR;
			goto init_error2;
		}
	}
	pchannel_p->class_p = local_class_p;

	/* Create the device node in /dev so the device is accessible
	 * as a character device
	 */
	strcat(device_name, name);
	pchannel_p->proxy_device_p = device_create(pchannel_p->class_p, NULL,
											   pchannel_p->dev_node, NULL, name);

	if (IS_ERR(pchannel_p->proxy_device_p))
	{
		dev_err(pchannel_p->dma_device_p, "unable to create the device\n");
		goto init_error3;
	}

	return 0;

init_error3:
	class_destroy(pchannel_p->class_p);

init_error2:
	cdev_del(&pchannel_p->cdev);

init_error1:
	unregister_chrdev_region(pchannel_p->dev_node, 1);
	return rc;
}

/* Exit the character device by freeing up the resources that it created and
 * disconnecting itself from the kernel.
 */
static void cdevice_exit(struct dma_proxy_channel *pchannel_p)
{
	/* Take everything down in the reverse order
	 * from how it was created for the char device
	 */
	pr_info("Releasing channel %p > %p / %p\n", pchannel_p, pchannel_p->proxy_device_p, pchannel_p->class_p);
	if (pchannel_p->proxy_device_p)
	{
		device_destroy(pchannel_p->class_p, pchannel_p->dev_node);

		/* If this is the last channel then get rid of the /sys/class/dma_proxy
		 */
		if (total_count == 1)
			class_destroy(pchannel_p->class_p);

		cdev_del(&pchannel_p->cdev);
		unregister_chrdev_region(pchannel_p->dev_node, 1);
	}
}

/* Create a DMA channel by getting a DMA channel from the DMA Engine and then setting
 * up the channel as a character device to allow user space control.
 */
static int create_channel(struct platform_device *pdev, struct dma_proxy_channel *pchannel_p, char *name, u32 direction)
{
	int error;
	u32 bd;

	/* Request the DMA channel from the DMA engine and then use the device from
	 * the channel for the proxy channel also.
	 */
	pchannel_p->dma_device_p = &pdev->dev;
	pchannel_p->channel_p = dma_request_chan(&pdev->dev, name);
	if (IS_ERR(pchannel_p->channel_p))
	{
		dev_err(pchannel_p->dma_device_p, "DMA channel request error %d\n", PTR_ERR(pchannel_p->channel_p));
		return PTR_ERR(pchannel_p->channel_p);
	}
	else
	{
		dev_info(pchannel_p->dma_device_p, "Channel requested %p\n", pchannel_p->channel_p);
	}

	/* Initialize the character device for the dma proxy channel
	 */
	error = cdevice_init(pchannel_p, name);
	if (error)
		return error;

	pchannel_p->direction = direction;

	/* Allocate DMA memory that will be shared/mapped by user space, allocating
	 * a set of buffers for the channel with user space specifying which buffer
	 * to use for a tranfer..
	 */
	pchannel_p->buffer_phys_addr = 0;
	pchannel_p->buffer_table_p = (struct channel_buffer *)
		dmam_alloc_coherent(pchannel_p->dma_device_p,
							sizeof(struct channel_buffer) * BUFFER_COUNT,
							&pchannel_p->buffer_phys_addr, GFP_KERNEL);

	/* Initialize each entry in the buffer descriptor table such that the physical address
	 * address of each buffer is ready to use later.
	 */
	for (bd = 0; bd < BUFFER_COUNT; bd++)
	{
		pchannel_p->bdtable[bd].dma_handle = (dma_addr_t)((pchannel_p->buffer_phys_addr) +
														  (sizeof(struct channel_buffer) * bd) + offsetof(struct channel_buffer, buffer));
		init_completion(&pchannel_p->bdtable[bd].cmp);
	}

	/* The buffer descriptor index into the channel buffers should be specified in each
	 * ioctl but we will initialize it to be safe.
	 */
	pchannel_p->bdindex = 0;
	if (!pchannel_p->buffer_table_p)
	{
		dev_err(pchannel_p->dma_device_p, "DMA allocation error\n");
		return ERROR;
	}
	return 0;
}
/* Initialize the dma proxy device driver module.
 */
int dma_proxy_probe(struct platform_device *pdev)
{
	int rc, i;
    unsigned int irq;
	struct dma_proxy *lp;
	struct device *dev = &pdev->dev;

	printk(KERN_INFO "dma_proxy module initialized\n");

	lp = (struct dma_proxy *)devm_kmalloc(&pdev->dev, sizeof(struct dma_proxy), GFP_KERNEL);
	if (!lp)
	{
		dev_err(dev, "Cound not allocate proxy device\n");
		return -ENOMEM;
	}
	dev_set_drvdata(dev, lp);

	/* Figure out how many channels there are from the device tree based
	 * on the number of strings in the dma-names property
	 */
	lp->channel_count = device_property_read_string_array(&pdev->dev,
														  "dma-names", NULL, 0);
	if (lp->channel_count <= 0)
		return 0;

	printk("Device Tree Channel Count: %d\r\n", lp->channel_count);

	/* Allocate the memory for channel names and then get the names
	 * from the device tree
	 */
	lp->names = devm_kmalloc_array(&pdev->dev, lp->channel_count,
								   sizeof(char *), GFP_KERNEL);
	if (!lp->names)
		return -ENOMEM;

	rc = device_property_read_string_array(&pdev->dev, "dma-names",
										   (const char **)lp->names, lp->channel_count);
	if (rc < 0)
		return rc;

	/* Allocate the memory for the channels since the number is known.
	 */
	lp->channels = devm_kmalloc(&pdev->dev,
								sizeof(struct dma_proxy_channel) * lp->channel_count, GFP_KERNEL);
	if (!lp->channels)
		return -ENOMEM;

	/* Create the channels in the proxy. The direction does not matter
	 * as the DMA channel has it inside it and uses it, other than this will not work
	 * for cyclic mode.
	 */
	for (i = 0; i < lp->channel_count; i++)
	{
		pr_info("Creating channel %s\r\n", lp->names[i]);
		rc = create_channel(pdev, &lp->channels[i], lp->names[i], DMA_MEM_TO_DEV);

		if (rc)
			return rc;

        irq = platform_get_irq(pdev, i);

        if(irq < 0)
        {
            dev_warn(dev, "Interrupt not set\n");
            
        } else 
        {
            dev_info(dev, "get irq: %d\n", irq);
            rc = devm_request_irq(dev, irq, irq_handler,  IRQF_SHARED | IRQF_TRIGGER_RISING, lp->names[i], (void*) &lp->channels[i]);
            if(rc) {
                dev_warn(dev, "Failed to register IRQ %s", lp->names[i]);
            }
            else {
                lp->channels[i].irq = irq;
                init_waitqueue_head(&lp->channels[i].wait_q_h);

                atomic_set(&lp->channels[i].wait_transfer_count, 0);
                
                dev_info(dev, "IRQ %s registeried.", lp->names[i]);
            }
        }

		total_count++;
	}
    

	if (internal_test)
		test(lp);
	return 0;
}

/* Exit the dma proxy device driver module.
 */
int dma_proxy_remove(struct platform_device *pdev)
{
	int i;
    int irq;
	struct device *dev = &pdev->dev;
	struct dma_proxy *lp = dev_get_drvdata(dev);

	pr_info("dma_proxy module exited\n");

	/* Take care of the char device infrastructure for each
	 * channel except for the last channel. Handle the last
	 * channel seperately.
	 */
	for (i = 0; i < lp->channel_count; i++)
	{
        // irq = platform_get_irq(pdev, i);
        // if(irq < 0) {
        //
        // }else {
        //     // devm_free_irq(dev, irq, (void*)&lp->channels[i]);
        // }
        //
		if (lp->channels[i].proxy_device_p)
			cdevice_exit(&lp->channels[i]);
		total_count--;
	}
	/* Take care of the DMA channels and any buffers allocated
	 * for the DMA transfers. The DMA buffers are using managed
	 * memory such that it's automatically done.
	 */
	for (i = 0; i < lp->channel_count; i++)
	{
		pr_info("release channel %p\r\n", lp->channels[i].channel_p);
		if (lp->channels[i].channel_p)
		{

			lp->channels[i].channel_p->device->device_terminate_all(lp->channels[i].channel_p);

			dma_release_channel(lp->channels[i].channel_p);
		}
	}
	return 0;
}

static const struct of_device_id dma_proxy_of_ids[] = {
	{
		.compatible = "xlnx,dma_proxy",
	},
	{}};

static struct platform_driver dma_proxy_driver = {
	.driver = {
		.name = "streamio-drv",
		.owner = THIS_MODULE,
		.of_match_table = dma_proxy_of_ids,
	},
	.probe = dma_proxy_probe,
	.remove = dma_proxy_remove,
};

int __init dma_proxy_init(void)
{
	return platform_driver_register(&dma_proxy_driver);
}

void __exit dma_proxy_exit(void)
{
	platform_driver_unregister(&dma_proxy_driver);
}

module_init(dma_proxy_init);
module_exit(dma_proxy_exit);

MODULE_AUTHOR("Xilinx, Inc.");
MODULE_DESCRIPTION("DMA Proxy Prototype");
MODULE_LICENSE("GPL v2");
