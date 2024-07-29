KDIR = /lib/modules/$(shell uname -r)/build
DTC = dtc

ec_encx24j600_smi-objs := encx24j600.o encx24j600-smi.o

obj-m += ec_encx24j600_smi.o

.PHONY: all modules overlay clean

all: modules overlays

modules:
	$(MAKE) -C $(KDIR) M=$(PWD) KBUILD_EXTRA_SYMBOLS=ethercat.symvers modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f *.dtbo

overlays: encx24j600-smi.dtbo

%.dtbo: %-overlay.dts
	$(DTC) -W no-unit_address_vs_reg -@ -I dts -O dtb -o $@ $<

