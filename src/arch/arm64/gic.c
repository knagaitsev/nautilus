
#include<arch/arm64/gic.h>

#include<nautilus/printk.h>

#define GIC_PRINTK(...) printk("[GIC] " __VA_ARGS__)

int init_gic(void *dtb, gic_t *gic) {
  // Later on we should actually parse the fdt correctly to get these values
  // but for now because we're targeting qemu-virt, the values are known
  gic->dist_base = 0x8000000;
  gic->cpu_interface_base = 0x8020000;

  GIC_PRINTK("initing, DIST_BASE = %p, CPU_INTERFACE_BASE = %p\n", 
      gic->dist_base, 
      gic->cpu_interface_base);

  gicd_type_reg_t *type_reg = get_gicd_type_reg(gic);
  gic->max_ints = (type_reg->it_lines_num+1) << 6; // 32*(N+1)
  gic->max_cpu_interfaces = type_reg->cpu_num+1;

  GIC_PRINTK("maximum number of interrupts = %llu\n", gic->max_ints);
  GIC_PRINTK("maximum number of cpu interfaces = %llu\n", gic->max_cpu_interfaces);

  return 0;
}

