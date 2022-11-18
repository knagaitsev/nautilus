#include <nautilus/nautilus.h>
#include <nautilus/irq.h>
#include <arch/riscv/sbi.h>
#include <dev/sifive.h>

#define UART_TXFIFO_FULL  0x80000000
#define UART_RXFIFO_EMPTY 0x80000000
#define UART_RXFIFO_DATA  0x000000ff
#define UART_TXCTRL_TXEN  0x1
#define UART_RXCTRL_RXEN  0x1
#define UART_IP_RXWM      0x2

#define SIFIVE_DEFAULT_BAUD_RATE 115200

static struct sifive_serial_regs * regs;
static bool_t   inited = false;

int sifive_handler (excp_entry_t * excp, excp_vec_t vector, void *state) {
    panic("sifive_handler!\n");
    while (1) {
        uint32_t r = regs->rxfifo;
        if (r & UART_RXFIFO_EMPTY) break;
        char buf = r & 0xFF;
        // call virtual console here!
        sifive_serial_putchar(buf);
    }
}

int sifive_test(void) {
    arch_enable_ints();
    while(1) {
        printk("ie=%p, ip=%p, sie=%p, status=%p, sip=%p, pp=%p\t\n", regs->ie, regs->ip, read_csr(sie), read_csr(sstatus), read_csr(sip), plic_pending());
        printk("BEFORE WFI\n");
        asm volatile ("wfi");
        printk("AFTER WFI\n");
        /* printk("%d\n", sifive_serial_getchar()); */
    }
}


static void sifive_init(addr_t addr, uint16_t irq) {
    printk("SIFIVE @ %p, irq=%d\n", addr, irq);
    regs = addr;

    regs->txctrl = UART_TXCTRL_TXEN;
    regs->rxctrl = UART_RXCTRL_RXEN;
    regs->ie = 0b10;

    inited = true;

    arch_irq_install(irq, sifive_handler);
}

int fdt_node_get_sifive(const void *fdt, int offset, int depth) {
    int lenp = 0;
    char *compat_prop = fdt_getprop(fdt, offset, "compatible", &lenp);
    if (compat_prop && strstr(compat_prop, "sifive,uart0")) {
        off_t addr = fdt_getreg_address(fdt, offset);
        uint32_t irq = fdt_get_interrupt(fdt, offset);
        sifive_init(addr, (uint16_t)irq);
        return 0;
    }
    return 1;
}

void sifive_serial_init(unsigned long fdt) {
    fdt_walk_devices(fdt, fdt_node_get_sifive);
}

void sifive_serial_write(const char *b) {
    while (b && *b) {
        sifive_serial_putchar(*b);
        b++;
    }
}

void sifive_serial_putchar(unsigned char ch) {
    if (!inited) {
        sbi_call(SBI_CONSOLE_PUTCHAR, ch);
    } else {
        while (regs->txfifo & UART_TXFIFO_FULL) {
        }

        regs->txfifo = ch;
    }
}

int sifive_serial_getchar(void) {
    if (!inited) {
        struct sbiret ret = sbi_call(SBI_CONSOLE_GETCHAR);
        return ret.error == -1 ? -1 : ret.value;
    } else {
        uint32_t r = regs->rxfifo;
        return (r & UART_RXFIFO_EMPTY) ? -1 : r & 0xFF;
    }
}



