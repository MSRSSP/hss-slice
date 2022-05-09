#
# MPFS HSS Embedded Software
#
# Copyright 2019-2021 Microchip Corporation.
#
# SPDX-License-Identifier: MIT
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to
# deal in the Software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
# sell copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.
#
#
# OpenSPI Service

SRCS-$(CONFIG_SERVICE_OPENSBI) += \
	services/opensbi/opensbi_service.c \


SRCS-$(CONFIG_SERVICE_OPENSBI_IHC) += \
	services/opensbi/opensbi_ihc_ecall.c \

ifeq ($(ENV), QEMU)
SBI_PLATFORM_CFLAGS = -DCONFIG_QEMU=1 -DCONFIG_SLICE_SW_RESET=0
SBI_BUILD_DIR = build-qemu
else
SBI_PLATFORM_CFLAGS = -DCONFIG_SLICE_SW_RESET=1
SBI_BUILD_DIR = build
endif


libsbi-objs-y += riscv_asm.o
libsbi-objs-y += riscv_atomic.o
libsbi-objs-y += riscv_locks.o

libsbi-objs-y += sbi_bitmap.o
libsbi-objs-y += sbi_bitops.o
libsbi-objs-y += sbi_console.o
libsbi-objs-y += sbi_domain.o
libsbi-objs-y += sbi_hart.o
libsbi-objs-y += sbi_math.o
libsbi-objs-y += sbi_hsm.o
libsbi-objs-y += sbi_init_count.o
libsbi-objs-y += sbi_ipi.o
libsbi-objs-y += sbi_platform.o
libsbi-objs-y += sbi_scratch.o
libsbi-objs-y += sbi_string.o
libsbi-objs-y += sbi_system.o
libsbi-objs-y += sbi_timer.o
libslice-objs-y += slice.o
libslice-objs-y += slice_smp.o
libslice-objs-y += slice_pmp.o
libslice-objs-y += slice_mgr.o
libslice-objs-y += slice_state.o
libslice-objs-y += slice_reset.o
libslice-objs-y += slice_mem.o

thirdparty/opensbi/$(SBI_BUILD_DIR)/lib/slice/%.o: thirdparty/opensbi/lib/slice/%.c
	$(CC) $(CFLAGS) $(INCLUDES) $(OPT-y) thirdparty/opensbi/lib/slice/$*.c -c -o $@

LIBS += $(foreach obj,$(libsbi-objs-y),thirdparty/opensbi/$(SBI_BUILD_DIR)/lib/sbi/$(obj))
LIBS += $(foreach obj,$(libslice-objs-y),thirdparty/opensbi/$(SBI_BUILD_DIR)/lib/slice/$(obj))
fdt-objs += lib/utils/sys/clint.o
fdt-objs += lib/utils/irqchip/plic.o
fdt-objs += lib/utils/fdt/fdt_domain_data.o

LIBS += $(foreach obj,$(fdt-objs),thirdparty/opensbi/$(SBI_BUILD_DIR)/$(obj))

L2LIM_SLICE_DEP += thirdparty/opensbi/$(SBI_BUILD_DIR)/platform/slice/firmware/slice_fw_dynamic.bin

thirdparty/opensbi/$(SBI_BUILD_DIR)/platform/slice/firmware/slice_fw_dynamic.bin: slice_fw_dynamic


slice_fw_dynamic: config.h
	make -C thirdparty/opensbi/ CROSS_COMPILE=$(CROSS_COMPILE) PLATFORM="slice" SLICE_HSS_DIR=$(shell pwd) FW_PIC=y -j16 O=$(SBI_BUILD_DIR)

application/crt.o: slice_fw_dynamic

ifdef CONFIG_SLICE
#SRCS-$(CONFIG_SERVICE_OPENSBI) += 
#	thirdparty/opensbi/build/platform/slice/platform.o
libslice-objs-y += ../../platform/slice/platform.o
libslice-objs-y += ../../platform/slice/slice_uart_helper.o
SRCS-$(CONFIG_SERVICE_OPENSBI)  += thirdparty/opensbi/platform/slice/slice_cache.c

#services/opensbi/slice-platform.c
else
SRCS-$(CONFIG_SERVICE_OPENSBI) += \
	services/opensbi/platform.c
	services/opensbi/opensbi_ecall.c
endif

ifdef CONFIG_OPENSBI
INCLUDES += \
	-I./services/opensbi \
	-I./thirdparty/opensbi/include/sbi \
	-I./thirdparty/opensbi/include \
	-I./thirdparty/opensbi/lib/utils/libfdt
endif

ifdef CONFIG_PROVIDE_DTB
ifdef CONFIG_DEFAULT_DEVICE_TREE
EXTRA_OBJS += services/opensbi/mpfs_dtb.o
services/opensbi/mpfs_dtb.o: $(CONFIG_DEFAULT_DEVICE_TREE:"%"=%).dtb
	$(CROSS_COMPILE)ld -r -b binary -o $@ $<
endif
endif

