obj-m += dvfs_latency.o

EXTRA_CFLAGS = -I$(src)

all:
	make -C $(KERNEL_SRC) M=$(PWD) modules

clean:
	make -C $(KERNEL_SRC) M=$(PWD) clean
