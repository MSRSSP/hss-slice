/*
 * The HSS software is released under the following software license:
 *
 * Copyright 2019-2022 Microchip FPGA Embedded Systems Solutions.
 *
 * SPDX-License-Identifier: MIT
 *
 * Based on OpenSBI fw_base.S
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Western Digital Corporation or its affiliates.
 */

#include <config.h>

#include <sbi/riscv_asm.h>
#include <sbi/riscv_elf.h>
#include <sbi/riscv_encoding.h>
#include <sbi/sbi_platform.h>
#include <sbi/sbi_scratch.h>
#include <sbi/sbi_trap.h>

	.section .entry, "ax", %progbits
	.attribute unaligned_access, 0
	.attribute stack_align, 16
	.align	3
	.globl	_start
	.globl	_start_warm
_start:
// a0: boot_hartid. System-wide boot if a0=0 else slice-wide boot
	call .__slice_init
	la	ra, _start
	call	_reset_regs

#define __STACK_SIZE_PER_HART 4000
.macro LOAD_GP
        //
        // RISC-V Relaxing and the Global Pointer, GP
        //
        // The gp (Global Pointer) register optimises memory accesses within a single 4KB region.
        // This size is 4K because RISC-V immediate values are 12-bit signed values (+/- 2048)
        //
        // The linker uses the __global_pointer$ symbol definition to compare memory addresses
        // and, if within range, it replaces absolute/pc-relative addressing with gp-relative
        // addressing.
        //
        // This process can be disabled by -Wl,--no-relax.
        //
#if defined(CONFIG_LD_RELAX)
.option push
.option norelax
        la	gp, __global_pointer$
.option pop
#endif
.endm

	LOAD_GP
.relocate:
#define BOOT_STATUS_RELOCATE_DONE	1
.macro BRANGE __start_reg, __end_reg, __check_reg, __jump_lable
	blt	\__check_reg, \__start_reg, 999f
	bge	\__check_reg, \__end_reg, 999f
	j	\__jump_lable
999:
.endm

_try_lottery:
	/* Jump to relocation wait loop if we don't get relocation lottery */
	lla	 s1, boot_hartid
	REG_L s1, (s1)
	csrr	s2, CSR_MHARTID
	bne s1, s2, _wait_relocate_copy_done
	/* Save load address */
	lla	t0, _load_start
	lla	t1, _hss_start
	REG_S	t1, 0(t0)
#ifdef FW_PIC
	/* relocate the global table content */
	lla	t0, _link_start
	REG_L	t0, 0(t0)
	/* t1 shall has the address of _hss_start */
	sub	t2, t1, t0
	lla	t3, _runtime_offset
	REG_S	t2, (t3)
	lla	t0, __rel_dyn_start
	lla	t1, __rel_dyn_end
	beq	t0, t1, _relocate_done
	j	5f
2:
	REG_L	t5, -(REGBYTES*2)(t0)	/* t5 <-- relocation info:type */
	li	t3, R_RISCV_RELATIVE	/* reloc type R_RISCV_RELATIVE */
	bne	t5, t3, 3f
	REG_L	t3, -(REGBYTES*3)(t0)
	REG_L	t5, -(REGBYTES)(t0)	/* t5 <-- addend */
	add	t5, t5, t2
	add	t3, t3, t2
	REG_S	t5, 0(t3)		/* store runtime address to the GOT entry */
	j	5f

3:
	lla	t4, __dyn_sym_start

