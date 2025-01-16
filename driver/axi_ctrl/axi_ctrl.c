#include <linux/module.h>
#include <linux/io.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/mm.h>

#define REG_SIZE 4
#define REG_LENGTH 512

struct transmit_ctrl_dev_data
{
    dev_t devid;
    struct device *mdev;
    struct cdev *cdev;
    struct class *class;
    struct device *device;
    struct kobject *kobj_ref;
    int major;
    int minor;
    struct resource *phy_regs;
    void *regs;
};

static struct transmit_ctrl_dev_data dev_data;

typedef struct _tc_dev_ctx
{
    struct transmit_ctrl_dev_data *dev_data;
    unsigned int addr;
    bool burst;
} transmit_ctx;

static int tc_cdev_open(struct inode *inode, struct file *file)
{
    transmit_ctx *tc = (transmit_ctx *)kzalloc(sizeof(transmit_ctx), GFP_KERNEL);

    file->private_data = tc;

    tc->dev_data = &dev_data;
    tc->addr = 0;
    file->f_pos = 0;

    // pr_info("tc_cdev_open");
    return 0;
}

static int tc_cdev_release(struct inode *inode, struct file *file)
{
    transmit_ctx *tc = (transmit_ctx *)file->private_data;

    kfree(tc);

    // pr_info("tc_cdev_release");

    return 0;
}

static ssize_t tc_cdev_read(struct file *file, char __user *user_buffer,
                            size_t size, loff_t *offset)
{
    transmit_ctx *tc = (transmit_ctx *)file->private_data;
    unsigned int *reg_read_buffer = (unsigned int *)kzalloc(size, GFP_KERNEL);

    unsigned int read_size = min((size / REG_SIZE), (size_t)REG_LENGTH - tc->addr);
    ssize_t exact_size = read_size * REG_SIZE;
    unsigned int addr = file->f_pos / REG_SIZE;

    dev_info(tc->dev_data->device, "request size %ld read size %d offset %ld\n", size, read_size, *offset);
    for (; addr < read_size; addr++)
    {
        reg_read_buffer[addr] = ioread32(tc->dev_data->regs + (addr * REG_SIZE));
        // dev_info(tc->dev_data->device, "read reg: %d %u \n", addr, reg_read_buffer[addr]);
    }
    /* read data from my_data->buffer to user buffer */
    if (copy_to_user(user_buffer, reg_read_buffer, exact_size))
    {

        kfree(reg_read_buffer);
        return -EFAULT;
    }
    file->f_pos += exact_size;
    kfree(reg_read_buffer);
    return exact_size;
}

static ssize_t tc_cdev_write(struct file *file, const char __user *user_buffer,
                             size_t size, loff_t *offset)
{
    transmit_ctx *tc = (transmit_ctx *)file->private_data;
    unsigned int *reg_read_buffer = (unsigned int *)kzalloc(size * sizeof(char), GFP_KERNEL);

    unsigned int write_size = min((size / REG_SIZE), (size_t)REG_LENGTH - tc->addr);
    unsigned int addr = file->f_pos / REG_SIZE;
    dev_info(tc->dev_data->device, "request size %ld write size %d offset %ld\n", size, write_size, *offset);
    /* read data from user buffer to my_data->buffer */
    if (copy_from_user(reg_read_buffer, user_buffer, size))
    {
        kfree(reg_read_buffer);
        return -EFAULT;
    }

    for (; addr < write_size; addr++)
    {
        // dev_info(tc->dev_data->device, "Write reg: %d x %u", addr, reg_read_buffer[addr]);
        iowrite32(reg_read_buffer[addr], tc->dev_data->regs + (addr * REG_SIZE));
    }
    kfree(reg_read_buffer);
    file->f_pos += write_size * REG_SIZE;
    // dev_info(tc->dev_data->device, "Write exited.");
    return size;
}

static loff_t llseek(struct file *file, loff_t offset, int whence)
{
    transmit_ctx *tc = (transmit_ctx *)file->private_data;
    loff_t newpos;
    switch (whence)
    {
    case SEEK_SET:
        newpos = offset;
        break;
    case SEEK_END:
        newpos = REG_LENGTH;
        break;
    case SEEK_CUR:
        newpos = tc->addr + offset;
        break;
    default:
        break;
    }
    if (newpos < 0)
        return -EINVAL;
    dev_info(tc->dev_data->device, "Seek to %d (%d %d)\n", newpos, offset, whence);
    file->f_pos = newpos;
    return newpos;
}

int simple_mmap(struct file *filp, struct vm_area_struct *vma)
{
    int result;
    transmit_ctx *tc = (transmit_ctx *)filp->private_data;
    unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
    unsigned long map_offset = (unsigned long)tc->dev_data->regs + offset;
    unsigned long pfn_start = vmalloc_to_pfn(tc->dev_data->regs);
    // unsigned long pfn_start = vmalloc_to_pfn((void *)map_offset) >> PAGE_SHIFT;
    unsigned long size = vma->vm_end - vma->vm_start;
    unsigned long vmstart = vma->vm_start;

    if ((size % PAGE_SIZE) != 0)
    {
        size = (size + PAGE_SIZE) / PAGE_SIZE * PAGE_SIZE;
    }

    // vma->vm_page_prot = pgprot_dmacoherent(vma->vm_page_prot);
    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
    // struct resource * res = tc->dev_data->phy_regs;
    dev_info(tc->dev_data->device, "Mapping 0x%X, 0x%X 0x%X", map_offset, pfn_start, virt_to_phys((void *)map_offset));
    // result = vm_iomap_memory(vma, res->start, res->end - res->start);
    result = remap_pfn_range(vma, vma->vm_start, pfn_start, size, vma->vm_page_prot);
    return result;
}

