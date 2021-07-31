obj-m += tag_service.o
tag_service-objs += tag_service_src.o ./lib/device.o ./lib/rwlock.o ./lib/structs.o ./lib/bitmap.o ./lib/usctm.o ./lib/vtpmo.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules 
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

