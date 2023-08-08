#ifndef __PL011_H__
#define __PL011_H__

#include<nautilus/naut_types.h>
#include<nautilus/chardev.h>
#include<nautilus/spinlock.h>

struct pl011_uart {
  struct nk_char_dev *dev;

  void *mmio_base;
  uint64_t clock;

  uint32_t baudrate;

  nk_irq_t irq;

  spinlock_t input_lock;
  spinlock_t output_lock;
};

#ifdef NAUT_CONFIG_PL011_UART_EARLY_OUTPUT
void pl011_uart_pre_vc_init(uint64_t dtb);
#endif

int pl011_uart_init(void);

int pl011_uart_dev_write(void*, uint8_t *src);
int pl011_uart_dev_read(void*, uint8_t *dest);

void pl011_uart_putchar_blocking(struct pl011_uart*, const char c);
void pl011_uart_puts_blocking(struct pl011_uart*, const char *s);

#endif
