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

  spinlock_t input_lock;
  spinlock_t output_lock;
};

void pl011_uart_early_init(struct pl011_uart*, void *base, uint64_t clock);
int pl011_uart_dev_init(char *name, struct pl011_uart*);
void pl011_uart_putchar(struct pl011_uart*, unsigned char);
void pl011_uart_puts(struct pl011_uart*, const char*);
void pl011_uart_write(struct pl011_uart*, const char*, size_t);

#endif
