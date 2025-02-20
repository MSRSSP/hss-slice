#include <sbi/riscv_atomic.h>
#include <sbi/sbi_console.h>
#include <sbi/sbi_domain.h>
#include <sbi/sbi_hart.h>
#include <sbi/sbi_init.h>
#include <sbi/sbi_scratch.h>
#include <sbi/sbi_string.h>
#include <slice/slice.h>
#include <slice/slice_fw.h>
#include <slice/slice_mgr.h>
#include <slice/slice_pmp.h>
#include "slice_attest.h"

void __attribute__((noreturn))
slice_jump_to_fw(unsigned long next_addr, unsigned long arg0,
                 unsigned long arg1, unsigned long arg2) {
  register unsigned long a0 asm("a0") = arg0;
  register unsigned long a1 asm("a1") = arg1;
  register unsigned long a2 asm("a2") = arg2;
  __asm__ __volatile__("jr %0\n" ::"r"(next_addr), "r"(a0), "r"(a1), "r"(a2));
  __builtin_unreachable();
}

static void zero_slice_memory(void *dom_ptr) {
  unsigned long startTicks = csr_read(CSR_MCYCLE);
  struct sbi_domain *dom = (struct sbi_domain *)dom_ptr;
  if (dom->slice_mem_start) {
    slice_printf("Zero Slice Mem %lx\n", dom->slice_mem_start);
    slice_printf("Zero Slice Mem size %lx.\n", dom->slice_mem_size);
    sbi_memset((void *)dom->slice_mem_start, 0, dom->slice_mem_size);
  }
  sbi_printf("%s: hart %d: #ticks = %lu\n", __func__, current_hartid(),
             csr_read(CSR_MCYCLE) - startTicks);
}

struct slice_fw_info fw_info[MAX_HART_NUM];
extern unsigned long hss_start_time[5];

void __attribute__((noreturn))
slice_loader(struct sbi_domain *dom, unsigned long fw_src,
             unsigned long fw_size) {
  // Slice-wide initialization: loading firmware into slice MEM.
  unsigned hartid = current_hartid();
  dom->slice_start_time[hartid] = csr_read(CSR_MCYCLE);
  unsigned long slice_fw_start = dom->slice_mem_start;
  sbi_printf("reset -> %s: hart %d: #ticks = %lu\n", __func__, hartid,
             dom->slice_start_time[hartid] - hss_start_time[hartid]);
  fw_info[hartid].slice_cfg_ptr = (unsigned long)dom;
  fw_info[hartid].sbi_fw_info.magic = SLICE_FW_INFO_MAGIC_VALUE;
  fw_info[hartid].sbi_fw_info.version = SLICE_FW_INFO_VERSION_MAX;
  fw_info[hartid].sbi_fw_info.next_addr = dom->next_addr;
  fw_info[hartid].sbi_fw_info.next_mode = dom->next_mode;
  fw_info[hartid].sbi_fw_info.boot_hart = dom->boot_hartid;
  if (!is_slice(dom)) {
    // bare metal boot, directlt call sbi_init.
    slice_jump_to_fw(fw_src, hartid, (unsigned long)slice_fdt(dom),
                     (unsigned long)&fw_info[hartid]);
    __builtin_unreachable();
  }
  if (slice_is_stopped(dom) && dom->boot_hartid == hartid) {
    //zero_slice_memory(dom);
    if (!fw_size) {
      sbi_hart_hang();
    }
    unsigned long startTicks = csr_read(CSR_MCYCLE);
    sbi_memcpy((void *)slice_fw_start, (void *)fw_src, fw_size);
    //slice_measure(dom, slice_fw_start, fw_size);
    slice_measure(dom, dom->next_boot_src, dom->next_boot_size);
    sbi_printf("copy slice_fw: hart %d: #ticks = %lu\n", current_hartid(),
               csr_read(CSR_MCYCLE) - startTicks);
    slice_status_start(dom);
  }
  while (!slice_is_running(dom)) {
  };
  // nonslice_setup_pmp(dom);
  sbi_printf("%s: next_addr=%#lx: arg: %d %lx %lx\n", __func__, slice_fw_start,
             hartid, (unsigned long)slice_fdt(dom),
             (unsigned long)&fw_info[hartid]);
  slice_jump_to_fw(slice_fw_start, hartid, (unsigned long)slice_fdt(dom),
                   (unsigned long)&fw_info[hartid]);
   sbi_printf("2: %s: next_addr=%#lx: arg: %d %lx %lx\n", __func__, slice_fw_start,
             hartid, (unsigned long)slice_fdt(dom),
             (unsigned long)&fw_info[hartid]);
  __builtin_unreachable();
}
