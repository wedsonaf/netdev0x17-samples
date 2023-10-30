obj-m := step1_mod.o step2_mod.o step3_mod.o step4_mod.o step5_mod.o step6_mod.o
step1_mod-y := step1.o
step2_mod-y := step2.o
step3_mod-y := step3.o
step4_mod-y := step4.o
step5_mod-y := step5.o
step6_mod-y := step6.o

KDIR := /lib/modules/$(shell uname -r)/build

all:
	make -C $(KDIR) M=$(shell pwd) modules

clean:
	make -C $(KDIR) M=$(shell pwd) clean
