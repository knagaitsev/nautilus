#include <nautilus/nautilus.h>
#include <nautilus/irq.h>
#include <dev/sifive_gpio.h>
#include <nautilus/fdt.h>
#include <arch/riscv/plic.h>

#define SIFIVE_GPIO_INPUT_VAL	0x00
#define SIFIVE_GPIO_INPUT_EN	0x04
#define SIFIVE_GPIO_OUTPUT_EN	0x08
#define SIFIVE_GPIO_OUTPUT_VAL	0x0C
#define SIFIVE_GPIO_RISE_IE	0x18
#define SIFIVE_GPIO_RISE_IP	0x1C
#define SIFIVE_GPIO_FALL_IE	0x20
#define SIFIVE_GPIO_FALL_IP	0x24
#define SIFIVE_GPIO_HIGH_IE	0x28
#define SIFIVE_GPIO_HIGH_IP	0x2C
#define SIFIVE_GPIO_LOW_IE	0x30
#define SIFIVE_GPIO_LOW_IP	0x34
#define SIFIVE_GPIO_OUTPUT_XOR	0x40

#define SIFIVE_GPIO_MAX		32

static void sifive_init(addr_t addr) {
    printk("GPIO @ %p\n", addr);

    uint32_t *enable_mask_addr = (uint32_t *)(addr + SIFIVE_GPIO_OUTPUT_EN);

    // *enable_mask_addr = 0x00000008;
    *enable_mask_addr = 0xFFFFF0FF;

    volatile uint32_t *output_mask_addr = (volatile uint32_t *)(addr + SIFIVE_GPIO_OUTPUT_VAL);

    // *output_mask_addr = 0x00000008;
    *output_mask_addr = 0xFFFFF0FF;
    // while (1) {
    //     *output_mask_addr = 0xFFFFF0FF;
    //     *output_mask_addr = 0x0;
    // }
}

int fdt_node_get_sifive_gpio(const void *fdt, int offset, int depth) {
    int lenp = 0;
    const char *name = fdt_get_name(fdt, offset, &lenp);
    // need an @ after this because on qemu it can otherwise get mixed up with "gpio-restart" device
    if (name && strncmp(name, "gpio@", 5) == 0) {
        off_t addr = fdt_getreg_address(fdt, offset);
        // // uint32_t irq = fdt_get_interrupt(fdt, offset);
        sifive_init(addr);
        return 0;
    }
    return 1;
}

void sifive_gpio_init(unsigned long fdt) {
    fdt_walk_devices(fdt, fdt_node_get_sifive_gpio);
}
