
#include<nautilus/arch.h>
#include<nautilus/timer.h>
#include<nautilus/percpu.h>
#include<nautilus/dev.h>
#include<nautilus/interrupt.h>

#include<nautilus/of/dt.h>

#define NS_PER_S 1000000000ULL

#ifndef NAUT_CONFIG_DEBUG_TIMERS
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

#define TIMER_PRINT(fmt, args...) INFO_PRINT("timer: " fmt, ##args)
#define TIMER_DEBUG(fmt, args...) DEBUG_PRINT("timer: " fmt, ##args)
#define TIMER_ERROR(fmt, args...) ERROR_PRINT("timer: " fmt, ##args)
#define TIMER_WARN(fmt, args...) WARN_PRINT("timer: " fmt, ##args)

static inline void print_timer_regs(void) {
  TIMER_PRINT("CNTPCT_EL0 = %u\n", ({uint64_t reg; asm volatile("mrs %0, CNTPCT_EL0" : "=r" (reg)); reg;}));
  TIMER_PRINT("CNTFRQ_EL0 = %u\n", ({uint64_t reg; asm volatile("mrs %0, CNTFRQ_EL0" : "=r" (reg)); reg;}));
  TIMER_PRINT("CNTP_CVAL_EL0 = %u\n", ({uint64_t reg; asm volatile("mrs %0, CNTP_CVAL_EL0" : "=r" (reg)); reg;}));
  TIMER_PRINT("CNTP_TVAL_EL0 = %u\n", ({uint64_t reg; asm volatile("mrs %0, CNTP_TVAL_EL0" : "=r" (reg)); reg;}));
}

static uint32_t current_ticks = 0;

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
    //TIMER_DEBUG("arch_update_timer(%u, %u)\n", ticks, cond);
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
    get_cpu()->in_timer_interrupt = 0;
    get_cpu()->in_kick_interrupt = 0;
}

void arch_set_timer(uint32_t ticks) {
    //TIMER_DEBUG("arch_set_timer(%u) current_timer = %u\n", ticks, arch_read_timer());
   
    asm volatile("msr CNTP_TVAL_EL0, %0" :: "r" ((int)ticks));

    current_ticks = ticks;
}
int arch_read_timer(void) {
  uint64_t tval;
  asm volatile ("mrs %0, CNTP_TVAL_EL0" : "=r" (tval));
  return (tval<<32)>>32;
}

int arch_timer_handler(struct nk_irq_action*, struct nk_regs*, void *state) { 

    uint64_t time_to_next_ns;

    get_cpu()->in_timer_interrupt = 1;

    time_to_next_ns = nk_timer_handler();

    //TIMER_DEBUG("time_to_next_ns = %llu, ticks = %llu\n", time_to_next_ns, arch_realtime_to_ticks(time_to_next_ns));

    if (time_to_next_ns == -1) {
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

static struct nk_dev_int arch_timer_int = {};

static nk_irq_t arch_timer_irq_num;

int arm_timer_init_one(struct nk_dev_info *info) {

  int did_register = 0;

  nk_irq_t irqs[4];

  int num_irq = nk_dev_info_num_irq(info);
  if(num_irq < 4) {
    TIMER_ERROR("Invalid number of interrupts found in ARM timer device tree: (%u) < 4!\n", num_irq);
    goto err_exit;
  }
  else if(num_irq > 4) {
    TIMER_WARN("Unexpected number of interrupts found in ARM timer device tree: (%u) > 4! Continuing anyways...\n", num_irq);
  }

  for(int i = 0; i < 4; i++) {
    nk_irq_t irq = nk_dev_info_read_irq(info, i);
    if(irq == NK_NULL_IRQ) {
      TIMER_ERROR("Could not get interrupt %u from device info!\n");
      goto err_exit; 
    }
    irqs[i] = irq;
  }

  TIMER_DEBUG("Found interrupts: Secure(%u), Insecure(%u), Virtual(%u), Hyper(%u)\n",
      irqs[0],irqs[1],irqs[2],irqs[3]);

  struct nk_dev *dev = nk_dev_register("arch-timer", NK_DEV_TIMER, 0, &arch_timer_int, NULL);

  if(dev == NULL) {
    TIMER_ERROR("Failed to register timer device!\n");
    goto err_exit;
  }

  did_register = 1;

  arch_timer_irq_num = irqs[1];

  // install the handler
  if(nk_irq_add_handler_dev(arch_timer_irq_num, arch_timer_handler, NULL, dev)) {
    TIMER_ERROR("Failed to install the timer IRQ handler!\n");
    goto err_exit;
  }

  return 0;

err_exit:
  if(did_register) {
    nk_dev_unregister(dev);
  }
  return -1;
}

static struct of_dev_id timer_of_dev_ids[] = {
  {.compatible = "arm,armv7-timer"},
  {.compatible = "arm,armv8-timer"}
};
static struct of_dev_match timer_of_match = {
  .ids = timer_of_dev_ids,
  .num_ids = sizeof(timer_of_dev_ids)/sizeof(struct of_dev_id),
  .max_num_matches = 1
};

int global_timer_init(void) {
  return of_for_each_match(&timer_of_match, arm_timer_init_one);
}

int percpu_timer_init(void) {

#ifdef NAUT_CONFIG_DEBUG_TIMERS
  print_timer_regs();
#endif

  // Enable the timer
  asm volatile ("mrs x0, CNTP_CTL_EL0; orr x0, x0, 1; msr CNTP_CTL_EL0, x0" ::: "x0");

  nk_unmask_irq(arch_timer_irq_num);

  return 0;
}

