
// #include "dma_tx.h"

#include <linux/module.h>
#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>

#include <linux/device.h>
#include <linux/fs.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/of_dma.h>
#include <linux/ioctl.h>
#include <linux/uaccess.h>
#include <linux/poll.h>

int streamio_drv_probe(struct platform_device *pdev)
{
}

static const struct of_device_id streamio_of_ids[] = {
    {
        .compatible = "xlnx,streamio-1.0",
    },
    {}};

static struct platform_driver streamio_driver = {
    .driver = {
        .name = "streamio_driver",
        .owner = THIS_MODULE,
        .of_match_table = streamio_of_ids,
    },
    .probe = streamio_drv_probe,
    .remove = streamio_drv_remove,
};

int __init streamio_init(void)
{
    return platform_driver_register(&streamio_driver);
}

void __exit streamio_exit(void)
{
    platform_driver_unregister(&streamio_driver);
}

module_init(streamio_init);
module_exit(streamio_driver);

MODULE_AUTHOR("ChengYe");
MODULE_DESCRIPTION("Stream I/O");
MODULE_LICENSE("GPL v2");
