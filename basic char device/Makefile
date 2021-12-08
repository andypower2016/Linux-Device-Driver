KERNELBUILD := /lib/modules/$(shell uname -r)/build

char_device_pipe-objs := char_device.o char_pipe.o
obj-m := char_device_pipe.o

default:
	make -C $(KERNELBUILD) M=$(PWD) modules
	
clean:
	rm -rf *.o *.ko *.mod.c *.cmd *.markers *.order *.symvers *.tmp_versions *.mod $(BIN)

.PHONY: load
load:
	sudo bash load

.PHONY: unload	
unload:
	sudo bash unload
	