#include <arch/riscv/plic.h>
#include <arch/riscv/sbi.h>
#include <nautilus/cpu.h>
#include <nautilus/naut_types.h>
#include <nautilus/fdt/fdt.h>
#include <nautilus/endian.h>

typedef struct plic_context {
  off_t enable_offset;
  off_t context_offset;
} plic_context_t;

static addr_t plic_addr = 0;

static plic_context_t *contexts = NULL;
// static plic_context_t contexts[100];

#define PLIC plic_addr
#define MREG(x) *((uint32_t *)(PLIC + (x)))
#define READ_REG(x) *((uint32_t *)((x)))

#define PLIC_PRIORITY_BASE 0x000000U

#define PLIC_PENDING_BASE 0x1000U

#define PLIC_ENABLE_BASE 0x002000U
#define PLIC_ENABLE_STRIDE 0x80U
#define IRQ_ENABLE 1
#define IRQ_DISABLE 0

#define PLIC_CONTEXT_BASE 0x200000U
#define PLIC_CONTEXT_STRIDE 0x1000U
#define PLIC_CONTEXT_THRESHOLD 0x0U
#define PLIC_CONTEXT_CLAIM 0x4U

#define PLIC_PRIORITY(n) (PLIC_PRIORITY_BASE + (n) * sizeof(uint32_t))
#define PLIC_ENABLE(n, h) (contexts[h].enable_offset + ((n) / 32) * sizeof(uint32_t))
#define PLIC_THRESHOLD(h) (contexts[h].context_offset + PLIC_CONTEXT_THRESHOLD)
#define PLIC_CLAIM(h) (contexts[h].context_offset + PLIC_CONTEXT_CLAIM)

void plic_init(unsigned long fdt) {
    int offset = fdt_node_offset_by_compatible(fdt, -1, "sifive,plic-1.0.0");
    if (offset < 0) {
        // something is bad, no plic found
        return;
    }
    off_t addr = fdt_getreg_address(fdt, offset);
    PLIC = addr;

    int lenp = 0;
    void *ints_extended_prop = fdt_getprop(fdt, offset, "interrupts-extended", &lenp);
    if (ints_extended_prop != NULL) {
        uint32_t *vals = (uint32_t *)ints_extended_prop;
        int context_count = lenp / 8;

        contexts = kmem_malloc(context_count * sizeof(plic_context_t));

        for (int context = 0; context < context_count; context++) {
            int c_off = context * 2;
            int phandle = be32toh(vals[c_off]);
            int nr = be32toh(vals[c_off + 1]);
            if (nr != 9) {
                continue;
            }
            // printk("\tcontext %d: (%d, %d)\n", context, phandle, nr);

            int intc_offset = fdt_node_offset_by_phandle(fdt, phandle);
            int cpu_offset = fdt_parent_offset(fdt, intc_offset);
            off_t hartid = fdt_getreg_address(fdt, cpu_offset);
            // printk("\tcpu num: %d\n", hartid);
            contexts[hartid].enable_offset = PLIC_ENABLE_BASE + context * PLIC_ENABLE_STRIDE;
            contexts[hartid].context_offset = PLIC_CONTEXT_BASE + context * PLIC_CONTEXT_STRIDE;
            // printk("%d, %x, %x\n", hartid, contexts[hartid].enable_offset, &MREG(PLIC_ENABLE(0, hartid)));
        }
    }
}

static void plic_toggle(int hart, int hwirq, int priority, bool_t enable) {
    if (hwirq == 0) return;

    // TODO: this masks the interrupt so we can use it, but it is a bit ugly
    // alternatively what Nick does is iterate through all the interrupts for each
    // PLIC during init and mask them
    // https://github.com/ChariotOS/chariot/blob/7cf70757091b79cbb102a943a963dce516a8c667/arch/riscv/plic.cpp#L85-L88
    MREG(4 * hwirq) = 7;

    uint32_t mask = (1 << (hwirq % 32));
    uint32_t val = MREG(PLIC_ENABLE(hwirq, hart));

    // printk("Hart: %d, hwirq: %d, v1: %d\n", hart, hwirq, val);
    if (enable)
        val |= mask;
    else
        val &= ~mask;


    printk("Hart: %d, hwirq: %d, irq: %x, *irq: %x\n", hart, hwirq, &MREG(PLIC_ENABLE(hwirq, hart)), val);

    MREG(PLIC_ENABLE(hwirq, hart)) = val;
}

