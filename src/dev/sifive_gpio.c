#include <nautilus/nautilus.h>
#include <nautilus/irq.h>
#include <arch/riscv/riscv_idt.h>
#include <dev/sifive_gpio.h>
#include <nautilus/fdt.h>
#include <arch/riscv/plic.h>

#define SIFIVE_GPIO_INPUT_VAL	0x00
#define SIFIVE_GPIO_INPUT_EN	0x04
#define SIFIVE_GPIO_OUTPUT_EN	0x08
#define SIFIVE_GPIO_OUTPUT_VAL	0x0C
#define SIFIVE_GPIO_PUE	0x10
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

static addr_t base_addr = 0;
#define MREG(x) *((volatile uint32_t *)(base_addr + (x)))

int gpio_int_handler(ulong_t irq) {
    // if (irq == 27) {
    //     return 0;
    // }

    uint32_t high_val = MREG(SIFIVE_GPIO_HIGH_IP);
    uint32_t low_val = MREG(SIFIVE_GPIO_LOW_IP);
    uint32_t rise_val = MREG(SIFIVE_GPIO_RISE_IP);
    uint32_t fall_val = MREG(SIFIVE_GPIO_FALL_IP);

    uint32_t mask = 0x00000010;

    printk("got interrupt: %d, high: %x, low: %x, rise: %x, fall: %x\n", irq, high_val, low_val, rise_val, fall_val);
    // printk("gpio interrupt: %d, val: %x\n", irq, val);

    // uint32_t full_val = 0xFFFFF0FF;
    // // mask = full_val;

    MREG(SIFIVE_GPIO_RISE_IP) = mask;
    MREG(SIFIVE_GPIO_FALL_IP) = mask;
    MREG(SIFIVE_GPIO_HIGH_IP) = mask;
    MREG(SIFIVE_GPIO_LOW_IP) = mask;

    // MREG(SIFIVE_GPIO_RISE_IP) = val;
    // MREG(SIFIVE_GPIO_RISE_IP) = 0x00000080;
    // MREG(SIFIVE_GPIO_RISE_IP) = 0x00000040;
    // MREG(SIFIVE_GPIO_RISE_IP) = 0x00000020;

    return 0;
}

static void sifive_init(const void *fdt, addr_t addr, int offset) {
    base_addr = addr;
    printk("GPIO @ %p\n", addr);

    int lenp = 0;
    void *ints = fdt_getprop(fdt, offset, "interrupts", &lenp);
	uint32_t *vals = (uint32_t *)ints;

    int num_irqs = lenp / 4;
	for (int i = 0; i < num_irqs; i++) {
		uint32_t irq = bswap_32(vals[i]);
        // if (irq == 23) {
        //     continue;
        // }
        riscv_irq_install((ulong_t)irq, gpio_int_handler);
	}

    // for (int i = 0; i < 256; i++) {
    //     riscv_irq_install((ulong_t)i, gpio_int_handler);
    // }

    uint32_t mask = 0x00000010;
    uint32_t full_val = 0xFFFFF0FF;
    // mask = full_val;

    MREG(SIFIVE_GPIO_RISE_IP) = mask;
    MREG(SIFIVE_GPIO_FALL_IP) = mask;
    MREG(SIFIVE_GPIO_HIGH_IP) = mask;
    MREG(SIFIVE_GPIO_LOW_IP) = mask;

    // uint32_t *enable_input_addr = (uint32_t *)(addr + SIFIVE_GPIO_INPUT_EN);
    // uint32_t *enable_output_addr = (uint32_t *)(addr + SIFIVE_GPIO_OUTPUT_EN);

    // *enable_mask_addr = 0x00000008;
    MREG(SIFIVE_GPIO_INPUT_EN) = mask;

    MREG(SIFIVE_GPIO_PUE) = mask;

    MREG(SIFIVE_GPIO_HIGH_IE) = mask;
    MREG(SIFIVE_GPIO_LOW_IE) = mask;
    MREG(SIFIVE_GPIO_RISE_IE) = mask;
    MREG(SIFIVE_GPIO_FALL_IE) = mask;

    // uint32_t prev_val = 0;
    // while (1) {
    //     uint32_t val = MREG(SIFIVE_GPIO_HIGH_IP);
    //     if (val != prev_val) {
    //         printk("Val: %x\n", val);
    //         MREG(SIFIVE_GPIO_RISE_IP) = mask;
    //         MREG(SIFIVE_GPIO_FALL_IP) = mask;
    //         MREG(SIFIVE_GPIO_HIGH_IP) = mask;
    //         MREG(SIFIVE_GPIO_LOW_IP) = mask;

    //         prev_val = val;
    //     }
    // }

    // MREG(SIFIVE_GPIO_OUTPUT_VAL) = full_val;

    // volatile uint32_t *output_val_addr = (volatile uint32_t *)(addr + SIFIVE_GPIO_OUTPUT_VAL);

    // *output_val_addr = 0x00000008;
    // *output_val_addr = 0xFFFFF0FF;
    // while (1) {
    //     *output_val_addr = 0xFFFFF0FF;
    //     *output_val_addr = 0x0;
    // }
}

int fdt_node_get_sifive_gpio(const void *fdt, int offset, int depth) {
    int lenp = 0;
    const char *name = fdt_get_name(fdt, offset, &lenp);
    // need an @ after this because on qemu it can otherwise get mixed up with "gpio-restart" device
    if (name && strncmp(name, "gpio@", 5) == 0) {
        off_t addr = fdt_getreg_address(fdt, offset);
        // // uint32_t irq = fdt_get_interrupt(fdt, offset);
        sifive_init(fdt, addr, offset);
        return 0;
    }
    return 1;
}

void sifive_gpio_init(unsigned long fdt) {
    fdt_walk_devices(fdt, fdt_node_get_sifive_gpio);
}
