#ifndef __PL011_H__
#define __PL011_H__

#include<nautilus/naut_types.h>

struct pl011_uart {
  void *mmio_base;
  uint64_t clock;

  uint32_t baudrate;
};

void pl011_uart_init(struct pl011_uart*, void *base, uint64_t clock);
void pl011_uart_putchar(struct pl011_uart*, unsigned char);
void pl011_uart_puts(struct pl011_uart*, const char*);
void pl011_uart_write(struct pl011_uart*, const char*, size_t);

#endif