4:
	REG_L	t5, -(REGBYTES*2)(t0)	/* t5 <-- relocation info:type */
	srli	t6, t5, SYM_INDEX	/* t6 <--- sym table index */
	andi	t5, t5, 0xFF		/* t5 <--- relocation type */
	li	t3, RELOC_TYPE
	bne	t5, t3, 5f

	/* address R_RISCV_64 or R_RISCV_32 cases*/
	REG_L	t3, -(REGBYTES*3)(t0)
	li	t5, SYM_SIZE
	mul	t6, t6, t5
	add	s5, t4, t6
	REG_L	t6, -(REGBYTES)(t0)	/* t0 <-- addend */
	REG_L	t5, REGBYTES(s5)
	add	t5, t5, t6
	add	t5, t5, t2		/* t5 <-- location to fix up in RAM */
	add	t3, t3, t2		/* t3 <-- location to fix up in RAM */
	REG_S	t5, 0(t3)		/* store runtime address to the variable */

5:
	addi	t0, t0, (REGBYTES*3)
	ble	t0, t1, 2b
	j	_relocate_done
_wait_relocate_copy_done:
	j	_wait_for_boot_hart
#else
	/* Relocate if load address != link address */
_relocate:
	lla	t0, _link_start
	REG_L	t0, 0(t0)
	lla	t1, _link_end
	REG_L	t1, 0(t1)
	lla	t2, _load_start
	REG_L	t2, 0(t2)
	sub	t3, t1, t0
	add	t3, t3, t2
	beq	t0, t2, _relocate_done
	lla	t4, _relocate_done
	sub	t4, t4, t2
	add	t4, t4, t0
	blt	t2, t0, _relocate_copy_to_upper
_relocate_copy_to_lower:
	ble	t1, t2, _relocate_copy_to_lower_loop
	lla	t3, _relocate_lottery
	BRANGE	t2, t1, t3, _start_hang
	lla	t3, _relocate_status
	BRANGE	t2, t1, t3, _start_hang
	lla	t3, _relocate
	lla	t5, _relocate_done
	BRANGE	t2, t1, t3, _start_hang
	BRANGE	t2, t1, t5, _start_hang
	BRANGE  t3, t5, t2, _start_hang
_relocate_copy_to_lower_loop:
	REG_L	t3, 0(t2)
	REG_S	t3, 0(t0)
	add	t0, t0, __SIZEOF_POINTER__
	add	t2, t2, __SIZEOF_POINTER__
	blt	t0, t1, _relocate_copy_to_lower_loop
	jr	t4
_relocate_copy_to_upper:
	ble	t3, t0, _relocate_copy_to_upper_loop
	lla	t2, _relocate_lottery
	BRANGE	t0, t3, t2, _start_hang
	lla	t2, _relocate_status
	BRANGE	t0, t3, t2, _start_hang
	lla	t2, _relocate
	lla	t5, _relocate_done
	BRANGE	t0, t3, t2, _start_hang
	BRANGE	t0, t3, t5, _start_hang
	BRANGE	t2, t5, t0, _start_hang
_relocate_copy_to_upper_loop:
	add	t3, t3, -__SIZEOF_POINTER__
	add	t1, t1, -__SIZEOF_POINTER__
	REG_L	t2, 0(t3)
	REG_S	t2, 0(t1)
	blt	t0, t1, _relocate_copy_to_upper_loop
	jr	t4
_wait_relocate_copy_done:
	lla	t0, _hss_start
	lla	t1, _link_start
	REG_L	t1, 0(t1)
	beq	t0, t1, _wait_for_boot_hart
	lla	t2, _relocate_status
	lla	t3, _wait_for_boot_hart
	sub	t3, t3, t0
	add	t3, t3, t1
1:
	/* waitting for relocate copy done (_relocate_status == 1) */
	li	t4, BOOT_STATUS_RELOCATE_DONE
	REG_L	t5, 0(t2)
	/* Reduce the bus traffic so that boot hart may proceed faster */
	nop
	nop
	nop
	bgt     t4, t5, 1b
	jr	t3
#endif
_relocate_done:

	/*
	 * Mark relocate copy done
	 * Use _relocate_status copy relative to the load address
	 */
	lla	t0, _relocate_status
