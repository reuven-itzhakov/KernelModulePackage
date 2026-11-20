ARCH ?= arm64
CROSS_COMPILE ?= aarch64-linux-
ROOTDIR ?= /lib/modules/$(shell uname -r)/build

obj-m := my_module.o
PWD := $(shell pwd)

default:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(ROOTDIR) M=$(PWD) modules

clean:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(ROOTDIR) M=$(PWD) clean