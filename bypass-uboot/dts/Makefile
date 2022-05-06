USE_SLICES := y
DTB-y := single.dtb
DTB-y += slice-1.dtb
DTB-y += slice-2.dtb

default: $(DTB-y)

%.dtb: %.dts
	dtc -I dts -O dtb $< > $@