#ifndef FW_PIC
	lla	t1, _link_start
	REG_L	t1, 0(t1)
	lla	t2, _load_start
	REG_L	t2, 0(t2)
	sub	t0, t0, t1
	add	t0, t0, t2
#endif
	li	t1, BOOT_STATUS_RELOCATE_DONE
	REG_S	t1, 0(t0)
	fence	rw, rw

_wait_for_boot_hart:
	li	t0, BOOT_STATUS_RELOCATE_DONE
	lla	t1, _relocate_status
	REG_L	t1, 0(t1)
	/* Reduce the bus traffic so that boot hart may proceed faster */
	nop
	nop
	nop
	bne	t0, t1, _wait_for_boot_hart
	// Preload HART details
	// s7 -> HART Count
	// s8 -> HART Stack Size
        // a0 -> current HART ID
	//
	la	a4, platform
	lwu	s7, SBI_PLATFORM_HART_COUNT_OFFSET(a4)
	//lwu	s8, SBI_PLATFORM_HART_STACK_SIZE_OFFSET(a4)
	li	s8, __STACK_SIZE_PER_HART
    csrr	a0, CSR_MHARTID

    //lla	tp,  __stack_bottom + 63
	lla tp, _fw_reloc_end + 63
    and	tp, tp, -64
    li	a1, 1

.setup_stack:
	mul	a2, s8, a0
	add	tp, tp, a2
    li	sp, __STACK_SIZE_PER_HART
	add	sp, sp, tp

.setup_scratches:
	// Setup scratch space for all the HARTs
	la	tp, scratches
	li	t2, 1 // Counter
	li	t1, 0 // hartid 0 is mandated by ISA

.disable_and_clear_interrupts:
	// Disable and clear all interrupts
	csrw	CSR_MIE, zero
	csrw	CSR_MIP, zero

.validate_hart_id:
	bge	a0, s7, _start_hang

.find_my_scratch_space:
	la	tp, scratches // find the scratch space for this hart
	li	a5, SBI_SCRATCH_SIZE
	mul	a5, a5, a0
	add	tp, tp, a5
	csrw	CSR_MSCRATCH, tp // update the mscratch
	li a5, 8
	mul a5, a5, a0
	la s1, hss_start_time
	add s1, s1, a5
	csrr a0, CSR_MCYCLE
	sd a0, 0(s1) // hss_start_time[hartid] =csrr(CSR_MCYCLE)
    csrr	a0, CSR_MHARTID
    beqz	a0, .boot_e51


.setup_u54_trap_handler:
	la	a4, _trap_handler
    call	.set_trap_handler

	csrw	CSR_SATP, 0 # clear SATP early on U54s, as it appears to be coming up randomly

.jump_sbi_if_slice:
	lla	 a0, boot_hartid
	REG_L a0, (a0)
	bnez a0, __slice_sbi_jump

.enable_interrupts:
	li	a2, MIP_MSIP // set MSIE bit to receive IPIs
    csrw	CSR_MIE, a2
    csrw    CSR_MSTATUS, a2
	jal	HSS_U54_Banner

#if defined(CONFIG_HSS_USE_IHC)
.enable_ihc:
        jal	HSS_IHCInit_U54
#endif

.spin_forever:
        wfi
        j	.spin_forever

.boot_e51:
#if defined(CONFIG_HSS_USE_IHC)
        // initialise PLIC if required for IHC
        jal	HSS_Setup_PLIC
#endif

	// Setup E51 trap handler
	la	a4, hss_e51_trap_handler
        call	.set_trap_handler

        // clearing of DTIM and L2LIM done from previous decompression stage

        li	a2, MIP_MSIP
	csrc	CSR_MSTATUS, a2
	nop
	nop
	nop
        j main

.set_trap_handler:
        csrw	CSR_MTVEC, a4
	//
        // Make sure that mtvec is updated
        //
        //
	csrr    a5, CSR_MTVEC
	bne	a4, a5, .set_trap_handler
	ret

	.section .entry, "ax", %progbits
	.align 3
	.globl _start_hang
