
#include<arch/arm64/gic.h>

#include<nautilus/printk.h>

#define GIC_PRINTK(...) printk("[GIC] " __VA_ARGS__)

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

  GIC_PRINTK("initing, DIST_BASE = %p, CPU_INTERFACE_BASE = %p\n", 
      __gic_ptr->dist_base, 
      __gic_ptr->cpu_interface_base);

  gicd_type_reg_t type_reg;
  type_reg.raw = LOAD_GICD_REG(__gic_ptr, GICD_TYPER_OFFSET);
  __gic_ptr->max_ints = (type_reg.it_lines_num+1) << 6; // 32*(N+1)
  __gic_ptr->max_cpu_interfaces = type_reg.cpu_num+1;

  GIC_PRINTK("maximum number of interrupts = %llu\n", __gic_ptr->max_ints);
  GIC_PRINTK("maximum number of cpu interfaces = %llu\n", __gic_ptr->max_cpu_interfaces);

  gicd_sgi_reg_t sgi_reg;
  sgi_reg.raw = LOAD_GICD_REG(__gic_ptr, GICD_SGIR_OFFSET);
  // Forward SGI's to the processor which requests them
  //     (other option is to send it to every processor)
  sgi_reg.target_list_filter = 0b10;
  STORE_GICD_REG(__gic_ptr, GICD_SGIR_OFFSET, sgi_reg.raw);

  gicd_ctl_reg_t ctl_reg;
  ctl_reg.raw = LOAD_GICD_REG(__gic_ptr, GICD_CTLR_OFFSET);
  ctl_reg.enabled = 1;
  STORE_GICD_REG(__gic_ptr, GICD_CTLR_OFFSET, ctl_reg.raw);

  GIC_PRINTK("enabled\n");

  return 0;
}

int per_cpu_init_gic(void) {
  gicc_ctl_reg_t ctl_reg;
  ctl_reg.raw = LOAD_GICC_REG(__gic_ptr, GICC_CTLR_OFFSET);
  ctl_reg.ack_ctl = 1;
  ctl_reg.dir_required_for_eoi = 0;
  ctl_reg.dir_required_for_eoi_alias = 0;
  STORE_GICC_REG(__gic_ptr, GICC_CTLR_OFFSET, ctl_reg.raw);
  return 0;
}

void print_gic(void) {

#define PRINT_FIELD(field) printk("\t\t" #field " = 0x%x = %u\n", field, field)
#define PRINTLN printk("\n");

/* I'll fix this later
  printk("GIC Descriptor Registers:\n");
  PRINTLN;
  gicd_ctl_reg_t *ctl_reg = get_gicd_ctl_reg(__gic_ptr);
  PRINT_FIELD(ctl_reg->raw);
  PRINT_FIELD(ctl_reg->enabled);
  PRINTLN;

  gicd_type_reg_t *type_reg = get_gicd_type_reg(__gic_ptr);
  PRINT_FIELD(type_reg->raw);
  PRINT_FIELD(type_reg->it_lines_num);
  PRINT_FIELD(type_reg->cpu_num);
  PRINT_FIELD(type_reg->security_ext);
  PRINT_FIELD(type_reg->lspi);
  PRINTLN;

  gicd_impl_id_reg_t *iid_reg = get_gicd_impl_id_reg(__gic_ptr);
  PRINT_FIELD(iid_reg->raw);
  PRINT_FIELD(iid_reg->implementor);
  PRINT_FIELD(iid_reg->revision);
  PRINT_FIELD(iid_reg->variant);
  PRINT_FIELD(iid_reg->product_id);
  PRINTLN;

  gicd_sgi_reg_t *sgi_reg = get_gicd_sgi_reg(__gic_ptr);
  PRINT_FIELD(sgi_reg->raw);
  PRINT_FIELD(sgi_reg->nsatt);
  PRINT_FIELD(sgi_reg->cpu_target_list);
  PRINT_FIELD(sgi_reg->target_list_filter);
  PRINTLN;

  printk("GIC CPU Interface Registers\n");

  gicc_ctl_reg_t *cpu_ctl_reg = get_gicc_ctl_reg(__gic_ptr);
  PRINT_FIELD(cpu_ctl_reg->raw);
  PRINT_FIELD(cpu_ctl_reg->grp0_enabled);
  PRINT_FIELD(cpu_ctl_reg->grp1_enabled);
  PRINT_FIELD(cpu_ctl_reg->ack_ctl);
  PRINT_FIELD(cpu_ctl_reg->grp0_fiq);
  PRINT_FIELD(cpu_ctl_reg->bpr_aliased);
  PRINT_FIELD(cpu_ctl_reg->sig_bypass_fiq);
  PRINT_FIELD(cpu_ctl_reg->sig_bypass_irq);
  PRINT_FIELD(cpu_ctl_reg->sig_bypass_fiq_alias);
  PRINT_FIELD(cpu_ctl_reg->sig_bypass_irq_alias);
  PRINT_FIELD(cpu_ctl_reg->eoi_disables_int);
  PRINT_FIELD(cpu_ctl_reg->eoi_disables_int_alias);
  PRINTLN;

  gicc_priority_reg_t *priority_mask_reg = get_gicc_priority_mask_reg(__gic_ptr);
  PRINT_FIELD(priority_mask_reg->raw);
  PRINT_FIELD(priority_mask_reg->priority);
  PRINTLN;

  gicc_binary_point_reg_t *binary_point_reg = get_gicc_binary_point_reg(__gic_ptr);
  PRINT_FIELD(binary_point_reg->raw);
  PRINT_FIELD(binary_point_reg->point);
  PRINTLN;

  gicc_interface_id_reg_t *cpu_interface_id_reg = get_gicc_interface_id_reg(__gic_ptr);
  PRINT_FIELD(cpu_interface_id_reg->raw);
  PRINT_FIELD(cpu_interface_id_reg->impl);
  PRINT_FIELD(cpu_interface_id_reg->revision);
  PRINT_FIELD(cpu_interface_id_reg->arch_version);
  PRINT_FIELD(cpu_interface_id_reg->product_id);
  PRINTLN;
*/
  printk("Distributor Enabled Interrupts:\n");
  for(uint_t i = 0; i < __gic_ptr->max_ints; i++) {
    if(gicd_int_enabled(__gic_ptr, i)) {
      printk("\tINTERRUPT %u: ENABLED\n", i);
    }
  }

  printk("Distributor Pending Interrupts:\n");
  for(uint_t i = 0; i < __gic_ptr->max_ints; i++) {
    if(gicd_int_pending(__gic_ptr, i)) {
      printk("\tINTERRUPT %u: PENDING\n", i);
    }
  }

  printk("Distributor Active Interrupts:\n");
  for(uint_t i = 0; i < __gic_ptr->max_ints; i++) {
    if(gicd_int_active(__gic_ptr, i)) {
      printk("\tINTERRUPT %u: ACTIVE\n", i);
    }
  }
}
