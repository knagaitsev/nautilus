
#include<arch/arm64/gic.h>

#include<nautilus/printk.h>

#define GIC_PRINT(...) printk("[GIC] " __VA_ARGS__)

gic_t *__gic_ptr;

#define QEMU_VIRT_GIC_DISTRIBUTOR_BASE 0x8000000
#define QEMU_VIRT_GIC_CPU_INTERFACE_BASE 0x8010000

int global_init_gic(void *dtb, gic_t *gic) {

  // Use this GIC as the global one
  __gic_ptr = gic;
  
  // Later on we should actually parse the fdt correctly to get these values
  // but for now because we're targeting qemu-virt, the values are known
  __gic_ptr->dist_base = (void*)QEMU_VIRT_GIC_DISTRIBUTOR_BASE;
  __gic_ptr->cpu_interface_base = (void*)QEMU_VIRT_GIC_CPU_INTERFACE_BASE;

  GIC_PRINT("initing, DIST_BASE = %p, CPU_INTERFACE_BASE = %p\n", 
      __gic_ptr->dist_base, 
      __gic_ptr->cpu_interface_base);

  gicd_type_reg_t type_reg;
  type_reg.raw = LOAD_GICD_REG(__gic_ptr, GICD_TYPER_OFFSET);
  __gic_ptr->max_ints = (type_reg.it_lines_num+1) << 6; // 32*(N+1)
  __gic_ptr->max_cpu_interfaces = type_reg.cpu_num+1;

  GIC_PRINT("maximum number of interrupts = %llu\n", __gic_ptr->max_ints);
  GIC_PRINT("maximum number of cpu interfaces = %llu\n", __gic_ptr->max_cpu_interfaces);

  gicd_ctl_reg_t ctl_reg;
  ctl_reg.raw = LOAD_GICD_REG(__gic_ptr, GICD_CTLR_OFFSET);
  ctl_reg.grp0_enabled = 1;
  ctl_reg.grp1_enabled = 1;
  STORE_GICD_REG(__gic_ptr, GICD_CTLR_OFFSET, ctl_reg.raw);

  GIC_PRINT("enabled\n");

  return 0;
}

int per_cpu_init_gic(void) {

  gicc_ctl_reg_t ctl_reg;
  ctl_reg.raw = LOAD_GICC_REG(__gic_ptr, GICC_CTLR_OFFSET);
  ctl_reg.grp0_enabled = 1;
  ctl_reg.grp1_enabled = 1;
  ctl_reg.ack_ctl = 1;
  ctl_reg.dir_required_for_eoi = 0;
  ctl_reg.dir_required_for_eoi_alias = 0;
  STORE_GICC_REG(__gic_ptr, GICC_CTLR_OFFSET, ctl_reg.raw);

  gicc_priority_reg_t priority_mask_reg;
  priority_mask_reg.raw = LOAD_GICC_REG(__gic_ptr, GICC_PMR_OFFSET);
  priority_mask_reg.priority = 0xFF;
  STORE_GICC_REG(__gic_ptr, GICC_PMR_OFFSET, priority_mask_reg.raw);

  return 0;
}