const struct file_operations tc_fops = {
    .owner = THIS_MODULE,
    .llseek = llseek,
    .open = tc_cdev_open,
    .release = tc_cdev_release,
    .read = tc_cdev_read,
    .write = tc_cdev_write,
    .mmap = simple_mmap,
};

static int cdevice_init(struct platform_device *pdev, struct transmit_ctrl_dev_data *tc)
{
    int err = 0;

    err = alloc_chrdev_region(&dev_data.devid, 0, 1, "transmit_control");
    if (err)
    {
        return -ENAVAIL;
    }

    dev_data.major = MAJOR(dev_data.devid);
    dev_data.minor = MINOR(dev_data.devid);

    dev_data.cdev = cdev_alloc();
    // dev_data.cdev->owner = THIS_MODULE;

    cdev_init(dev_data.cdev, &tc_fops);

    dev_info(&pdev->dev, "transmit control dev created major=%d,minor=%d\r\n", dev_data.major, dev_data.minor);

    err = cdev_add(dev_data.cdev, dev_data.devid, 1);
    if (err)
    {
        goto r_class;
    }

    dev_data.class = class_create("tc");

    if (IS_ERR(dev_data.class))
    {
        err = PTR_ERR(dev_data.class);
        goto r_class;
    }

    dev_data.device = device_create(dev_data.class, NULL, dev_data.devid, NULL, "tc");
    if (IS_ERR(dev_data.device))
    {
        err = PTR_ERR(dev_data.device);
        goto r_device;
    }

    return err;

r_device:
    pr_info("Releasing device code=%d\n", err);
    class_destroy(dev_data.class);
r_class:
    pr_info("Releasing cdev class code=%d\n", err);
    cdev_del(dev_data.cdev);
    unregister_chrdev_region(dev_data.devid, 1);

    return err;
}

static void cdevice_exit(void)
{
    device_destroy(dev_data.class, MKDEV(dev_data.major, dev_data.minor));

    class_unregister(dev_data.class);
    class_destroy(dev_data.class);
    cdev_del(dev_data.cdev);
    unregister_chrdev_region(dev_data.devid, 1);
}

static int transmit_control_probe(struct platform_device *pdev)
{
    struct device *dev = &pdev->dev;
    int ret;

    dev_data.device = dev;

    dev_info(dev, "Transmit control driver probing...\n");

    dev_data.regs = devm_platform_ioremap_resource(pdev, 0);

    if (IS_ERR(dev_data.regs))
    {
        return PTR_ERR(dev_data.regs);
    }
    dev_data.phy_regs = platform_get_resource(pdev, IORESOURCE_IO, 0);
    dev_dbg(dev, "phy_regs: %p", dev_data.phy_regs);
    dev_data.phy_regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    dev_dbg(dev, "phy_regs: %p", dev_data.phy_regs);
    // if (IS_ERR(dev_data.phy_regs))
    // {
    //     return PTR_ERR(dev_data.phy_regs);
    // }

    // dev_info(dev, "IO mapped: %p %p\b",dev_data.phy_regs->start, dev_data.regs);

    ret = cdevice_init(pdev, &dev_data);
    if (ret)
    {
        dev_err(dev, "Failed to initialize cdevice %d\n", ret);
        return ret;
    }
    platform_set_drvdata(pdev, &dev_data);
    return 0;
}

static int transmit_control_remove(struct platform_device *pdev)
{
    // DataMuxCtx *data_mux = platform_get_drvdata(pdev);
    // data_mux_deinit(data_mux);
    cdevice_exit();
    return 0;
}

static const struct of_device_id transmit_control_of_ids[] = {
    {
        .compatible = "xlnx,baseband-sys-1.0",
    },
    {
        .compatible = "xlnx,bb-sys-1.0",
    },
    {}
};

static struct platform_driver transmit_control_driver = {
    .driver = {
        .name = "transmit_control",
        .owner = THIS_MODULE,
        .of_match_table = transmit_control_of_ids,
    },
    .probe = transmit_control_probe,
    .remove = transmit_control_remove,
};

int __init stream_axi_ctrl_init(void)
{
    return platform_driver_register(&transmit_control_driver);
}

void __exit stream_axi_ctrl_exit(void)
{
    platform_driver_unregister(&transmit_control_driver);
}

module_init(stream_axi_ctrl_init);
module_exit(stream_axi_ctrl_exit);

MODULE_AUTHOR("Guoyansong");
MODULE_DESCRIPTION("Data Mux Driver");
MODULE_LICENSE("GPL v2");
