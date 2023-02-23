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

static uint32_t mask10 = 0x00000400;

static uint32_t mask4 = 0x00000010;

static volatile int curr_out_state = 0;

int gpio_int_handler(ulong_t irq) {
    // if (irq == 27) {
    //     return 0;
    // }

    // this needs to go before reading of the below IP registers
    curr_out_state = !curr_out_state;
    printk("state: %d\n", curr_out_state);
    if (curr_out_state) {
        MREG(SIFIVE_GPIO_OUTPUT_VAL) = mask10;
    } else {
        MREG(SIFIVE_GPIO_OUTPUT_VAL) = 0x0;
    }

    uint32_t rise_val = MREG(SIFIVE_GPIO_RISE_IP);
    uint32_t fall_val = MREG(SIFIVE_GPIO_FALL_IP);
    uint32_t high_val = MREG(SIFIVE_GPIO_HIGH_IP);
    uint32_t low_val = MREG(SIFIVE_GPIO_LOW_IP);

    printk("got interrupt: %d, high: %x, low: %x, rise: %x, fall: %x\n", irq, high_val, low_val, rise_val, fall_val);

    // uint32_t full_val = 0xFFFFF0FF;
    // // mask4 = full_val;

    MREG(SIFIVE_GPIO_RISE_IP) = mask4;
    MREG(SIFIVE_GPIO_FALL_IP) = mask4;
    MREG(SIFIVE_GPIO_HIGH_IP) = mask4;
    MREG(SIFIVE_GPIO_LOW_IP) = mask4;

    // uint32_t out_val = MREG(SIFIVE_GPIO_OUTPUT_VAL);
    // printk("out val: %x\n", out_val);

    return 0;
}

static void sifive_init(const void *fdt, addr_t addr, int offset) {
    base_addr = addr;
    printk("GPIO @ %p\n", addr);

    uint32_t full_val = 0xFFFFF0FF;
    // mask4 = full_val;

    // clean up bits
    MREG(SIFIVE_GPIO_RISE_IP) = mask4;
    MREG(SIFIVE_GPIO_FALL_IP) = mask4;
    MREG(SIFIVE_GPIO_HIGH_IP) = mask4;
    MREG(SIFIVE_GPIO_LOW_IP) = mask4;

    MREG(SIFIVE_GPIO_INPUT_EN) = mask4;

    // MREG(SIFIVE_GPIO_PUE) = mask4;
    // MREG(SIFIVE_GPIO_PUE) = mask10;

    // volatile uint32_t *output_val_addr = (volatile uint32_t *)(addr + SIFIVE_GPIO_OUTPUT_VAL);

    // *output_val_addr = 0x00000008;
    // *output_val_addr = 0xFFFFF0FF;
    // while (1) {
    //     *output_val_addr = 0xFFFFF0FF;
    //     *output_val_addr = 0x0;
    // }

    int lenp = 0;
    const void *ints = fdt_getprop(fdt, offset, "interrupts", &lenp);
	uint32_t *vals = (uint32_t *)ints;

    int num_irqs = lenp / 4;
	for (int i = 0; i < num_irqs; i++) {
		uint32_t irq = bswap_32(vals[i]);
        // if (irq == 23) {
        //     continue;
        // }
        riscv_irq_install((ulong_t)irq, gpio_int_handler);
	}

    // MREG(SIFIVE_GPIO_HIGH_IE) = mask4;
    // MREG(SIFIVE_GPIO_LOW_IE) = mask4;
    MREG(SIFIVE_GPIO_RISE_IE) = mask4;
    // MREG(SIFIVE_GPIO_FALL_IE) = mask4;

    MREG(SIFIVE_GPIO_OUTPUT_EN) = mask10;
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

int sifive_gpio_set_pin(int is_high) {
    if (is_high) {
        MREG(SIFIVE_GPIO_OUTPUT_VAL) = mask10;
    } else {
        MREG(SIFIVE_GPIO_OUTPUT_VAL) = 0;
    }

    return 0;
}
