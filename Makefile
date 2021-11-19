KERNELBUILD := /lib/modules/$(shell uname -r)/build

obj-m := char_device.o

default:
	make -C $(KERNELBUILD) M=$(PWD) modules
	
clean:
	rm -rf *.o *.ko *.mod.c *.cmd *.markers *.order *.symvers *.tmp_versions *.mod $(BIN)

load:
	sudo bash load_char
	
unload:
	sudo bash unload_char
	