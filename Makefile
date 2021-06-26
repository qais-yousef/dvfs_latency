obj-m += dvfs_latency.o

OUTPUT=dvfs_latency_test

EXTRA_CFLAGS = -I$(src)

all:
	make -C $(KERNEL_SRC) M=$(PWD) modules
	rm -rf $(OUTPUT)
	mkdir $(OUTPUT)
	cp dvfs_latency.ko $(OUTPUT)
	cp run_dvfs_latency.sh $(OUTPUT)

clean:
	make -C $(KERNEL_SRC) M=$(PWD) clean
	rm -rf $(OUTPUT)
