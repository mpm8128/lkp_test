
obj-m := p2.o

CONFIG_MODULE_SIG=n

#this should be the path to your kernel source directory
#KDIR := ~/linux
KDIR := /lib/modules/$(shell uname -r)/build

PWD := $(shell pwd)

all: p2.c
	make -C $(KDIR) SUBDIRS=$(PWD) modules

clean:
	make -C $(KDIR) SUBDIRS=$(PWD) clean
