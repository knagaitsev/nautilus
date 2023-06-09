#include <nautilus/nautilus.h>
//#include <nautilus/acpi.h>
#include <nautilus/smp.h>
//#include <nautilus/sfi.h>
#include <nautilus/irq.h>
#include <nautilus/mm.h>
#include <nautilus/percpu.h>
#include <nautilus/numa.h>
#include <nautilus/cpu.h>
#include <nautilus/fdt.h>

#ifndef NAUT_CONFIG_DEBUG_SMP
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif
#define SMP_PRINT(fmt, args...) printk("SMP: " fmt, ##args)
#define SMP_DEBUG(fmt, args...) DEBUG_PRINT("SMP: " fmt, ##args)
#define SMP_ERROR(fmt, args...) ERROR_PRINT("SMP: " fmt, ##args)

static struct sys_info * sys;

static int
configure_cpu (unsigned long fdt, int offset) {

    off_t reg_addr = fdt_getreg_address(fdt, offset);
    int lenp = 0;
    char *status = fdt_getprop(fdt, offset, "status", &lenp);
    int enabled = 1;

    if (status && !strcmp(status, "disabled")) {
        enabled = 0;
    }

    struct cpu * new_cpu = NULL;

    if (sys->num_cpus == NAUT_CONFIG_MAX_CPUS) {
        panic("CPU count exceeded max (check your .config)\n");
    }

    if(!(new_cpu = mm_boot_alloc(sizeof(struct cpu)))) {
        panic("Couldn't allocate CPU struct\n");
    } 

    memset(new_cpu, 0, sizeof(struct cpu));

    new_cpu->id         = reg_addr;
    new_cpu->lapic_id   = 0;

    new_cpu->enabled    = enabled;
    new_cpu->is_bsp     = (new_cpu->id == sys->bsp_id ? 1 : 0);
    new_cpu->cpu_sig    = 0;
    new_cpu->feat_flags = 0;
    new_cpu->system     = sys;
    new_cpu->cpu_khz    = 0;

    SMP_DEBUG("CPU %u\n", new_cpu->id);
    SMP_DEBUG("\tEnabled?=%01d\n", new_cpu->enabled);
    SMP_DEBUG("\tBSP?=%01d\n", new_cpu->is_bsp);

    spinlock_init(&new_cpu->lock);

    sys->cpus[new_cpu->id] = new_cpu;
    sys->num_cpus++;


    return 0;
}

int fdt_node_get_cpu(const void *fdt, int offset, int depth) {
    int lenp = 0;
    char *name = fdt_get_name(fdt, offset, &lenp);
    // maybe not the ideal way to do this
    if(name && !strncmp(name, "cpu@", 4)) {
        // printk("Hit: %s\n", name);
        configure_cpu(fdt, offset);
    }

    return 1;
}

void parse_cpus(unsigned long fdt) {
  uint64_t offset = fdt_node_offset_by_prop_value(fdt, -1, "device_type", "cpu", 4);
  while(offset != -FDT_ERR_NOTFOUND) {
    configure_cpu(fdt, offset);
    offset = fdt_node_offset_by_prop_value(fdt, offset, "device_type", "cpu", 4);
  }
}

static int __early_init_dtb(struct naut_info * naut) {
    sys = &(naut->sys);
    parse_cpus(naut->sys.dtb);
    return 0;
}

int 
arch_early_init (struct naut_info * naut)
{
    int ret;

    SMP_DEBUG("Checking for DTB\n");

    if (__early_init_dtb(naut) == 0) {
        SMP_DEBUG("DTB init succeeded\n");
        goto out_ok;
    } else {
        panic("DTB not present/working! Cannot detect CPUs. Giving up.\n");
    }

out_ok:
    SMP_PRINT("Detected %d CPUs\n", naut->sys.num_cpus);
    return 0;
}

// Stealing the...
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
