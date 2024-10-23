INSTALL_CMD := bash ../deploy.sh
KERN_DIR = /home/orgleaf/Works/adrv9001-project/software/linux-xlnx-xlnx_rebase_v6.6_LTS_merge_6.6.50/
DEPLOY_DIR :=  dev:zynq/projects/ad9002_sys/tq15eg/rootfs/home/petalinux
TOOLCHAIN_PATH = /media/orgleaf/MainPool/Apps/Xilinx/Vitis/2024.1/gnu/aarch64/lin/aarch64-linux/bin/


ARCH:=arm64
CROSS_COMPILE:=$(TOOLCHAIN_PATH)aarch64-linux-gnu-


LINUX_INC = -I$(KERN_DIR) \
        -I$(KERN_DIR)include \
        -I$(KERN_DIR)arch/$(ARCH)/include \
        -I$(KERN_DIR)arch/$(ARCH)/include/generated \


# #Create object files of all c files
OBJS = $(SRCS:.c=.o)

MOD_OBJS = $(OBJS:$(PWD)/%=%)

obj-m += axi_ctrl.o

EXTRA_CFLAGS += -D_INDEP_MODULE
all:
	echo $(MOD_OBJS)
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KERN_DIR) M=`pwd` modules -j

install: all
	$(INSTALL_CMD) ./axi_ctrl.ko

clean:
	rm -rf ${TARGET} ${OBJS} $(OBJS:.o=.su)
	make -C $(KERN_DIR) M=`pwd` clean