_start_hang:
	wfi
	j	_start_hang

	.section .entry, "ax", %progbits
	.align 3
	.globl _trap_handler
_trap_handler:
	// Swap TP and MSCRATCH
	csrrw	tp, CSR_MSCRATCH, tp

	// Save T0 in scratch space
	REG_S	t0, SBI_SCRATCH_TMP0_OFFSET(tp)

	// Check which mode we came from
	csrr	t0, CSR_MSTATUS
	srl	t0, t0, MSTATUS_MPP_SHIFT
	and	t0, t0, PRV_M
	xori	t0, t0, PRV_M
	beq	t0, zero, _trap_handler_m_mode

	// We came from S-mode or U-mode
_trap_handler_s_mode:
	// Set T0 to original SP
	add	t0, sp, zero

	// Setup exception stack
	add	sp, tp, -(SBI_TRAP_REGS_SIZE)

	// Jump to code common for all modes
	j	_trap_handler_all_mode

	// We came from M-mode
_trap_handler_m_mode:
	// Set T0 to original SP
	add	t0, sp, zero

	// Re-use current SP as exception stack
	add	sp, sp, -(SBI_TRAP_REGS_SIZE)

_trap_handler_all_mode:
	// Save original SP (from T0) on stack
	REG_S	t0, SBI_TRAP_REGS_OFFSET(sp)(sp)

	// Restore T0 from scratch space
	REG_L	t0, SBI_SCRATCH_TMP0_OFFSET(tp)

	// Save T0 on stack
	REG_S	t0, SBI_TRAP_REGS_OFFSET(t0)(sp)

	// Swap TP and MSCRATCH
	csrrw	tp, CSR_MSCRATCH, tp

	// Save MEPC and MSTATUS CSRs
	csrr	t0, CSR_MEPC
	REG_S	t0, SBI_TRAP_REGS_OFFSET(mepc)(sp)
	csrr	t0, CSR_MSTATUS
	REG_S	t0, SBI_TRAP_REGS_OFFSET(mstatus)(sp)
	REG_S	zero, SBI_TRAP_REGS_OFFSET(mstatusH)(sp)

	// Save all general regisers except SP and T0
	//REG_S	zero, SBI_TRAP_REGS_OFFSET(zero)(sp)
	REG_S	ra, SBI_TRAP_REGS_OFFSET(ra)(sp)
	REG_S	gp, SBI_TRAP_REGS_OFFSET(gp)(sp)
	REG_S	tp, SBI_TRAP_REGS_OFFSET(tp)(sp)
	REG_S	t1, SBI_TRAP_REGS_OFFSET(t1)(sp)
	REG_S	t2, SBI_TRAP_REGS_OFFSET(t2)(sp)
	REG_S	s0, SBI_TRAP_REGS_OFFSET(s0)(sp)
	REG_S	s1, SBI_TRAP_REGS_OFFSET(s1)(sp)
	REG_S	a0, SBI_TRAP_REGS_OFFSET(a0)(sp)
	REG_S	a1, SBI_TRAP_REGS_OFFSET(a1)(sp)
	REG_S	a2, SBI_TRAP_REGS_OFFSET(a2)(sp)
	REG_S	a3, SBI_TRAP_REGS_OFFSET(a3)(sp)
	REG_S	a4, SBI_TRAP_REGS_OFFSET(a4)(sp)
	REG_S	a5, SBI_TRAP_REGS_OFFSET(a5)(sp)
	REG_S	a6, SBI_TRAP_REGS_OFFSET(a6)(sp)
	REG_S	a7, SBI_TRAP_REGS_OFFSET(a7)(sp)
	REG_S	s2, SBI_TRAP_REGS_OFFSET(s2)(sp)
	REG_S	s3, SBI_TRAP_REGS_OFFSET(s3)(sp)
	REG_S	s4, SBI_TRAP_REGS_OFFSET(s4)(sp)
	REG_S	s5, SBI_TRAP_REGS_OFFSET(s5)(sp)
	REG_S	s6, SBI_TRAP_REGS_OFFSET(s6)(sp)
	REG_S	s7, SBI_TRAP_REGS_OFFSET(s7)(sp)
	REG_S	s8, SBI_TRAP_REGS_OFFSET(s8)(sp)
	REG_S	s9, SBI_TRAP_REGS_OFFSET(s9)(sp)
	REG_S	s10, SBI_TRAP_REGS_OFFSET(s10)(sp)
	REG_S	s11, SBI_TRAP_REGS_OFFSET(s11)(sp)
	REG_S	t3, SBI_TRAP_REGS_OFFSET(t3)(sp)
	REG_S	t4, SBI_TRAP_REGS_OFFSET(t4)(sp)
	REG_S	t5, SBI_TRAP_REGS_OFFSET(t5)(sp)
	REG_S	t6, SBI_TRAP_REGS_OFFSET(t6)(sp)

	LOAD_GP

	// Call C routine
	csrr 	a0, CSR_MCAUSE
	sext.w	a0, a0
	blt	a0, zero, .checkIfOpenSBITrap

