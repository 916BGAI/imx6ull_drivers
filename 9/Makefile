NAME = demo
ARCH = arm
CROSS_COMPILE = arm-linux-gnueabihf-
KERN_DIR = /home/kjlsai/qemu_imx6ull_kernel

obj-m += $(NAME).o
$(NAME)-objs += main.o
PWD := $(shell pwd)

all:
	make -C $(KERN_DIR) M=$(PWD) modules

clean:
	make -C $(KERN_DIR) M=$(PWD) modules clean
	rm -rf modules.order

.PHONY:all clean
