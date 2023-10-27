
#include<arch/arm64/sys_reg.h>
#include<nautilus/nautilus.h>
#include<nautilus/naut_types.h>

cpu_id_t mpidr_el1_to_cpu_id(mpidr_el1_t *mpid) {
  struct naut_info *info = nk_get_nautilus_info();

  for(int i = 0; i < info->sys.num_cpus; i++) {
    struct cpu *cpu = info->sys.cpus[i];
    if(cpu != NULL) {
      if(cpu->aff0 == mpid->aff0 &&
         cpu->aff1 == mpid->aff1 &&
         cpu->aff2 == mpid->aff2 &&
         cpu->aff3 == mpid->aff3) {
        return cpu->id;
      }
    }
  }

  return (cpu_id_t)-1;
}

int ttbr0_el1_set_base_addr(ttbr0_el1_t *reg, void *addr) 
{
  uint64_t val = (uint64_t)addr;
  if(val & ((1ULL<<5)-1)) {
    // Unaligned
    return -1;
  }
  reg->base_addr = ((val >> 1)&((1ULL<<47)-1)); 
  return 0;
}

int ttbr1_el1_set_base_addr(ttbr1_el1_t *reg, void *addr) 
{
  uint64_t val = (uint64_t)addr;
  if(val & ((1ULL<<5)-1)) {
    // Unaligned
    return -1;
  }
  reg->base_addr = ((val >> 1)&((1ULL<<47)-1)); 
  return 0;
}

