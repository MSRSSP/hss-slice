/*******************************************************************************
 * Copyright 2019-2022 Microchip FPGA Embedded Systems Solutions.
 *
 * SPDX-License-Identifier: MIT
 *
 * MPFS HSS Embedded Software
 *
 */

/*!
 * \file Goto State Machine
 * \brief U54 opensbi
 */

#include "config.h"
#include "hss_types.h"
#include "hss_state_machine.h"
#include "hss_debug.h"
#include "hss_atomic.h"
#include "hss_clock.h"

#include "ssmb_ipi.h"

#include <string.h>
#include <assert.h>

#include "csr_helper.h"

#include "ssmb_ipi.h"

#include "opensbi_service.h"
#include "opensbi_ecall.h"
#include "riscv_encoding.h"

#if IS_ENABLED(CONFIG_SERVICE_BOOT)
#  include "hss_boot_pmp.h"
#endif

#include "mpfs_reg_map.h"
#include "sbi_version.h"
#include "sbi/sbi_platform.h"
#include "sbi/sbi_ipi.h"
#include "sbi/riscv_atomic.h"
#include "slice/slice_reset.h"
#include "slice/slice.h"

#if !IS_ENABLED(CONFIG_OPENSBI)
#  error OPENSBI needed for this module
#endif

#if IS_ENABLED(CONFIG_HSS_USE_IHC)
#  include "miv_ihc.h"
#endif

extern const struct sbi_platform platform;
static unsigned long l_hartid_to_scratch(int hartid);

//
// OpenSBI needs a scratch structure per hart, plus some ancilliary data space
// after this...
//
// "External firmware must create per-HART non-overlapping:
// 1. Program Stack
// 2.OpenSBI scratch space (i.e. struct sbi_scratch instance with extra space above)"

// place this in DDR...
union t_HSS_scratchBuffer {
    struct sbi_scratch scratch;
    unsigned long buffer[SBI_SCRATCH_SIZE / __SIZEOF_POINTER__];
} scratches[MAX_NUM_HARTS];

union t_HSS_scratchBuffer  system_scratches[MAX_NUM_HARTS] __attribute__((section(".l2_scratchpad"), used));
//} scratches[MAX_NUM_HARTS] __attribute__((section(".opensbi_scratch")));
asm(".align 3\n"
	"scratch_addr: .quad scratches\n");

extern const size_t scratch_addr;
union t_HSS_scratchBuffer *pScratches = 0;

extern unsigned long _slice_fw_start, _slice_fw_end;
static void opensbi_scratch_setup(bool is_system, enum HSSHartId hartid)
{
    assert(hartid < MAX_NUM_HARTS);
    if(is_system){
        pScratches = (union t_HSS_scratchBuffer * const)system_scratches;
    }else{
        pScratches = (union t_HSS_scratchBuffer * const)scratches;
    }
    #if IS_ENABLED(CONFIG_QEMU)
    pScratches = (union t_HSS_scratchBuffer * const)scratches;
    #endif
    pScratches[hartid].scratch.options = SBI_SCRATCH_DEBUG_PRINTS;
    pScratches[hartid].scratch.hartid_to_scratch = (unsigned long)l_hartid_to_scratch;
    pScratches[hartid].scratch.platform_addr = (unsigned long)&platform;

    pScratches[hartid].scratch.fw_start = (unsigned long)&_slice_fw_start;
    pScratches[hartid].scratch.fw_size = (unsigned long)&_slice_fw_end - (unsigned long)&_slice_fw_start;
    //(unsigned long)&_slice_fw_end - (unsigned long)&_slice_fw_start;
}

static unsigned long l_hartid_to_scratch(int hartid)
{
    unsigned long result = 0u;
    assert(hartid < MAX_NUM_HARTS);

    if (hartid != 0) {
        result = (unsigned long)(&(pScratches[hartid].scratch));
    }

    return result;
}

static void opensbi_init_handler(struct StateMachine * const pMyMachine);
static void opensbi_idle_handler(struct StateMachine * const pMyMachine);

/*!
 * \brief OPENSBI Driver States
 */
enum GotoStatesEnum {
    OPENSBI_INITIALIZATION,
    OPENSBI_IDLE,
    OPENSBI_NUM_STATES = OPENSBI_IDLE+1
};

/*!
 * \brief OPENSBI Driver State Descriptors
 */
static const struct StateDesc opensbi_state_descs[] = {
    { (const stateType_t)OPENSBI_INITIALIZATION, (const char *)"Init",     NULL, NULL, &opensbi_init_handler },
    { (const stateType_t)OPENSBI_IDLE,           (const char *)"Idle",     NULL, NULL, &opensbi_idle_handler },
};

