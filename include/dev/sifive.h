#ifndef __SIFIVE_H__
#define __SIFIVE_H__

#include <nautilus/naut_types.h>

struct sifive_serial_regs {
    volatile uint32_t txfifo;
    volatile uint32_t rxfifo;
    volatile uint32_t txctrl;
    volatile uint32_t rxctrl;
    volatile uint32_t ie;  // interrupt enable
    volatile uint32_t ip;  // interrupt pending
    volatile uint32_t div;
};

void sifive_serial_init(unsigned long fdt);
void sifive_serial_write(const char *b);
void sifive_serial_putchar(unsigned char ch);
int  sifive_serial_getchar(void);

#endif
