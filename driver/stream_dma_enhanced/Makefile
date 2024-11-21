include $(PWD)/../../Makefile.variable

DEPLOY_DIR :=  dev:zynq/projects/ad9002_sys/tq15eg/rootfs/home/petalinux
ARCH = $(TARGET_ARCH)
LINUX_INC = -I$(KERN_DIR) \
        -I$(KERN_DIR)include \
        -I$(KERN_DIR)arch/$(ARCH)/include \
        -I$(KERN_DIR)arch/$(ARCH)/include/generated \


SRCS += stream_dma_driver.c
# #Create object files of all c files
OBJS = $(SRCS:.c=.o)

MOD_OBJS = $(OBJS:$(PWD)/%=%)

adrv9001_drv-objs := streamio_core.o $(MOD_OBJS)
obj-m += streamio_drv.ko

EXTRA_CFLAGS += -D_INDEP_MODULE
all:
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KERN_DIR) M=`pwd` modules -j

dtb:
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KERN_DIR) M=`pwd` device-tree/system-top.dtb

install: all
	$(INSTALL_CMD) ./stream_dma_driver.ko kernel_modules/

clean:
	rm -rf ${TARGET} ${OBJS} $(OBJS:.o=.su)
	make -C $(KERN_DIR) M=`pwd` clean
