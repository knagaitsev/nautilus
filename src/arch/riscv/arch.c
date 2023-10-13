#include <nautilus/arch.h>
#include <nautilus/cpu_state.h>
#include <nautilus/of/numa.h>
#include <nautilus/endian.h>

void arch_enable_ints(void)  { set_csr(sstatus, SSTATUS_SIE); }
void arch_disable_ints(void) { clear_csr(sstatus, SSTATUS_SIE); }
int  arch_ints_enabled(void) { return read_csr(sstatus) & SSTATUS_SIE; };

#include <arch/riscv/plic.h>
/*
void arch_irq_enable(int irq) { plic_enable(irq, 1); }
void arch_irq_disable(int irq) { 
    printk("im disabling irq=%d\n", irq);
    plic_disable(irq); 
}
*/

uint32_t arch_cycles_to_ticks(uint64_t cycles) { /* TODO */ return cycles; }
uint32_t arch_realtime_to_ticks(uint64_t ns) { return ((ns*RISCV_CLOCKS_PER_SECOND)/1000000000ULL); }
uint64_t arch_realtime_to_cycles(uint64_t ns) { /* TODO */ return arch_realtime_to_ticks(ns); }
uint64_t arch_cycles_to_realtime(uint64_t cycles) { return (cycles * 1000000000ULL)/RISCV_CLOCKS_PER_SECOND; }

#include <arch/riscv/sbi.h>
#include <nautilus/scheduler.h>
#include <nautilus/timer.h>

static uint8_t  timer_set = 0;
static uint32_t current_ticks = 0;
static uint64_t timer_count = 0;

void arch_update_timer(uint32_t ticks, nk_timer_condition_t cond) {
    if (!timer_set) {
      arch_set_timer(ticks);
    } else {
    switch(cond) {
    case UNCOND:
        arch_set_timer(ticks);
        break;
    case IF_EARLIER:
        if (ticks < current_ticks) { arch_set_timer(ticks); }
        break;
    case IF_LATER:
        if (ticks > current_ticks) { arch_set_timer(ticks); }
        break;
    }
    }
    get_cpu()->in_timer_interrupt = 0;
    get_cpu()->in_kick_interrupt = 0;
}

void arch_set_timer(uint32_t ticks) {
    uint64_t time_to_set = !ticks ? read_csr(time) + 1 :
           (ticks != -1 || current_ticks == -1) ? read_csr(time) + ticks : -1;
    sbi_set_timer(time_to_set);
    timer_set = 1;
    current_ticks = ticks;
}

int  arch_read_timer(void) { /* TODO */ return 0; }

int arch_timer_handler(struct nk_irq_action * action, struct nk_regs *regs, void *state)
{
    uint64_t time_to_next_ns;

    get_cpu()->in_timer_interrupt = 1;

    timer_count++;

    timer_set = 0;

    time_to_next_ns = nk_timer_handler();

    if (time_to_next_ns == 0) {
      arch_set_timer(-1);
    } else {
      arch_set_timer(arch_realtime_to_ticks(time_to_next_ns));
    }

    return 0;
}

uint64_t arch_read_timestamp() { 
  uint64_t time = read_csr(time); 
  return time;
}

int arch_numa_init(struct sys_info *sys) {
  return fdt_numa_init(sys);
}

void arch_print_regs(struct nk_regs * r) {

    printk("RA:  %016lx SP:  %016lx\n", r->ra, r->sp);
    //printk("GP:  %016lx TP:  %016lx\n", r->gp, r->tp);
    printk("A00: %016lx A01: %016lx A02: %016lx\n", r->a0, r->a1, r->a2);
    printk("A03: %016lx A04: %016lx A05: %016lx\n", r->a3, r->a4, r->a5);
    printk("A06: %016lx A07: %016lx\n", r->a6, r->a7);
    printk("T00: %016lx T01: %016lx T02: %016lx\n", r->t0, r->t1, r->t2);
    printk("T03: %016lx T04: %016lx T05: %016lx\n", r->t4, r->t5, r->t6);
    printk("T06: %016lx T07: %016lx\n", r->t6, r->t7);
    printk("ZERO: %016lx\n", r->__zero);
}

void * arch_read_sp(void) {
    void * sp = NULL;
    __asm__ __volatile__ ( "mv %[_r], sp" : [_r] "=r" (sp) : : "memory" );
    return sp;
}

void arch_relax(void) {
    // RISCV HACK
    // asm volatile ("pause");
}

void arch_halt(void) {
    // RISCV HACK
    // asm volatile ("hlt");
}

int arch_little_endian(void) {
  return check_little_endian();
}

int arch_atomics_enabled(void) {
  // KJH - I'm just assuming RISC-V atomics should be enabled always
  return 1;
}

int arch_handle_io_map(struct nk_io_mapping *mapping) {
  // KJH - do nothing we should be identity mapped
  return 0;
}
int arch_handle_io_unmap(struct nk_io_mapping *mapping) {
  // KJH - same as arch_handle_io_map
  return 0;
}
