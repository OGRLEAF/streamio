include $(PWD)/../../Makefile.variable

KERN_DIR = /home/orgleaf/Works/adrv9001-project/software/linux-xlnx-xlnx_rebase_v6.6_LTS_merge_6.6.50/
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

adrv9001_drv-objs := stream_dma_driver.o $(MOD_OBJS)
obj-m += stream_dma_driver.o

EXTRA_CFLAGS += -D_INDEP_MODULE
all:
	echo $(MOD_OBJS)
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KERN_DIR) M=`pwd` modules -j

dtb:
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KERN_DIR) M=`pwd` device-tree/system-top.dtb

install: all
	$(INSTALL_CMD) ./stream_dma_driver.ko kernel_modules/

clean:
	rm -rf ${TARGET} ${OBJS} $(OBJS:.o=.su)
	make -C $(KERN_DIR) M=`pwd` clean
