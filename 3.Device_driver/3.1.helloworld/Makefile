ARCH ?= arm
CROSS_COMPILE ?= arm-poky-linux-gnueabi-

KERNEL_SRC := /home/adonisking/workspace/4.SDK/sysroots/cortexa72-poky-linux/usr/src/kernel
SRC_DIR := $(shell pwd)/src

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(SRC_DIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	cd src
	find . -maxdepth 1 ! -name 'helloworld.c' ! -name '.' -exec rm -v {} +

	

