#include <arch/riscv/plic.h>
#include <arch/riscv/sbi.h>
#include <nautilus/cpu.h>
#include <nautilus/naut_types.h>
#include <nautilus/percpu.h>
#include <nautilus/fdt.h>

typedef struct plic_context {
  off_t enable_offset;
  off_t context_offset;
} plic_context_t;

static addr_t plic_addr = 0;

static plic_context_t *contexts = NULL;

#define PLIC plic_addr
#define MREG(x) *((uint32_t *)(PLIC + (x)))

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

// #define PLIC_PRIORITY MREG(PLIC + 0x0)
// #define PLIC_PENDING MREG(PLIC + 0x1000)
// #define PLIC_MENABLE(hart) MREG(PLIC + 0x2000 + (hart)*0x100)
// #define PLIC_SENABLE(hart) MREG(PLIC + 0x2080 + (hart)*0x100)
// #define PLIC_MPRIORITY(hart) MREG(PLIC + 0x201000 + (hart)*0x2000)
// #define PLIC_SPRIORITY(hart) MREG(PLIC + 0x200000 + (hart)*0x2000)
// #define PLIC_MCLAIM(hart) MREG(PLIC + 0x201004 + (hart)*0x2000)
// #define PLIC_SCLAIM(hart) MREG(PLIC + 0x200004 + (hart)*0x2000)

// #define ENABLE_BASE 0x2000
// #define ENABLE_PER_HART 0x100

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
            int phandle = bswap_32(vals[c_off]);
            int nr = bswap_32(vals[c_off + 1]);
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

    uint32_t mask = (1 << (hwirq % 32));
    uint32_t val = MREG(PLIC_ENABLE(hwirq, hart));
    // printk("Hart: %d, hwirq: %d, v1: %d\n", hart, hwirq, val);
    if (enable)
        val |= mask;
    else
        val &= ~mask;

    MREG(PLIC_ENABLE(hwirq, hart)) = val;
    // printk("hart: %d, addr: %x, val: %d\n", hart, &MREG(PLIC_ENABLE(hwirq, hart)), MREG(PLIC_ENABLE(hwirq, hart)));

    // printk("toggling on hart %d, irq=%d, priority=%d, enable=%d, plic=%p\n", hart, hwirq, priority, enable, PLIC);
    // off_t enable_base = PLIC + ENABLE_BASE + hart * ENABLE_PER_HART;
    // printk("enable_base=%p\n", enable_base);
    // uint32_t* reg = &(MREG(enable_base + (hwirq / 32) * 4));
    // printk("reg=%p\n", reg);
    // uint32_t hwirq_mask = 1 << (hwirq % 32);
    // printk("hwirq_mask=%p\n", hwirq_mask);
    // MREG(PLIC + 4 * hwirq) = 7;
    // PLIC_SPRIORITY(hart) = 0;

    // if (enable) {
    // *reg = *reg | hwirq_mask;
    // printk("*reg=%p\n", *reg);
    // } else {
    // *reg = *reg & ~hwirq_mask;
    // }

    // printk("*reg=%p\n", *reg);
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
    // return PLIC_SCLAIM(1);
    // printk("%d, %x\n", my_cpu_id(), &MREG(PLIC_CLAIM(my_cpu_id())));
    return MREG(PLIC_CLAIM(my_cpu_id()));
}
void plic_complete(int irq)
{
    // PLIC_SCLAIM(1) = irq;
    MREG(PLIC_CLAIM(my_cpu_id())) = irq;
}
int plic_pending(void)
{
    return MREG(PLIC_PENDING_BASE);
}

void plic_dump(void)
{
    
}

void plic_init_hart(int hart) {
    // PLIC_SPRIORITY(hart) = 0;
    MREG(PLIC_PRIORITY(hart)) = 0;
    // printk("CPU ID: %d\n", my_cpu_id());
}

