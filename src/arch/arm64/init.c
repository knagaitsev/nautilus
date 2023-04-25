
#define __NAUTILUS_MAIN__

#include<nautilus/nautilus.h>
#include<nautilus/printk.h>
#include<nautilus/fpu.h>
#include<nautilus/naut_string.h>

#include<arch/arm64/unimpl.h>

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

extern void *_bssEnd;
extern void *_bssStart;


void init(unsigned long dtb_raw, unsigned long x1, unsigned long x2, unsigned long x3) {

  struct naut_info *naut = &nautilus_info;

  // Zero out .bss
  nk_low_level_memset(_bssStart, 0, (off_t)_bssEnd - (off_t)_bssStart);
  
  // Enable the FPU
  fpu_init(naut, 0);
 
  // Initialize serial output
  pl011_uart_init(&_main_pl011_uart, (void*)QEMU_PL011_VIRT_BASE_ADDR, QEMU_VIRT_BASE_CLOCK);

  // Enable printk by handing it the uart
  extern struct pl011_uart *printk_uart;
  printk_uart = &_main_pl011_uart;

  printk(NAUT_WELCOME);

  printk("Hello, %d\n", 12345678);

  while(1) {}
}