void print_gic(void) {

#define PRINT_FIELD(field) printk("[GIC]\t\t" #field " = 0x%x = %u\n", field, field)
#define PRINTLN printk("[GIC]\n");

  printk("GIC Descriptor Registers:\n");
  PRINTLN;
  gicd_ctl_reg_t ctl_reg;
  ctl_reg.raw = LOAD_GICD_REG(__gic_ptr, GICD_CTLR_OFFSET);
  PRINT_FIELD(ctl_reg.raw);
  PRINT_FIELD(ctl_reg.grp0_enabled);
  PRINT_FIELD(ctl_reg.grp1_enabled);
  PRINT_FIELD(ctl_reg.write_pending);
  PRINTLN;

  gicd_type_reg_t type_reg;
  type_reg.raw = LOAD_GICD_REG(__gic_ptr, GICD_TYPER_OFFSET);
  PRINT_FIELD(type_reg.raw);
  PRINT_FIELD(type_reg.it_lines_num);
  PRINT_FIELD(type_reg.cpu_num);
  PRINT_FIELD(type_reg.security_ext);
  PRINT_FIELD(type_reg.lspi);
  PRINTLN;

  gicd_impl_id_reg_t iid_reg;
  iid_reg.raw = LOAD_GICD_REG(__gic_ptr, GICD_IIDR_OFFSET);
  PRINT_FIELD(iid_reg.raw);
  PRINT_FIELD(iid_reg.implementor);
  PRINT_FIELD(iid_reg.revision);
  PRINT_FIELD(iid_reg.variant);
  PRINT_FIELD(iid_reg.product_id);
  PRINTLN;

  GIC_PRINT("GIC CPU Interface Registers\n");

  gicc_ctl_reg_t cpu_ctl_reg;
  cpu_ctl_reg.raw = LOAD_GICC_REG(__gic_ptr, GICC_CTLR_OFFSET);
  PRINT_FIELD(cpu_ctl_reg.raw);
  PRINT_FIELD(cpu_ctl_reg.grp0_enabled);
  PRINT_FIELD(cpu_ctl_reg.grp1_enabled);
  PRINT_FIELD(cpu_ctl_reg.ack_ctl);
  PRINT_FIELD(cpu_ctl_reg.grp0_fiq);
  PRINT_FIELD(cpu_ctl_reg.bpr_aliased);
  PRINT_FIELD(cpu_ctl_reg.sig_bypass_fiq);
  PRINT_FIELD(cpu_ctl_reg.sig_bypass_irq);
  PRINT_FIELD(cpu_ctl_reg.sig_bypass_fiq_alias);
  PRINT_FIELD(cpu_ctl_reg.sig_bypass_irq_alias);
  PRINT_FIELD(cpu_ctl_reg.dir_required_for_eoi);
  PRINT_FIELD(cpu_ctl_reg.dir_required_for_eoi_alias);
  PRINTLN;

  gicc_priority_reg_t priority_mask_reg;
  priority_mask_reg.raw = LOAD_GICC_REG(__gic_ptr, GICC_PMR_OFFSET);
  PRINT_FIELD(priority_mask_reg.raw);
  PRINT_FIELD(priority_mask_reg.priority);
  PRINTLN;

  gicc_binary_point_reg_t binary_point_reg;
  binary_point_reg.raw = LOAD_GICC_REG(__gic_ptr, GICC_BPR_OFFSET);
  PRINT_FIELD(binary_point_reg.raw);
  PRINT_FIELD(binary_point_reg.point);
  PRINTLN;

  gicc_interface_id_reg_t cpu_interface_id_reg;
  cpu_interface_id_reg.raw = LOAD_GICC_REG(__gic_ptr, GICC_IIDR_OFFSET);
  PRINT_FIELD(cpu_interface_id_reg.raw);
  PRINT_FIELD(cpu_interface_id_reg.impl);
  PRINT_FIELD(cpu_interface_id_reg.revision);
  PRINT_FIELD(cpu_interface_id_reg.arch_version);
  PRINT_FIELD(cpu_interface_id_reg.product_id);
  PRINTLN;

  GIC_PRINT("Distributor Enabled Interrupts:\n");
  for(uint_t i = 0; i < __gic_ptr->max_ints; i++) {
    if(gicd_int_enabled(__gic_ptr, i)) {
      GIC_PRINT("\tINTERRUPT %u: ENABLED\n", i);
    }
  }

  GIC_PRINT("Distributor Pending Interrupts:\n");
  for(uint_t i = 0; i < __gic_ptr->max_ints; i++) {
    if(gicd_int_pending(__gic_ptr, i)) {
      GIC_PRINT("\tINTERRUPT %u: PENDING\n", i);
    }
  }

  GIC_PRINT("Distributor Active Interrupts:\n");
  for(uint_t i = 0; i < __gic_ptr->max_ints; i++) {
    if(gicd_int_active(__gic_ptr, i)) {
      GIC_PRINT("\tINTERRUPT %u: ACTIVE\n", i);
    }
  }
}
