#include "services/tinycli/tinycli_slice.h"
#include <stddef.h>
#include "mss_uart.h"
#include "mss_uart_regs.h"
#include "mss_plic.h"
#include "mss_util.h"
#include "fpga_design_config/fpga_design_config.h"
#define MSS_UART_DATA_READY             ((uint8_t) 0x01)

unsigned long _start_warm;
int slicevisor_main(int n, char ** args){
	tinyCLI_Slice(n, (const char**)args);
	return 0;
}
void
__gcov_exit (void)
{
}

int slice_pmp_dump(void){
  return 0;
}

mss_uart_instance_t g_mss_uart0_lo;

size_t
MSS_UART_get_rx
(
    mss_uart_instance_t * this_uart,
    uint8_t * rx_buff,
    size_t buff_size
)
{
    size_t rx_size = 0u;
    uint8_t status = 0u;

    ASSERT(rx_buff != ((uint8_t*)0));
    ASSERT(buff_size > 0u);

    if ((rx_buff != (uint8_t*)0) && (buff_size > 0u))
    {
        status = this_uart->hw_reg->LSR;
        this_uart->status |= status;

        while (((status & MSS_UART_DATA_READY) != 0u) && (rx_size < buff_size))
        {
            rx_buff[rx_size] = this_uart->hw_reg->RBR;
            ++rx_size;
            status = this_uart->hw_reg->LSR;
            this_uart->status |= status;
        }
    }

    return rx_size;
}


 void slice_sw_start(int dom_index) {
}