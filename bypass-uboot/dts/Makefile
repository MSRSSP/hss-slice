USE_SLICES := y
DTB-y := single.dtb

default: $(DTB-y)

%.dtb: %.dts
	dtc -I dts -O dtb $< > $@
