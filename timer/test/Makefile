KERNELBUILD := /lib/modules/$(shell uname -r)/build

obj-m := test_timer.o

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
	