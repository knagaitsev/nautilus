#ifndef __PC_8250_H__
#define __PC_8250_H__

/*
 * "PC" Serial Port Driver
 *
 * Supposed to provide the same functionality as "dev/serial.c"
 */

#ifdef NAUT_CONFIG_PC_8250_UART_EARLY_OUTPUT
int pc_8250_pre_vc_init(void);
#endif

int pc_8250_init(void);

#endif