/*!
 * \brief OPENSBI Driver State Machine
 */
struct StateMachine opensbi_service = {
    .state             = (stateType_t)OPENSBI_INITIALIZATION,
    .prevState         = (stateType_t)SM_INVALID_STATE,
    .numStates         = (const uint32_t)OPENSBI_NUM_STATES,
    .pMachineName      = (const char *)"opensbi_service",
    .startTime         = 0u,
    .lastExecutionTime = 0u,
    .executionCount    = 0u,
    .pStateDescs       = opensbi_state_descs,
    .debugFlag         = false,
    .priority          = 0u,
    .pInstanceData     = NULL,
};


// --------------------------------------------------------------------------------------------------
// Handlers for each state in the state machine
//
static void opensbi_init_handler(struct StateMachine * const pMyMachine)
{
    pMyMachine->state++;
}

/////////////////
static void opensbi_idle_handler(struct StateMachine * const pMyMachine)
{
    (void)pMyMachine; // unused
}


/////////////////

extern unsigned long _trap_handler;

#include "sbi/sbi_domain.h"
#include "sbi/sbi_ecall_interface.h"

int sbi_console_init(struct sbi_scratch *scratch);

#define RESET_BASE_ADDR 0x100000
void HSS_OpenSBI_Setup(void)
{
    enum HSSHartId hartid = current_hartid();

    if (hartid == HSS_HART_E51) {
        uint32_t mstatus_val = mHSS_CSR_READ(CSR_MSTATUS);
        mstatus_val = EXTRACT_FIELD(mstatus_val, MSTATUS_MPIE);
        mHSS_CSR_WRITE(CSR_MSTATUS, mstatus_val);
        mHSS_CSR_WRITE(CSR_MIE, 0u);
        d_reset_init(RESET_BASE_ADDR);
        opensbi_scratch_setup(true, hartid);
        sbi_platform_ops(&platform)->irqchip_init(true);
        int rc = sbi_console_init(&(pScratches[hartid].scratch));
        //sbi_platform_domains_init(&platform);
        if (rc)
            sbi_hart_hang();
        } else {
            ;
        }
}

#define SLICE_FW_COPY_OFFSET 0x1000000

void sbi_exit(struct sbi_scratch *scratch){
}

static atomic_t host_init_domain = ATOMIC_INITIALIZER(0);
static atomic_t host_init_complete = ATOMIC_INITIALIZER(0);

void __noreturn HSS_OpenSBI_DoBoot(enum HSSHartId hartid, int sbi_is_shared)
{
    static unsigned long fsbl_addr = (unsigned long)&_slice_fw_start;
    static unsigned long fsbl_store_addr = (unsigned long)&_slice_fw_start + SLICE_FW_COPY_OFFSET;
    unsigned long fsbl_size = (unsigned long)&_slice_fw_end - (unsigned long)&_slice_fw_start;
    // (unsigned long)&_slice_fw_end - (unsigned long)&_slice_fw_start;
    #ifdef CONFIG_SERVICE_BOOT_DDR_SLICE_FW_START
        fsbl_store_addr =  CONFIG_SERVICE_BOOT_DDR_SLICE_FW_START;
    #endif
    uint32_t mstatus_val = mHSS_CSR_READ(CSR_MSTATUS);
    mstatus_val = EXTRACT_FIELD(mstatus_val, MSTATUS_MPIE);
    mHSS_CSR_WRITE(CSR_MSTATUS, mstatus_val);
    mHSS_CSR_WRITE(CSR_MIE, 0u);
    
    opensbi_scratch_setup(sbi_is_shared, hartid);
    if (!sbi_is_shared) {
        slice_uart_init(hartid);
    }
    unsigned long slice_fw_start = slice_mem_start_this_hart();
    enum HSSHartId primary_hartid = slice_owner_hart(hartid);
    bool is_slice = (slice_mem_size_this_hart() != -1UL);
    if(!is_slice && !sbi_is_shared){
        sbi_printf("Bare-metal boot.========\n");
        mpfs_mark_hart_as_booted(hartid); 
        slice_loader(sbi_domain_thishart_ptr(), fsbl_store_addr, fsbl_size);
    }
    if(slice_fw_start && sbi_is_shared){
        if(hartid == primary_hartid){
            // System-wide initialization
            // TBD: Move to hart0 service
            init_slice_sbi_copy_status();
            if(atomic_add_return(&host_init_domain, 1)==1){
                memcpy((void*)fsbl_store_addr, (void*)fsbl_addr, fsbl_size);
                sbi_domain_init(&pScratches[0].scratch, 0);
                sbi_printf("sbi_domain_init done\n");
                sbi_platform_domains_init(&platform);
                sbi_ipi_init(&pScratches[0].scratch, TRUE);
                if(atomic_add_return(&host_init_complete, 1)!=1){
                    sbi_printf("%s: should not reach here.\n", __func__);
                    wfi();
                }
                sbi_printf("atomic_add_return done\n");

			}
            while(atomic_read(&host_init_complete)==0){
            }
            mpfs_mark_hart_as_booted(hartid);
            slice_loader(sbi_domain_thishart_ptr(), fsbl_store_addr, fsbl_size);
        }else{
            while(!is_slice_sbi_copy_done()){
            }
            mpfs_mark_hart_as_booted(hartid);
            slice_loader(sbi_domain_thishart_ptr(), fsbl_store_addr, fsbl_size);
        }
    }

    while (1) {
        asm("wfi");
    };
}

