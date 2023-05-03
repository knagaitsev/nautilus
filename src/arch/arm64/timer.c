
#include<nautilus/arch.h>
#include<nautilus/timer.h>
#include<nautilus/percpu.h>

#define NS_PER_S 1000000000ULL

static uint8_t  timer_set = 0;
static uint32_t current_ticks = 0;
static uint64_t timer_count = 0;

uint32_t arch_cycles_to_ticks(uint64_t cycles) {
  ARM64_ERR_UNIMPL;
  return 0;
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

static inline void generic_timer_set(uint64_t time) {
  asm volatile("msr CNTP_CVAL_EL0, %0" :: "r" (time));
}

void arch_set_timer(uint32_t ticks) {
    uint64_t time_to_set = !ticks ? arch_read_timestamp() + 1 :
           (ticks != -1 || current_ticks == -1) ? arch_read_timestamp() + ticks : -1;

    generic_timer_set(time_to_set); 

    timer_set = 1;
    current_ticks = ticks;
}
int arch_read_timer(void) {
  ARM64_ERR_UNIMPL;
  return 0;
}

int arch_timer_handler(excp_entry_t *excp, ulong_t vec, void *state) {

    printk("Timer interrupt!\n");

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

    nk_yield();


  return 0;
}

uint64_t arch_read_timestamp(void) {
  uint64_t phys;
  asm volatile ("isb; mrs %0, CNTPCT_EL0" : "=r" (phys));
  return phys;
}

