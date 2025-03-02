include $(PWD)/../../Makefile.variable

ARCH = $(TARGET_ARCH)
LINUX_INC = -I$(KERN_DIR) \
        -I$(KERN_DIR)include \
        -I$(KERN_DIR)arch/$(ARCH)/include \
        -I$(KERN_DIR)arch/$(ARCH)/include/generated \



LINUX_INC = -I$(KERN_DIR) \
        -I$(KERN_DIR)include \
        -I$(KERN_DIR)arch/$(ARCH)/include \
        -I$(KERN_DIR)arch/$(ARCH)/include/generated \


# #Create object files of all c files
OBJS = $(SRCS:.c=.o)

MOD_OBJS = $(OBJS:$(PWD)/%=%)

obj-m += streamio-ctrl-drv.o

EXTRA_CFLAGS += -D_INDEP_MODULE
all:
	make ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KERN_DIR) M=`pwd` modules -j

install: all
	$(INSTALL_CMD) ./streamio-ctrl-drv.ko kernel_modules/

clean:
	rm -rf ${TARGET} ${OBJS} $(OBJS:.o=.su)
	make -C $(KERN_DIR) M=`pwd` clean