void plic_enable(int hwirq, int priority)
{
    plic_toggle(my_cpu_id(), hwirq, priority, true);
}
void plic_disable(int hwirq)
{
    plic_toggle(my_cpu_id(), hwirq, 0, false);
}
int plic_claim(void)
{
    return MREG(PLIC_CLAIM(my_cpu_id()));
}
void plic_complete(int irq)
{
    MREG(PLIC_CLAIM(my_cpu_id())) = irq;
}
int plic_pending(void)
{
    return MREG(PLIC_PENDING_BASE);
}

void plic_dump(void)
{
    printk("Dumping SiFive PLIC Register Map\n");

    printk("source priorities:\n");
    uint32_t* addr = 0x0c000000L;
    for (off_t i = 1; i < 54; i++) {
        printk("  %p (source %2d) = %d\n", addr + i, i, READ_REG(addr + i));
    }

    addr = (uint32_t*)0x0c001000L;
    printk("pending array:\t\t\t\t%p = %p\n", addr, READ_REG(addr));

    addr = (uint32_t*)0x0c002000L;
    printk("hart 0 m-mode interrupt enables:\t%p = %p\n", addr, READ_REG(addr));

    addr = (uint32_t*)0x0c002080L;
    printk("hart 1 m-mode interrupt enables:\t%p = %p\n", addr, READ_REG(addr));
    addr = (uint32_t*)0x0c002100L;
    printk("hart 1 s-mode interrupt enables:\t%p = %p\n", addr, READ_REG(addr));


    addr = (uint32_t*)0x0c002180L;
    printk("hart 2 m-mode interrupt enables:\t%p = %p\n", addr, READ_REG(addr));
    addr = (uint32_t*)0x0c002200L;
    printk("hart 2 s-mode interrupt enables:\t%p = %p\n", addr, READ_REG(addr));

    addr = (uint32_t*)0x0c002280L;
    printk("hart 3 m-mode interrupt enables:\t%p = %p\n", addr, READ_REG(addr));
    addr = (uint32_t*)0x0c002300L;
    printk("hart 3 s-mode interrupt enables:\t%p = %p\n", addr, READ_REG(addr));


    addr = (uint32_t*)0x0c002380L;
    printk("hart 4 m-mode interrupt enables:\t%p = %p\n", addr, READ_REG(addr));
    addr = (uint32_t*)0x0c002400L;
    printk("hart 4 s-mode interrupt enables:\t%p = %p\n", addr, READ_REG(addr));

    addr = (uint32_t*)0x0c200000L;
    printk("hart 0 m-mode priority threshold:\t%p = %p\n", addr, READ_REG(addr));

    addr = (uint32_t*)0x0c200004L;
    printk("hart 0 m-mode claim/complete:\t\t%p = %p\n", addr, READ_REG(addr));

    addr = (uint32_t*)0x0c201000L;
    printk("hart 1 m-mode priority threshold:\t%p = %p\n", addr, READ_REG(addr));

    addr = (uint32_t*)0x0c201004L;
    printk("hart 1 m-mode claim/complete\t\t%p = %p\n", addr, READ_REG(addr));

    addr = (uint32_t*)0x0c202000L;
    printk("hart 1 s-mode priority threshold:\t%p = %p\n", addr, READ_REG(addr));

    addr = (uint32_t*)0x0c202004L;
    printk("hart 1 s-mode claim/complete:\t\t%p = %p\n", addr, READ_REG(addr));
}

void plic_init_hart(int hart) {
    MREG(PLIC_THRESHOLD(hart)) = 0;
}

