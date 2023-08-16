#include <nautilus/nautilus.h>
//#include <nautilus/acpi.h>
#include <nautilus/smp.h>
//#include <nautilus/sfi.h>
#include <nautilus/irq.h>
#include <nautilus/mm.h>
#include <nautilus/percpu.h>
#include <nautilus/numa.h>
#include <nautilus/cpu.h>
#include <nautilus/endian.h>
#include <nautilus/of/fdt.h>

#include <arch/arm64/sys_reg.h>

#ifndef NAUT_CONFIG_DEBUG_SMP
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif
#define SMP_PRINT(fmt, args...) printk("SMP: " fmt, ##args)
#define SMP_DEBUG(fmt, args...) DEBUG_PRINT("SMP: " fmt, ##args)
#define SMP_WARN(fmt, args...) WARN_PRINT("SMP: " fmt, ##args)
#define SMP_ERROR(fmt, args...) ERROR_PRINT("SMP: " fmt, ##args)

// Initializes the TPIDR_EL1 register with the correct
// "struct cpu*" for the current PE
int smp_init_tpidr(void) 
{
  mpidr_el1_t mpidr;
  LOAD_SYS_REG(MPIDR_EL1, mpidr.raw);

  extern struct naut_info nautilus_info;
  
  struct cpu *cpu;
  for(int i = 0; i < nautilus_info.sys.num_cpus; i++) 
  {
    struct cpu *cur = nautilus_info.sys.cpus[i];
    if((cur->aff0 == mpidr.aff0) &&
       (cur->aff1 == mpidr.aff1) &&
       (cur->aff2 == mpidr.aff2) &&
       (cur->aff3 == mpidr.aff3)) {
      cpu = cur;
      break;
    } 
  }

  if(cpu == NULL) {
    SMP_ERROR("Cannot find CPU struct for processor with aff0 = %x, aff1 = %x, aff2 = %x, aff3 = %x\n", 
        mpidr.aff0, mpidr.aff1, mpidr.aff2, mpidr.aff3);
    return 1;
  }

  STORE_SYS_REG(TPIDR_EL1, (uint64_t)(void*)cpu);
  return 0;
}

static int fdt_handle_cpu_node (unsigned long fdt, int offset, int cpu_addr_cells, struct sys_info *sys) 
{
    SMP_PRINT("Found CPU in Device Tree!\n");

    struct cpu * new_cpu = NULL;

    if (sys->num_cpus == NAUT_CONFIG_MAX_CPUS) {
        SMP_ERROR("CPU count exceeded max (check your .config) max = %u\n", NAUT_CONFIG_MAX_CPUS);
        goto err_exit;
    }
    
    new_cpu = mm_boot_alloc_aligned(sizeof(struct cpu), 8);
    if(new_cpu == NULL) {
        SMP_ERROR("Couldn't allocate CPU struct!\n");
        goto err_exit;
    } 
    memset(new_cpu, 0, sizeof(struct cpu));

    int lenp;
 
    // ID
    new_cpu->id         = sys->num_cpus;

    // Status
    char *status = fdt_getprop(fdt, offset, "status", &lenp);
    if (status && lenp >= 8 && !strncmp(status, "disabled", 8)) {
      new_cpu->enabled = 0;
    } else {
      new_cpu->enabled = 1;
    }

    // Affinity
    uint32_t *regs = fdt_getprop(fdt, offset, "reg", &lenp); 
    if(regs == NULL) {
      SMP_ERROR("Cannot get \"regs\" property in \"cpu\" device tree node!\n");
      goto err_free;
    } else if(cpu_addr_cells == 1 && lenp == 4) {
      uint32_t reg0 = be32toh(regs[0]);
      new_cpu->aff0 = (uint8_t)(reg0 & 0xFF);
      new_cpu->aff1 = (uint8_t)((reg0 >> 8) & 0xFF);
      new_cpu->aff2 = (uint8_t)((reg0 >> 16) & 0xFF);
      new_cpu->aff3 = 0;
    } else if(cpu_addr_cells == 2 && lenp == 8) {
      uint32_t reg0 = be32toh(regs[0]);
      uint32_t reg1 = be32toh(regs[1]);
      new_cpu->aff0 = (uint8_t)(reg1 & 0xFF);
      new_cpu->aff1 = (uint8_t)((reg1 >> 8) & 0xFF);
      new_cpu->aff2 = (uint8_t)((reg1 >> 16) & 0xFF);
      new_cpu->aff3 = (uint8_t)(reg0 & 0xFF);
    } else {
      SMP_ERROR("Invalid \"regs\" property length and/or cpus \"#address-cells\"!\n");
      goto err_free;
    }

    new_cpu->is_bsp     = new_cpu->aff0 |
                          new_cpu->aff1 |
                          new_cpu->aff2 |
                          new_cpu->aff3 == 0;

    // Other fields
    new_cpu->cpu_sig    = 0;
    new_cpu->feat_flags = 0;
    new_cpu->system     = sys;
    new_cpu->cpu_khz    = 0;

    new_cpu->irq_dev = NULL;

    SMP_PRINT("CPU %u\n", new_cpu->id);
    SMP_PRINT("\tEnabled?=%01d\n", new_cpu->enabled);
    SMP_PRINT("\tBSP?=%01d\n", new_cpu->is_bsp);
    SMP_PRINT("\tAFF0=%x\n", new_cpu->aff0);
    SMP_PRINT("\tAFF1=%x\n", new_cpu->aff1);
    SMP_PRINT("\tAFF2=%x\n", new_cpu->aff2);
    SMP_PRINT("\tAFF3=%x\n", new_cpu->aff3);

    spinlock_init(&new_cpu->lock);

    sys->cpus[sys->num_cpus] = new_cpu;
    sys->num_cpus++;

    return 0;

err_free:
    mm_boot_free(new_cpu, sizeof(struct cpu));
err_exit:
    return -1;
}

