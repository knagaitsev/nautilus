
#include<nautilus/fpu.h>

#include<arch/arm64/sys_reg.h>

void fpu_init(struct naut_info *info, int is_ap) {

  cpacr_el1_t cpacr;
  LOAD_SYS_REG(CPACR_EL1, cpacr.raw);

  cpacr.fp_enabled = 1;
  cpacr.sve_enabled = 1;

  STORE_SYS_REG(CPACR_EL1, cpacr.raw);

  asm volatile ("isb");

  fpcr_t fpcr;
  LOAD_SYS_REG(FPCR, fpcr.raw);

  // Here we can modify the fpu settings

  STORE_SYS_REG(FPCR, fpcr.raw);
}