.checkIfHssIpi:
	call 	HSS_U54_HandleIPI 	// check if it is a HSS IPI
	bnez	a0, .skipOpenSbi

.checkIfOpenSBITrap:
	add	a0, sp, zero
	csrr	a1, CSR_MSCRATCH
	#call	sbi_trap_handler

.skipOpenSbi:
	// Restore all general regisers except SP and T0
	REG_L	ra, SBI_TRAP_REGS_OFFSET(ra)(sp)
	REG_L	gp, SBI_TRAP_REGS_OFFSET(gp)(sp)
	REG_L	tp, SBI_TRAP_REGS_OFFSET(tp)(sp)
	REG_L	t1, SBI_TRAP_REGS_OFFSET(t1)(sp)
	REG_L	t2, SBI_TRAP_REGS_OFFSET(t2)(sp)
	REG_L	s0, SBI_TRAP_REGS_OFFSET(s0)(sp)
	REG_L	s1, SBI_TRAP_REGS_OFFSET(s1)(sp)
	REG_L	a0, SBI_TRAP_REGS_OFFSET(a0)(sp)
	REG_L	a1, SBI_TRAP_REGS_OFFSET(a1)(sp)
	REG_L	a2, SBI_TRAP_REGS_OFFSET(a2)(sp)
	REG_L	a3, SBI_TRAP_REGS_OFFSET(a3)(sp)
	REG_L	a4, SBI_TRAP_REGS_OFFSET(a4)(sp)
	REG_L	a5, SBI_TRAP_REGS_OFFSET(a5)(sp)
	REG_L	a6, SBI_TRAP_REGS_OFFSET(a6)(sp)
	REG_L	a7, SBI_TRAP_REGS_OFFSET(a7)(sp)
	REG_L	s2, SBI_TRAP_REGS_OFFSET(s2)(sp)
	REG_L	s3, SBI_TRAP_REGS_OFFSET(s3)(sp)
	REG_L	s4, SBI_TRAP_REGS_OFFSET(s4)(sp)
	REG_L	s5, SBI_TRAP_REGS_OFFSET(s5)(sp)
	REG_L	s6, SBI_TRAP_REGS_OFFSET(s6)(sp)
	REG_L	s7, SBI_TRAP_REGS_OFFSET(s7)(sp)
	REG_L	s8, SBI_TRAP_REGS_OFFSET(s8)(sp)
	REG_L	s9, SBI_TRAP_REGS_OFFSET(s9)(sp)
	REG_L	s10, SBI_TRAP_REGS_OFFSET(s10)(sp)
	REG_L	s11, SBI_TRAP_REGS_OFFSET(s11)(sp)
	REG_L	t3, SBI_TRAP_REGS_OFFSET(t3)(sp)
	REG_L	t4, SBI_TRAP_REGS_OFFSET(t4)(sp)
	REG_L	t5, SBI_TRAP_REGS_OFFSET(t5)(sp)
	REG_L	t6, SBI_TRAP_REGS_OFFSET(t6)(sp)

	// Restore MEPC and MSTATUS CSRs
	REG_L	t0, SBI_TRAP_REGS_OFFSET(mepc)(sp)
	csrw	CSR_MEPC, t0
	REG_L	t0, SBI_TRAP_REGS_OFFSET(mstatus)(sp)
	csrw	CSR_MSTATUS, t0

	// Restore T0
	REG_L	t0, SBI_TRAP_REGS_OFFSET(t0)(sp)

	// Restore SP
	REG_L	sp, SBI_TRAP_REGS_OFFSET(sp)(sp)

	mret

	.section .entry, "ax", %progbits
	.align	3
	.globl	_reset_regs