static int fdt_init_cpus(void *dtb, struct sys_info *sys) 
{
  int cpus_offset = fdt_subnode_offset_namelen(dtb, 0, "cpus", 4);
  if(cpus_offset < 0) {
    SMP_ERROR("Could not find \"cpus\" node in device tree!\n");
    return -1;
  }

  // #address-cells tells us how to associate MPIDR_EL1 registers with nodes
  int lenp;
  uint32_t *cpu_addr_cells_ptr = fdt_getprop(dtb, cpus_offset, "#address-cells", &lenp);
  if(cpu_addr_cells_ptr == NULL || lenp != 4) {
    SMP_ERROR("Could not read \"#address-cells\" property of \"cpus\" device tree node!\n");
    return -1;
  } 
  uint32_t cpu_addr_cells = be32toh(*cpu_addr_cells_ptr);
 
  // #size-cells should always be present and always be zero,
  // but if it's absent or non-zero it shouldn't be a problem, just weird.
  uint32_t *cpu_size_cells_ptr = fdt_getprop(dtb, cpus_offset, "#size-cells", &lenp);
  if(cpu_size_cells_ptr == NULL || lenp != 4) {
    SMP_WARN("Could not read \"#size-cells\" property of \"cpus\" device tree node.\n");
  } else {
    uint32_t cpu_size_cells = be32toh(*cpu_size_cells_ptr);
    if(cpu_size_cells != 0) {
      SMP_WARN("Non-zero \"#size-cells\" property in \"cpus\" device tree node.\n");
    }
  }

  int cpu_node_offset;
  int err_count = 0;
  fdt_for_each_subnode(cpu_node_offset, dtb, cpus_offset) 
  {
    int lenp;
    char *dev_type_prop = fdt_getprop(dtb, cpu_node_offset, "device_type", &lenp);
    if(dev_type_prop == NULL) {
      SMP_DEBUG("cpus subnode (%u) missing device_type property\n", cpu_node_offset);
      continue;
    }
    if(!(lenp >= 3 && (strncmp(dev_type_prop, "cpu", 3) == 0))) {
      SMP_DEBUG("cpus subnode (%u) has device_type property but it is not \"cpu\"\n", cpu_node_offset);
      continue;
    } 

    // This is a cpu node
    if(fdt_handle_cpu_node(dtb, cpu_node_offset, cpu_addr_cells, sys)) {
      SMP_ERROR("Could not initialize FDT CPU node at offset: %u!\n", cpu_node_offset);
      err_count += 1;
    }
  }

  if(err_count > 0) {
    SMP_ERROR("Could not initialize (%u) CPU nodes!\n", err_count);
    return -1;
  }

  SMP_DEBUG("finished FDT SMP init\n");
  return 0;
}

int 
arch_smp_early_init (struct naut_info * naut)
{
    int ret = fdt_init_cpus(naut->sys.dtb, &naut->sys);

    SMP_PRINT("Detected %d CPUs\n", naut->sys.num_cpus);
    return 0;
}

// RISCV HACK

// /* Some fakery to get the scheduler working */
// uint8_t
// nk_topo_cpus_share_socket (struct cpu * a, struct cpu * b)
// {
//     return 1;
//     // return a->coord->pkg_id == b->coord->pkg_id;
// }

// uint8_t
// nk_topo_cpus_share_phys_core (struct cpu * a, struct cpu * b)
// {
//     return nk_topo_cpus_share_socket(a, b) && (a->coord->core_id == b->coord->core_id);
// }
