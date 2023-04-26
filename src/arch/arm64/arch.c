
#include<nautilus/arch.h>

#include<arch/arm64/sys_reg.h>

void arch_enable_ints(void) {
  int_mask_reg_t reg;
  load_int_mask_reg(&reg);
  reg.fiq_masked = 0;
  reg.irq_masked = 0;
  reg.serror_masked = 0;
  reg.debug_masked = 0;
  store_int_mask_reg(&reg);
}
void arch_disable_ints(void) {
  int_mask_reg_t reg;
  load_int_mask_reg(&reg);
  reg.fiq_masked = 1;
  reg.irq_masked = 1;
  reg.serror_masked = 1;
  reg.debug_masked = 1;
  store_int_mask_reg(&reg);
}
int arch_ints_enabled(void) {
  int_mask_reg_t reg;
  load_int_mask_reg(&reg);
  return !!(reg.raw);
}

void arch_irq_enable(int irq) {
  ARM64_ERR_UNIMPL;
}
void arch_irq_disable(int irq) {
  ARM64_ERR_UNIMPL;
}
void arch_irq_install(int irq, int (*handler)(excp_entry_t *excp,
                                              ulong_t vector,
                                              void *state)) {
  ARM64_ERR_UNIMPL;
}

void arch_irq_uninstall(int irq) {
  ARM64_ERR_UNIMPL;
}

int arch_early_init(struct naut_info *naut) {
  ARM64_ERR_UNIMPL;
  return 0;
}
int arch_numa_init(struct sys_info *sys) {
  ARM64_ERR_UNIMPL;
  return 0;
}

uint32_t arch_cycles_to_ticks(uint64_t cycles) {
  ARM64_ERR_UNIMPL;
  return 0;
}
uint32_t arch_realtime_to_ticks(uint64_t ns) {
  ARM64_ERR_UNIMPL;
  return 0;
}
uint64_t arch_realtime_to_cycles(uint64_t ns) {
  ARM64_ERR_UNIMPL;
  return 0;
}
uint64_t arch_cycles_to_realtime(uint64_t cycles) {
  ARM64_ERR_UNIMPL;
  return 0;
}

void arch_update_timer(uint32_t ticks, nk_timer_condition_t cond) {
  ARM64_ERR_UNIMPL;
}
void arch_set_timer(uint32_t ticks) {
  ARM64_ERR_UNIMPL;
}
int arch_read_timer(void) {
  ARM64_ERR_UNIMPL;
  return 0;
}
int arch_timer_handler(excp_entry_t *excp, ulong_t vec, void *state) {
  ARM64_ERR_UNIMPL;
  return 0;
}

uint64_t arch_read_timestamp(void) {
  ARM64_ERR_UNIMPL;
  return 0;
}

void arch_print_regs(struct nk_regs *r) {
  ARM64_ERR_UNIMPL;
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
  __asm__ __volatile__ ("wfi");
}
void arch_halt(void) {
  __asm__ __volatile__ ("wfe");
}