enum IPIStatusCode HSS_OpenSBI_IPIHandler(TxId_t transaction_id, enum HSSHartId source, uint32_t immediate_arg, void *p_extended_buffer, void *p_ancilliary_buffer_in_ddr)
{
    enum IPIStatusCode result = IPI_FAIL;

    int hartid = current_hartid();

    if (source != HSS_HART_E51) { // prohibited by policy
        mHSS_DEBUG_PRINTF(LOG_ERROR, "hart %d: request from source %d prohibited by policy" CRLF, hartid, source);
    } else if (hartid == HSS_HART_E51) { // prohibited by policy
        mHSS_DEBUG_PRINTF(LOG_ERROR, "hart %d: request prohibited by policy" CRLF, HSS_HART_E51);
    } else {
        IPI_Send(source, IPI_MSG_ACK_COMPLETE, transaction_id, IPI_SUCCESS, NULL, NULL);
        IPI_MessageUpdateStatus(transaction_id, IPI_IDLE); // free the IPI

#if IS_ENABLED(CONFIG_HSS_USE_IHC)
        __sync_synchronize();

        // small delay to ensure that IHC message has been sent before jumping into OpenSBI
        // without this, HSS never receives ack from U54 that OPENSBI_INIT was successful
        HSS_SpinDelay_MilliSecs(250u);
#endif

        struct IPI_Outbox_Msg *pMsg = IPI_DirectionToFirstMsgInQueue(source, hartid);
        size_t i;

        for (i = 0u; i < IPI_MAX_NUM_QUEUE_MESSAGES; i++) {
            if (pMsg->transaction_id == transaction_id) { break; }
            pMsg++;
        }

        // if message found process it...
        if (pMsg->transaction_id == transaction_id) {
            pMsg->msg_type = IPI_MSG_NO_MESSAGE;
            result = IPI_SUCCESS;
        } else {
            result = IPI_FAIL;
        }

        if (result != IPI_FAIL) {
            csr_write(mscratch, &(pScratches[hartid].scratch));

            if(p_extended_buffer){
                pScratches[hartid].scratch.next_addr = (uintptr_t)p_extended_buffer;
            }
            if(!pScratches[hartid].scratch.next_mode){
                pScratches[hartid].scratch.next_mode = (unsigned long)immediate_arg;
            }
        // set arg1 (A1) to point to override device tree blob, if provided
#if IS_ENABLED(CONFIG_PROVIDE_DTB)
#  if IS_ENABLED(CONFIG_PLATFORM_MPFS)
            extern unsigned long _binary_services_opensbi_mpfs_dtb_start;
            pScratches[hartid].scratch.next_arg1 = (unsigned long)&_binary_services_opensbi_mpfs_dtb_start;
#  elif IS_ENABLED(CONFIG_PLATFORM_FU540)
            extern unsigned long _binary_hifive_unleashed_a00_dtb_start;
            pScratches[hartid].scratch.next_arg1 = (unsigned long)&_binary_hifive_unleashed_a00_dtb_start;
#  else
#    error Unknown PLATFORM settings
#  endif
#else
            // else use ancilliary data if provided in boot image, assuming it is a DTB
#endif
            if(p_ancilliary_buffer_in_ddr){
                pScratches[hartid].scratch.next_arg1 = (uintptr_t)p_ancilliary_buffer_in_ddr;
            }
            HSS_OpenSBI_DoBoot(hartid, 1);
        }
    }

    return result;
}

void HSS_OpenSBI_Reboot(void)
{
    uint32_t index;

sbi_printf("%s() called\n", __func__);
    IPI_MessageAlloc(&index);
    IPI_MessageDeliver(index, HSS_HART_E51, IPI_MSG_BOOT_REQUEST, 0u, NULL, NULL);
}
