KDIR = /lib/modules/$(shell uname -r)/build
DTC = dtc

obj-m += encx24j600.o encx24j600-spi.o encx24j600-smi.o

.PHONY: all modules overlay clean

all: modules overlays

modules:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -f *.dtbo

overlays: encx24j600.dtbo encx24j600-smi.dtbo

%.dtbo: %-overlay.dts
	$(DTC) -W no-unit_address_vs_reg -@ -I dts -O dtb -o $@ $<

