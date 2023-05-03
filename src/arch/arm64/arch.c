
#include<nautilus/arch.h>

#include<arch/arm64/sys_reg.h>
#include<arch/arm64/gic.h>

void arch_enable_ints(void) {

  gicc_ctl_reg_t ctl_reg;
  ctl_reg.raw = LOAD_GICC_REG(__gic_ptr, GICC_CTLR_OFFSET);
  ctl_reg.grp0_enabled = 1;
  ctl_reg.grp1_enabled = 1;
  STORE_GICC_REG(__gic_ptr, GICC_CTLR_OFFSET, ctl_reg.raw);

  gicc_priority_reg_t priority_reg;
  priority_reg.raw = LOAD_GICC_REG(__gic_ptr, GICC_PMR_OFFSET);
  priority_reg.priority = 0xFF;
  STORE_GICC_REG(__gic_ptr, GICC_PMR_OFFSET, priority_reg.raw);

  __asm__ __volatile__ ("msr DAIFClr, 0xF");
}
void arch_disable_ints(void) {

  gicc_ctl_reg_t ctl_reg;
  ctl_reg.raw = LOAD_GICC_REG(__gic_ptr, GICC_CTLR_OFFSET);
  ctl_reg.grp0_enabled = 0;
  ctl_reg.grp1_enabled = 0;
  STORE_GICC_REG(__gic_ptr, GICC_CTLR_OFFSET, ctl_reg.raw);

  __asm__ __volatile__ ("msr DAIFSet, 0xF");
}
int arch_ints_enabled(void) {
  int_mask_reg_t reg;
  load_int_mask_reg(&reg);
  return !!(reg.raw);
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
  // NOP (correct but bad for power
}
void arch_halt(void) {
  __asm__ __volatile__ ("wfi");
}

