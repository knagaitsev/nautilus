
#include<nautilus/arch.h>

#include<arch/arm64/sys_reg.h>
#include<arch/arm64/gic.h>

void arch_enable_ints(void) {
  __asm__ __volatile__ ("msr DAIFClr, 0xF; isb");
}
void arch_disable_ints(void) {
  __asm__ __volatile__ ("msr DAIFSet, 0xF; isb");
}
int arch_ints_enabled(void) {
  uint_t daif;
  __asm__ __volatile__ ("mrs %0, DAIF" : "=r" (daif));
  return !(daif);
}

void arch_print_regs(struct nk_regs *r) {

#define PRINT_REG(REG) printk("\t"#REG" = 0x%x = %u\n", r->REG, r->REG)

  printk("--- Registers ---\n");
  PRINT_REG(x0);
  PRINT_REG(x1);
  PRINT_REG(x2);
  PRINT_REG(x3);
  PRINT_REG(x4);
  PRINT_REG(x5);
  PRINT_REG(x6);
  PRINT_REG(x7);
  PRINT_REG(x8);
  PRINT_REG(x9);
  PRINT_REG(x10);
  PRINT_REG(x11);
  PRINT_REG(x12);
  PRINT_REG(x13);
  PRINT_REG(x14);
  PRINT_REG(x15);
  PRINT_REG(x16);
  PRINT_REG(x17);
  PRINT_REG(x18);
  PRINT_REG(frame_ptr);
  PRINT_REG(link_ptr);
  printk("--- End Registers ---\n");
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
  // NOP (correct but bad for power)
}
void arch_halt(void) {
  __asm__ __volatile__ ("wfi");
}

