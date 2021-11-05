KERNELBUILD := /lib/modules/$(shell uname -r)/build

obj-m := hello.o

default:
	make -C $(KERNELBUILD) M=$(PWD) modules

clean:
	rm -rf *.o *.ko *.mod.c *.cmd *.markers *.order *.symvers *.tmp_versions
