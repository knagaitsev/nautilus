
#define __NAUTILUS_MAIN__

#include<nautilus/nautilus.h>

#include<dev/pl011.h>

#define NAUT_WELCOME                                      \
  "Welcome to                                         \n" \
  "    _   __               __   _  __                \n" \
  "   / | / /____ _ __  __ / /_ (_)/ /__  __ _____    \n" \
  "  /  |/ // __ `// / / // __// // // / / // ___/    \n" \
  " / /|  // /_/ // /_/ // /_ / // // /_/ /(__  )     \n" \
  "/_/ |_/ \\__,_/ \\__,_/ \\__//_//_/ \\__,_//____/  \n" \
  "+===============================================+  \n" \
  " Kyle C. Hale (c) 2014 | Northwestern University   \n" \
  "+===============================================+  \n\n"

#define QEMU_PL011_VIRT_BASE_ADDR 0x9000000
#define QEMU_VIRT_BASE_CLOCK 24000000 // 24 MHz Clock

struct pl011_uart _main_pl011_uart;

/* Faking some vc stuff */

uint16_t
vga_make_entry (char c, uint8_t color)
{
    uint16_t c16 = c;
    uint16_t color16 = color;
    return c16 | color16 << 8;
}

void init(unsigned long dtb_raw, unsigned long x1, unsigned long x2, unsigned long x3) {

  extern struct pl011_uart *printk_uart;
  printk_uart = &_main_pl011_uart;

  pl011_uart_init(&_main_pl011_uart, (void*)QEMU_PL011_VIRT_BASE_ADDR, QEMU_VIRT_BASE_CLOCK);

  pl011_uart_puts(&_main_pl011_uart, NAUT_WELCOME);

  while(1) {}
}
