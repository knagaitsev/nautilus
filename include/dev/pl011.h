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

void pl011_uart_early_init(struct pl011_uart*, uint64_t dtb);
int pl011_uart_dev_init(const char *name, struct pl011_uart*);

int pl011_uart_dev_write(void*, uint8_t *src);
int pl011_uart_dev_read(void*, uint8_t *dest);

void pl011_uart_putchar_blocking(struct pl011_uart*, const char c);
void pl011_uart_puts_blocking(struct pl011_uart*, const char *s);

#endif
