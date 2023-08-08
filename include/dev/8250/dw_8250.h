#ifndef __DW_8250_H__
#define __DW_8250_H__

#ifdef NAUT_CONFIG_DW_8250_UART_EARLY_OUTPUT
int dw_8250_pre_vc_init(uint64_t dtb);
#endif

int dw_8250_init(void);

#endif
