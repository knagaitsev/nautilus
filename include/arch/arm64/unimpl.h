#ifndef __ARM64_UNIMPL_H__
#define __ARM64_UNIMPL_H__

#include<dev/pl011.h>

extern struct pl011_uart *printk_uart;

#define _stringify(x) #x
#define _tostring(x) _stringify(x)

#define ARM64_ERR_UNIMPL \
  pl011_uart_puts(printk_uart, "\nERROR: Unimplemented section of code reached!\n" \
             "LINE: " _tostring(__LINE__) \
             " FILE: " __FILE__ \
             "\n")

#endif
