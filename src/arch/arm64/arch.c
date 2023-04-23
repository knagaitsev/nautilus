
#include<nautilus/arch.h>

void arch_enable_ints(void) {
  // TODO
}
void arch_disable_ints(void) {
  // TODO
}
int arch_ints_enabled(void) {
  // TODO
  return 0;
}

void arch_irq_enable(int irq) {
  // TODO
}
void arch_irq_disable(int irq) {
  // TODO
}
void arch_irq_install(int irq, int (*handler)(excp_entry_t *excp,
                                              ulong_t vector,
                                              void *state)) {
  // TODO
}

void arch_irq_uninstall(int irq) {
  // TODO
}

int arch_early_init(struct naut_info *naut) {
  // TODO
  return 0;
}
int arch_numa_init(struct sys_info *sys) {
  // TODO
  return 0;
}

void arch_detect_mem_map(mmap_info_t *mm_info, mem_map_entry_t *memory_map,
                         unsigned long mbd) {
  // TODO
}
void arch_reserve_boot_regions(unsigned long mbd) {
  // TODO
}

uint32_t arch_cycles_to_ticks(uint64_t cycles) {
  // TODO
  return 0;
}
uint32_t arch_realtime_to_ticks(uint64_t ns) {
  // TODO
  return 0;
}
uint64_t arch_realtime_to_cycles(uint64_t ns) {
  // TODO
  return 0;
}
uint64_t arch_cycles_to_realtime(uint64_t cycles) {
  // TODO
  return 0;
}

void arch_update_timer(uint32_t ticks, nk_timer_condition_t cond) {
  // TODO
}
void arch_set_timer(uint32_t ticks) {
  // TODO
}
int arch_read_timer(void) {
  // TODO
  return 0;
}
int arch_timer_handler(excp_entry_t *excp, ulong_t vec, void *state) {
  // TODO
  return 0;
}

uint64_t arch_read_timestamp(void) {
  // TODO
  return 0;
}

void arch_print_regs(struct nk_regs *r) {
  // TODO
}
void *arch_read_sp(void) {
  void *stack_ptr;
  __asm__ __volatile__ (
      "mov sp, %0"
      : "=r" (stack_ptr)
      :
      : "memory"
      );
  return stack_ptr;
}
void arch_relax(void) {
  // TODO
}
void arch_halt(void) {
  // TODO
}

