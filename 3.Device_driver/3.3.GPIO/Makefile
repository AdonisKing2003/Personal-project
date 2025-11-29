.phony: all help clean

OBJ ?= GPIO_control

obj-m += ${OBJ}.o

# Default ARCH to empty if not defined
ARCH ?=

# Set KERNEL_SRC and CROSS_COMPILE based on ARCH value
ifeq ($(ARCH),arm64)
	# Raspberry Pi 4 64-bit kernel source and cross-compile prefix
	KERNEL_SRC ?= /home/adonisking/workspace/4.SDK/sysroots/cortexa72-poky-linux/usr/src/kernel
	CROSS_COMPILE ?= aarch64-poky-linux-
else
	# Default: native Linux kernel build directory and no cross-compiler
	KERNEL_SRC ?= /lib/modules/$(shell uname -r)/build
	CROSS_COMPILE :=
endif

all: ## Build the project
	@echo "Building for ARCH=$(ARCH)"
	@echo "Using Kernel source: $(KERNEL_SRC)"
	@echo "Cross compile prefix: $(CROSS_COMPILE)"
	$(MAKE) -C $(KERNEL_SRC) M=$(shell pwd) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean: ## Clean build artifacts
	$(MAKE) -C $(KERNEL_SRC) M=$(shell pwd) clean

help: ## Show this help message with available commands and argument usage
	@echo "Usage: make <target> [ARGUMENT=value]"
	@echo
	@echo "Available commands:"
	@awk 'BEGIN {FS = ":.*## "; printf ""} /^[a-zA-Z0-9_-]+:.*## / {printf "  \033[36m%-15s\033[0m %s\n", $$1, $$2}' $(MAKEFILE_LIST)
	@echo
	@echo "Helpful arguments you can use:"
	@echo " ARCH    Specify architecture: \033[32mx86_64, arm64\033[0m"
	@echo " OBJ     Object file list: \033[32mdefault = GPIO_control\033[0m"
	@echo
