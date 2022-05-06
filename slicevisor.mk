Source-slicevisor += application/slicevisor.c
#Source-slicevisor += thirdparty/opensbi/lib/slice/slice_pmp.c
Source-slicevisor += thirdparty/opensbi/lib/slice/slice.c
Source-slicevisor += thirdparty/opensbi/lib/slice/slice_mgr.c
Source-slicevisor += thirdparty/opensbi/lib/slice/slice_reset.c
Source-slicevisor += services/tinycli/tinycli_slice.c 
Source-slicevisor += services/tinycli/tinycli_helper.c 
Source-slicevisor += thirdparty/opensbi/lib/sbi/riscv_asm.c
Source-slicevisor += thirdparty/opensbi/lib/sbi/riscv_atomic.c
Source-slicevisor += thirdparty/opensbi/lib/sbi/sbi_math.c
Source-slicevisor += thirdparty/opensbi/lib/sbi/sbi_bitops.c
Source-slicevisor += thirdparty/opensbi/lib/sbi/sbi_console.c
Source-slicevisor += thirdparty/opensbi/lib/sbi/sbi_domain.c
Source-slicevisor += thirdparty/opensbi/lib/sbi/sbi_platform.c
Source-slicevisor += thirdparty/opensbi/lib/utils/fdt/fdt_domain_data.c
#Source-slicevisor += thirdparty/opensbi/lib/sbi/sbi_hart.c
Source-slicevisor += thirdparty/opensbi/lib/sbi/sbi_hsm.c
Source-slicevisor += thirdparty/opensbi/lib/sbi/sbi_ipi.c
Source-slicevisor += thirdparty/opensbi/lib/sbi/sbi_scratch.c
Source-slicevisor += thirdparty/opensbi/lib/sbi/sbi_string.c
Source-slicevisor += thirdparty/opensbi/platform/slice/slice_cache.c
Source-slicevisor += modules/misc/csr_helper.c
#Source-slicevisor += thirdparty/opensbi/platform/slice/mss_uart.c
Source-slicevisor += modules/misc/c_stubs.c 
#Source-slicevisor += envm-wrapper/envm-wrapper_stubs.c
Source-slicevisor += modules/misc/assert.c  
Source-slicevisor	 += modules/misc/stack_guard.c 
Source-slicevisor +=  modules/debug/hss_debug.c
Source-slicevisor +=  application/hart0/hss_clock.c
#Source-slicevisor += services/boot/hss_boot_service.c
#Source-slicevisor += modules/ssmb/ipi/ssmb_ipi.c

all:
	mkdir -p slicevisor
	@echo "hi"
	riscv64-unknown-linux-gnu-gcc -O0 \
-ftest-coverage  -DCONFIG_PLATFORM_MPFS=1\
 -DCONFIG_SLICE_SW_RESET=1 -DCONFIG_SLICE=1 -DCONFIG=TINY_TCB \
 -DFW_PIC=y -o slicevisor/slicevisor -I./boards/slice/fpga_design_config/\
 $(Source-slicevisor) -I./boards/mpfs-icicle-kit-es/ \
  -I./thirdparty/opensbi/platform/slice/\
  -I./boards/slice baremetal/polarfire-soc-bare-metal-library/src/platform/mpfs_hal/common/mss_irq_handler_stubs.c\
   -pipe -grecord-gcc-switches -fno-builtin -fno-builtin-printf -fomit-frame-pointer -Wredundant-decls -Wundef -Wwrite-strings\
    -fno-strict-aliasing -fno-common -Wendif-labels -Wmissing-include-dirs -Wempty-body -Wformat=2 -Wformat-security -Wformat-y2k\
	 -Winit-self -Wold-style-declaration -Wtype-limits -mno-fdiv -fms-extensions -nostdlib -ffunction-sections -fdata-sections \
	 -fstack-protector-strong -DCONFIG_PLATFORM_MPFS=1 -DCONFIG_SLICE_SW_RESET=1 -DCONFIG_SLICE=1 -DFW_PIC=y  -DROLLOVER_TEST=0 \
   -I./ -I./thirdparty/opensbi/include/ -I./thirdparty/opensbi/include/sbi  \
   -I./thirdparty/opensbi/include/sbi_utils -I./thirdparty/opensbi/lib/utils/libfdt/ \
    -I./thirdparty/opensbi/include/slice -I./include/ -I./modules/ssmb/ipi/ -I./  \
	-I./baremetal/polarfire-soc-bare-metal-library/src/platform/mpfs_hal/startup_gcc/ \
	-I./services/boot/ -I./services/sgdma -I./services/usbdmsc/ -I./services/opensbi -I./modules/debug/ \
	-I./baremetal/polarfire-soc-bare-metal-library/src/platform/mpfs_hal/common  \
	-I./baremetal/polarfire-soc-bare-metal-library/src/platform    -Wl,--gc-sections  -Wl,-eslicevisor_main

c:
	mkdir -p slicevisor/c
	cmd = $(foreach obj,$(Source-slicevisor),cp /home/ziqiao/riscv/slice-hss/$(obj) slicevisor/c/;)
	echo $(cmd)
	#cmd = $(foreach obj,$(HEADER-slicevisor),cp /home/ziqiao/riscv/slice-hss/$(obj) sliceloader/c/;)
	#echo $(cmd)

list:
	mkdir -p slicevisor
	@echo "hi"
	riscv64-unknown-linux-gnu-gcc -O0 -H -M \
-ftest-coverage  -DCONFIG_PLATFORM_MPFS=1\
 -DCONFIG_SLICE_SW_RESET=1 -DCONFIG_SLICE=1 \
 -DFW_PIC=y -o slicevisor/slicevisor -I./boards/slice/fpga_design_config/\
 $(Source-slicevisor) -I./boards/mpfs-icicle-kit-es/ \
  -I./boards/slice baremetal/polarfire-soc-bare-metal-library/src/platform/mpfs_hal/common/mss_irq_handler_stubs.c\
   -pipe -grecord-gcc-switches -fno-builtin -fno-builtin-printf -fomit-frame-pointer -Wredundant-decls -Wundef -Wwrite-strings\
    -fno-strict-aliasing -fno-common -Wendif-labels -Wmissing-include-dirs -Wempty-body -Wformat=2 -Wformat-security -Wformat-y2k\
	 -Winit-self -Wold-style-declaration -Wtype-limits -mno-fdiv -fms-extensions -nostdlib -ffunction-sections -fdata-sections \
	 -fstack-protector-strong -DCONFIG_PLATFORM_MPFS=1 -DCONFIG_SLICE_SW_RESET=1 -DCONFIG_SLICE=1 -DFW_PIC=y  -DROLLOVER_TEST=0 \
   -I./ -I./thirdparty/opensbi/include/ -I./thirdparty/opensbi/include/sbi  \
   -I./thirdparty/opensbi/include/sbi_utils -I./thirdparty/opensbi/lib/utils/libfdt/ \
    -I./thirdparty/opensbi/include/slice -I./include/ -I./modules/ssmb/ipi/ -I./  \
	-I./baremetal/polarfire-soc-bare-metal-library/src/platform/mpfs_hal/startup_gcc/ \
	-I./services/boot/ -I./services/sgdma -I./services/usbdmsc/ -I./services/opensbi -I./modules/debug/ \
	-I./baremetal/polarfire-soc-bare-metal-library/src/platform/mpfs_hal/common  \
	-I./baremetal/polarfire-soc-bare-metal-library/src/platform    -Wl,--gc-sections  -Wl,-eslicevisor_main