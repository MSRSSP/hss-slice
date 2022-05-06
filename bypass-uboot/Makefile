ifdef O
        BUILD_DIR=$(O)
else
        BUILD_DIR=$(CURDIR)/build
endif

DTS_DIR := dts
EXAMPLE := slice
payload_config := conf/$(EXAMPLE)/config.yaml
hss_payload_generator := $(BUILD_DIR)/hss-payload-generator
hss_payload_bin := $(BUILD_DIR)/$(EXAMPLE)-payload.bin
buildroot_ext4 := ../thirdparty/buildroot/build/images/rootfs.ext4
dtb := $(foreach obj,$(DTB-y),$(DTS_DIR)/$(obj))
label:= qemu
qemu_sd := $(BUILD_DIR)/$(EXAMPLE)-$(label)-sd.img
qemu_mem_size := 4096
default: payload

payload: $(hss_payload_bin)

qemu: $(qemu_sd)

$(hss_payload_generator): ../tools/hss-payload-generator/
	make -C ../tools/hss-payload-generator/ O=$(BUILD_DIR)

$(hss_payload_bin): $(payload_config) $(hss_payload_generator) 
	$(hss_payload_generator) -c $(payload_config) -v -v $(hss_payload_bin)

ROOTFS	= 0FC63DAF-8483-4772-8E79-3D69D8477DE4
HSS_PAYLOAD = 21686148-6449-6E6F-744E-656564454649
PAYLOAD_START :=2048
RESERVED_SIZE :=2048
# Increase Kernel size if needed
KERNEL_SIZE := 200000
format-payload-prepare: $(DISK) $(hss_payload_bin)
	@echo pay:$(PAYLOAD_END)
	@echo root:$(ROOTFS_START)
	@echo $(shell $((440*$(qemu_mem_size))))
	@test -b $(DISK) || (echo "$(DISK): is not a block device"; exit 1)
	$(eval DEVICE_NAME := $(shell basename $(DISK)))
	$(eval SD_SIZE := $(shell cat /sys/block/$(DEVICE_NAME)/size))
	$(eval ROOT_SIZE := $(shell expr $(SD_SIZE) \- $(RESERVED_SIZE)))
	$(eval PAYLOAD_END := $(shell expr $(PAYLOAD_START) \+ $(KERNEL_SIZE)))
	$(eval ROOTFS_START := $(shell echo $$(($(PAYLOAD_END)+2048))))
	sudo /sbin/sgdisk -Zo  \
    	--new=1:$(PAYLOAD_START):$(PAYLOAD_END) --change-name=1:payload --typecode=1:$(HSS_PAYLOAD) \
	--new=2:$(ROOTFS_START):$(ROOT_SIZE) --change-name=2:root --typecode=2:$(ROOTFS) \
	${DISK}
	ls $(DISK)*

format-payload.val: format-payload-prepare
	$(eval PART1 := $(shell ls $(DISK)*1))
	$(eval PART2 := $(shell ls $(DISK)*2))

format-payload: format-payload.val
	sudo dd if=$(hss_payload_bin) of=$(PART1)
format-rootfs-ext4: format-payload.val
	sudo dd if=$(buildroot_ext4) of=$(PART2)


$(qemu_sd).raw:
	dd if=/dev/null of=$@ bs=1M seek=$(qemu_mem_size)
	
$(qemu_sd): loop
	make -C $(CURDIR) format-payload DISK=$(DISK)
	# Uncomment format-rootfs-ext4 if we want to load rootfs from disk.
	# Cannot concurrently allow two kernels to use one SD card. Do not enable this for more than two guest slices.
	# make -C $(CURDIR) format-rootfs-ext4 DISK=$(DISK)
	
	echo "Release loop device ..." &&\
	sleep 1
	sudo losetup -D &&\
	ln -s -f $(qemu_sd).raw $@ &&\
	echo "Run ./qemu.sh $@ to launch emulation."

loop: $(qemu_sd).raw
	 $(eval DISK := $(shell sudo losetup --partscan --show --find $<))