_reset_regs:
	// flush the instruction cache
	fence.i

	// Reset all registers except ra, a0, a1 and a2
	li gp,	0
	li sp,	0
	li tp,	0
	li t0,	0
	li t1,	0
	li t2,	0
	li s0,	0
	li s1,	0
	li a3,	0
	li a4,	0
	li a5,	0
	li a6,	0
	li a7,	0
	li s2,	0
	li s3,	0
	li s4,	0
	li s5,	0
	li s6,	0
	li s7,	0
	li s8,	0
	li s9,	0
	li s10,	0
	li s11,	0
	li t3,	0
	li t4,	0
	li t5,	0
	li t6,	0
	csrw	CSR_MSCRATCH, 0

	ret

	.section .entry, "ax", %progbits
	.align	3
	.globl	hss_e51_trap_handler
hss_e51_trap_handler:
        wfi
        j	hss_e51_trap_handler

/***********************************************************************************
 *
 * The following init_memory() symbol overrides the weak symbol in the HAL and does
 * a safe copy of RW data and clears zero-init memory
 *
 */
        // zero_section helper function:
        //       a0 = exec_start_addr
        //       a1 = exec_end_addr
        //
	.type	.zero_section, @function
.zero_section:
	beq	a0, a1, .zero_section_done
	sd	zero, (a0)
	addi	a0, a0, 8
	j	.zero_section
.zero_section_done:
	ret

        // copy_section helper function:
        //	a0 = load_addr
        //	a1 = exec_start_addr
        //	a2 = exec_end_addr
	.globl	copy_section
	.type	copy_section, @function
copy_section:
        beq     a1, a0, .copy_section_done // if load_addr == exec_start_addr, goto copy_section_done
.check_if_copy_section_done:
	beq	a1, a2, .copy_section_done // if offset != length, goto keep_copying
.keep_copying:
	ld	a3, 0(a0)                  // val = *load_addr
	sd	a3, 0(a1)                  // *exec_start_addr = val;
	addi	a0, a0, 8                  // load_addr = load_addr + 8
	addi	a1, a1, 8                  // exec_start_addr = exec_start_addr + 8
        j	.check_if_copy_section_done
.copy_section_done:
        ret

        // init_memory function, used to initialize memory early before C code runs
        //
	.globl	init_memory
	.type	init_memory, @function
init_memory:
	addi	sp,sp,-16
	sd	ra,8(sp)

        //
        // Initialize R/W data
        //  (sdata and data sections)
        //
        la	a0, __sdata_load
        la	a1, __sdata_start
        la	a2, __sdata_end
	call	copy_section

        la	a0, __data_load
        la	a1, __data_start
        la	a2, __data_end
	call	copy_section

        //
        // Clear zero-init memory
        //  (SBSS and BSS sections)
        //
        la	a0, __sbss_start
        la	a1, __sbss_end
	call	.zero_section

        la	a0, __bss_start
        la	a1, __bss_end

	ld	ra,8(sp)
	addi	sp,sp,16
	tail	.zero_section