services/opensbi/opensbi_service.o: CFLAGS=$(CFLAGS_GCCEXT)
services/opensbi/opensbi_ihc_ecall.o: CFLAGS=$(CFLAGS_GCCEXT)
services/opensbi/platform.o: CFLAGS=$(CFLAGS_GCCEXT)
thirdparty/opensbi/lib/utils/sys/clint.o: CFLAGS=$(CFLAGS_GCCEXT)
thirdparty/opensbi/lib/utils/irqchip/plic.o: CFLAGS=$(CFLAGS_GCCEXT)
thirdparty/opensbi/lib/utils/libfdt/fdt_rw.o: CFLAGS=$(CFLAGS_GCCEXT)
thirdparty/opensbi/lib/utils/libfdt/fdt_ro.o: CFLAGS=$(CFLAGS_GCCEXT)
thirdparty/opensbi/lib/utils/libfdt/fdt.o: CFLAGS=$(CFLAGS_GCCEXT)
thirdparty/opensbi/lib/utils/libfdt/fdt_overlay.o: CFLAGS=$(CFLAGS_GCCEXT)
thirdparty/opensbi/lib/utils/libfdt/fdt_strerror.o: CFLAGS=$(CFLAGS_GCCEXT)
thirdparty/opensbi/lib/utils/libfdt/fdt_sw.o: CFLAGS=$(CFLAGS_GCCEXT)
thirdparty/opensbi/lib/utils/libfdt/fdt_addresses.o: CFLAGS=$(CFLAGS_GCCEXT)
thirdparty/opensbi/lib/utils/libfdt/fdt_wip.o: CFLAGS=$(CFLAGS_GCCEXT)
thirdparty/opensbi/lib/utils/serial/sifive-uart.o: CFLAGS=$(CFLAGS_GCCEXT)
thirdparty/opensbi/platform/sifive/fu540/platform.o: CFLAGS=$(CFLAGS_GCCEXT)
thirdparty/opensbi/lib/sbi/riscv_asm.o: CFLAGS=$(CFLAGS_GCCEXT)
thirdparty/opensbi/lib/sbi/riscv_atomic.o: CFLAGS=$(CFLAGS_GCCEXT)
thirdparty/opensbi/lib/sbi/riscv_locks.o: CFLAGS=$(CFLAGS_GCCEXT)
thirdparty/opensbi/lib/sbi/sbi_bitmap.o: CFLAGS=$(CFLAGS_GCCEXT)
thirdparty/opensbi/lib/sbi/sbi_bitops.o: CFLAGS=$(CFLAGS_GCCEXT)
thirdparty/opensbi/lib/sbi/sbi_console.o: CFLAGS=$(CFLAGS_GCCEXT)
thirdparty/opensbi/lib/sbi/sbi_domain.o: CFLAGS=$(CFLAGS_GCCEXT)
thirdparty/opensbi/lib/sbi/sbi_ecall_base.o: CFLAGS=$(CFLAGS_GCCEXT)
thirdparty/opensbi/lib/sbi/sbi_ecall.o: CFLAGS=$(CFLAGS_GCCEXT)
thirdparty/opensbi/lib/sbi/sbi_ecall_hsm.o: CFLAGS=$(CFLAGS_GCCEXT)
thirdparty/opensbi/lib/sbi/sbi_ecall_legacy.o: CFLAGS=$(CFLAGS_GCCEXT)
thirdparty/opensbi/lib/sbi/sbi_ecall_replace.o: CFLAGS=$(CFLAGS_GCCEXT)
thirdparty/opensbi/lib/sbi/sbi_ecall_vendor.o: CFLAGS=$(CFLAGS_GCCEXT)
thirdparty/opensbi/lib/sbi/sbi_emulate_csr.o: CFLAGS=$(CFLAGS_GCCEXT)
thirdparty/opensbi/lib/sbi/sbi_fifo.o: CFLAGS=$(CFLAGS_GCCEXT)
thirdparty/opensbi/lib/sbi/sbi_hart.o: CFLAGS=$(CFLAGS_GCCEXT)
thirdparty/opensbi/lib/sbi/sbi_hsm.o: CFLAGS=$(CFLAGS_GCCEXT)
thirdparty/opensbi/lib/sbi/sbi_illegal_insn.o: CFLAGS=$(CFLAGS_GCCEXT)
thirdparty/opensbi/lib/sbi/sbi_init.o: CFLAGS=$(CFLAGS_GCCEXT)
thirdparty/opensbi/lib/sbi/sbi_ipi.o: CFLAGS=$(CFLAGS_GCCEXT)
thirdparty/opensbi/lib/sbi/sbi_math.o: CFLAGS=$(CFLAGS_GCCEXT)
thirdparty/opensbi/lib/sbi/sbi_misaligned_ldst.o: CFLAGS=$(CFLAGS_GCCEXT)
thirdparty/opensbi/lib/sbi/sbi_platform.o: CFLAGS=$(CFLAGS_GCCEXT)
thirdparty/opensbi/lib/sbi/sbi_scratch.o: CFLAGS=$(CFLAGS_GCCEXT)
thirdparty/opensbi/lib/sbi/sbi_string.o: CFLAGS=$(CFLAGS_GCCEXT)
thirdparty/opensbi/lib/sbi/sbi_system.o: CFLAGS=$(CFLAGS_GCCEXT)
thirdparty/opensbi/lib/sbi/sbi_timer.o: CFLAGS=$(CFLAGS_GCCEXT)
thirdparty/opensbi/lib/sbi/sbi_tlb.o: CFLAGS=$(CFLAGS_GCCEXT)
thirdparty/opensbi/lib/sbi/sbi_trap.o: CFLAGS=$(CFLAGS_GCCEXT)
thirdparty/opensbi/lib/sbi/sbi_unpriv.o: CFLAGS=$(CFLAGS_GCCEXT)
