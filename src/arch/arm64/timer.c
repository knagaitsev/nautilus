
#include<nautilus/arch.h>
#include<nautilus/timer.h>
#include<nautilus/percpu.h>
#include<arch/arm64/gic.h>

#define NS_PER_S 1000000000ULL

#ifndef NAUT_CONFIG_DEBUG_TIMERS
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

#define TIMER_PRINT(fmt, args...) printk("Timer: " fmt, ##args)
#define TIMER_DEBUG(fmt, args...) DEBUG_PRINT("Timer: " fmt, ##args)
#define TIMER_ERROR(fmt, args...) ERROR_PRINT("Timer: " fmt, ##args)
#define TIMER_WARN(fmt, args...) WARN_PRINT("Timer: " fmt, ##args)

static inline void print_timer_regs(void) {
  DEBUG_PRINT("CNTPCT_EL0 = %u\n", ({uint64_t reg; asm volatile("mrs %0, CNTPCT_EL0" : "=r" (reg)); reg;}));
  DEBUG_PRINT("CNTFRQ_EL0 = %u\n", ({uint64_t reg; asm volatile("mrs %0, CNTFRQ_EL0" : "=r" (reg)); reg;}));
  DEBUG_PRINT("CNTP_CVAL_EL0 = %u\n", ({uint64_t reg; asm volatile("mrs %0, CNTP_CVAL_EL0" : "=r" (reg)); reg;}));
  DEBUG_PRINT("CNTP_TVAL_EL0 = %u\n", ({uint64_t reg; asm volatile("mrs %0, CNTP_TVAL_EL0" : "=r" (reg)); reg;}));
}

static uint8_t  timer_set = 0;
static uint32_t current_ticks = 0;
static uint64_t timer_count = 0;

uint32_t arch_cycles_to_ticks(uint64_t cycles) {
  /* acting as if 1 cycle is 1 tick */
  /* TODO: fix this so cycles != ticks */
  return cycles;
}
uint32_t arch_realtime_to_ticks(uint64_t ns) {
  uint64_t frq;
  asm volatile ("mrs %0, CNTFRQ_EL0" : "=r" (frq));
  return (ns*frq)/NS_PER_S;
}

uint64_t arch_realtime_to_cycles(uint64_t ns) {
  /* acting as if 1 cycle is 1 tick */
  /* TODO: fix this so cycles != ticks */
  return arch_realtime_to_ticks(ns);
}
uint64_t arch_cycles_to_realtime(uint64_t cycles) {
  /* acting as if 1 cycle is 1 tick */
  /* TODO: fix this so cycles != ticks */
  uint64_t frq;
  asm volatile ("mrs %0, CNTFRQ_EL0" : "=r" (frq));
  return (cycles*NS_PER_S)/frq;
}

void arch_update_timer(uint32_t ticks, nk_timer_condition_t cond) {
    TIMER_DEBUG("arch_update_timer(%u, %u)\n", ticks, cond);
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
    TIMER_DEBUG("arch_set_timer(%u) current_timer = %u\n", ticks, arch_read_timer());
   
    asm volatile("msr CNTP_TVAL_EL0, %0" :: "r" ((int)ticks));

//    gicd_clear_int_pending(__gic_ptr, 30);

    timer_set = 1;
    current_ticks = ticks;
}
int arch_read_timer(void) {
  uint64_t tval;
  asm volatile ("mrs %0, CNTP_TVAL_EL0" : "=r" (tval));
  return (tval<<32)>>32;
}

int arch_timer_handler(excp_entry_t *excp, ulong_t vec, void *state) { 

    TIMER_DEBUG("Interrupt\n");

    uint64_t time_to_next_ns;

    get_cpu()->in_timer_interrupt = 1;

    timer_count++;

    timer_set = 0;

    time_to_next_ns = nk_timer_handler();

    TIMER_DEBUG("time_to_next_ns = %llu, ticks = %llu\n", time_to_next_ns, arch_realtime_to_ticks(time_to_next_ns));

    if (time_to_next_ns == 0) {
      arch_set_timer(-1);
    } else {
      arch_set_timer(arch_realtime_to_ticks(time_to_next_ns));
    } 

  return 0;
}

uint64_t arch_read_timestamp(void) {
  uint64_t phys;
  asm volatile ("mrs %0, CNTPCT_EL0" : "=r" (phys));
  return phys;
}

int percpu_timer_init(void) {

  // install the handler
  arch_irq_install(30, arch_timer_handler);

  // Enable the timer
  asm volatile ("mrs x0, CNTP_CTL_EL0; orr x0, x0, 1; msr CNTP_CTL_EL0, x0" ::: "x0");
  return 0;
}

