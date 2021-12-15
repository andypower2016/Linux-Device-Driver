KERNELBUILD := /lib/modules/$(shell uname -r)/build
DEBFLAGS := -O2
EXTRA_CFLAGS += $(DEBFLAGS)

obj-m := char_pipe.o

default:
	make -C $(KERNELBUILD) M=$(PWD) modules
	
clean:
	rm -rf *.o *.ko *.mod.c .depend *.cmd *.markers *.order *.symvers *.tmp_versions *.mod $(BIN)

.PHONY: load
load:
	sudo bash load

.PHONY: unload	
unload:
	sudo bash unload


depend .depend dep:
	$(CC) $(EXTRA_CFLAGS) -M *.c > .depend

ifeq (.depend,$(wildcard .depend))
include .depend
endif
	