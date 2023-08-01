
#include<nautilus/arch.h>
#include<nautilus/of/numa.h>
#include<nautilus/of/mem.h>

#include<arch/arm64/sys_reg.h>
#include<arch/arm64/unimpl.h>

// Double checking some macros
#ifndef NAUT_CONFIG_ARCH_ARM64
#error "NAUT_CONFIG_ARCH_ARM64 is not defined yet arm64 objects are being compiled!"
#endif

#ifdef NAUT_CONFIG_HOST_X86_64
#error "NAUT_CONFIG_HOST_X86_64 is defined for arm64!"
#endif
#ifdef NAUT_CONFIG_ARCH_X86
#error "NAUT_CONFIG_ARCH_X86 is defined for arm64!"
#endif
#ifdef NAUT_CONFIG_ARCH_RISCV
#error "NAUT_CONFIG_ARCH_RISCV is defined for arm64!"
#endif

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
  PRINT_REG(sp);
}

void *arch_read_sp(void) {
  void *stack_ptr;
  __asm__ __volatile__ (
      "mov %0, sp"
      : "=r" (stack_ptr)
      :
      );
  return stack_ptr;
}

void arch_relax(void) {
  __asm__ __volatile__ ("yield");
}
void arch_halt(void) {
  __asm__ __volatile__ ("wfi");
}

void arch_detect_mem_map(mmap_info_t *mm_info, mem_map_entry_t *memory_map,
                         unsigned long mbd) {
  fdt_detect_mem_map(mm_info, memory_map, mbd);
}
void arch_reserve_boot_regions(unsigned long mbd) {
  fdt_reserve_boot_regions(mbd);
}
int arch_numa_init(struct sys_info *sys) {
  return fdt_numa_init(sys);
}

int arch_little_endian(void) {
  // Technically only refers to data accesses not instruction but that's good enough
  sys_ctrl_reg_t ctlr;
  LOAD_SYS_REG(SCTLR_EL1, ctlr.raw);
  return !ctlr.el1_big_endian;
}

void *arch_instr_ptr_reg(struct nk_regs *regs) {
  // This isn't technically correct but until I merge excp_entry_t and nk_regs this will have to do - KJH
  return regs->link_ptr;
}

