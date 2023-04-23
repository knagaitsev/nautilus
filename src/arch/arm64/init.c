
#define __NAUTILUS_MAIN__

#include<nautilus/nautilus.h>

/* Faking some vc stuff */

uint16_t
vga_make_entry (char c, uint8_t color)
{
    uint16_t c16 = c;
    uint16_t color16 = color;
    return c16 | color16 << 8;
}

void init(unsigned long dtb_raw, unsigned long x1, unsigned long x2, unsigned long x3) {
	while (1) {}
}