/***********************************************************************************
 *
 * The following copy_switch_code() symbol overrides the weak symbol in the HAL and does
 * a safe copy of HW config data
 */
	.globl	copy_switch_code
	.type	copy_switch_code, @function
copy_switch_code:
        la      a5, __sc_start               // a5 = __sc_start
        la      a4, __sc_load                // a4 = __sc_load
	beq	a5,a4,.copy_switch_code_done // if a5 == a4, goto copy_switch_code_done
        la      a3, __sc_end                 // a3 = __sc_end
	beq	a5,a3,.copy_switch_code_done // if a5 == a3, goto copy_switch_code_done
.copy_switch_code_loop:
	lw	a2,0(a4)                     // a2 = *a4
	sw	a2,0(a5)                     // *a5 = a2
	addi	a5,a5,4                      // a5+=4
	addi	a4,a4,4                      // a4+=4

	bltu	a5,a3,.copy_switch_code_loop // if a5 < a3, goto copy_switch_code_loop
.copy_switch_code_done:
	ret

/***********************************************************************************
 *
 */

.clear_l2lim:
	// Clear the LIM
	//
	// On reset, the first 15 ways are L2 and the last way is cache
	// We can initialize all, as cache write through to DDR is blocked
	// until DDR in initialized, so will have no effect other than clear ECC
	//
	// NOTE: we need to check if we are debugging from LIM,if so do not initialize
	//
	la	a2, _hss_start
	la 	a4, __l2lim_start
	beq	a2, a4, .done_clear
	la	a5, __l2lim_end
        j	1f
.clear_dtim:
        //
        // Clear the E51 DTIM to prevent any memory errors on initial access
        // to the cache
        //
        la	a4, __dtim_start
        la	a5, __dtim_end
1:
        // common loop used by both .clear_l2lim and .clear_dtim
	REG_S	x0, 0(a4)
	add	a4, a4, __SIZEOF_POINTER__
	blt	a4, a5, 1b
.done_clear:
	ret

__slice_sbi_jump:
	/* At this point we are running from link address */
	/*csrw    CSR_MSTATUS, zero
	csrr	a1, CSR_MSCRATCH
	la 		a2, SBI_SCRATCH_SIZE
    add 	a2, a2, a1
	call	copy_section
	*/
	csrr	a0, CSR_MHARTID
	li a1, 0
	call	HSS_OpenSBI_DoBoot
	ret

.__slice_init:	
	lla s2, boot_hartid
	REG_S a0, (s2)
	lla s3, _slice_fw_start
	lla s4, _slice_fw_end
_slice_init_done:
	ret

.align 4
_slice_sbi_boot:
	RISCV_PTR 0x0

#ifdef FW_PIC
_runtime_offset:
	RISCV_PTR	0
#endif

	.section .data
	.align 8

_load_start:
	RISCV_PTR	0x8000000
_link_start:
	RISCV_PTR	0x8000000
_link_end:
	RISCV_PTR	_fw_reloc_end
.globl _relocate_status
_relocate_status:
	RISCV_PTR	0
.globl _relocate_lottery
_relocate_lottery:
	RISCV_PTR	0
boot_hartid:
	RISCV_PTR	0

.section .text, "ax", @progbits
	.option push
	.option norelax
	.align 4
	.globl	_slice_fw_start
_slice_fw_start:
#ifndef CONFIG_QEMU
	.incbin	"thirdparty/opensbi/build/platform/slice/firmware/slice_fw_dynamic.bin"
#else
	.incbin	"thirdparty/opensbi/build-qemu/platform/slice/firmware/slice_fw_dynamic.bin"
#endif
	.globl	_slice_fw_end
_slice_fw_end:
	RISCV_PTR	0

