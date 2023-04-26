
#include<nautilus/nautilus.h>
#include<nautilus/idt.h>
#include<nautilus/irq.h>
#include<nautilus/printk.h>

#include<arch/arm64/gic.h>

struct __attribute__((packed)) excp_entry_info {
  uint64_t x0;
  uint64_t x1;
  uint64_t x2;
  uint64_t x3;
  uint64_t x4;
  uint64_t x5;
  uint64_t x6;
  uint64_t x7;
  uint64_t x8;
  uint64_t x9;
  uint64_t x10;
  uint64_t x11;
  uint64_t x12;
  uint64_t x13;
  uint64_t x14;
  uint64_t x15;
  uint64_t x16;
  uint64_t x17;
  uint64_t x18;
  uint64_t sp;
  uint64_t frame_ptr;
  uint64_t link_ptr;
  uint64_t esr;
  uint64_t far;
};

struct gic irq_gic;

void el1_to_el1_interrupt(struct excp_entry_info *info) {
  gic_int_info_t gic_info;
  gic_get_int_info(&irq_gic, &gic_info);

  int(*handler)(excp_entry_t*, excp_vec_t, void*);
  void *state;

  idt_get_entry(gic_info.int_id, &handler, &state);

  excp_entry_t entry = {
    .rip = info->far,
    //.rflags = The CPU saves these for us so we need to get them from a specific register
    .rsp = info->sp
  };

  int status = (*handler)(&entry, gic_info.int_id, state);
  if(status) {
    printk("Error Code Returned from Handling EL1 to EL1 Exception!\n");
    printk("EC: %u, INT_ID: %u, CPUID: %u\n", status, gic_info.int_id, gic_info.cpu_id);
  }

  gic_end_of_int(&irq_gic, &info);
}

void el0_to_el1_interrupt(struct excp_entry_info *info) {
  gic_int_info_t gic_info;
  gic_get_int_info(&irq_gic, &gic_info);

  int(*handler)(excp_entry_t*, excp_vec_t, void*);
  void *state;

  idt_get_entry(gic_info.int_id, &handler, &state);

  excp_entry_t entry = {
    .rip = info->far,
    //.rflags = The CPU saves these for us so we need to get them from a specific register
    .rsp = info->sp
  };

  int status = (*handler)(&entry, gic_info.int_id, state);
  if(status) {
    printk("Error Code Returned from Handling EL1 to EL1 Exception!\n");
    printk("EC: %u, INT_ID: %u, CPUID: %u\n", status, gic_info.int_id, gic_info.cpu_id);
  }

  gic_end_of_int(&irq_gic, &info);
}

// Double fault
void el1_to_el1_exception(struct excp_entry_info *info) {
  printk("EL1 to EL1 EXCEPTION: ESR = 0x%x, FAR = %p\n", info->esr, (void*)info->far);
  while(1) {}
}

// User-mode exception
void el0_to_el1_exception(struct excp_entry_info *info) {
  printk("EL0 to EL1 EXCEPTION: ESR = 0x%x, FAR = %p\n", info->esr, (void*)info->far);
  while(1) {}
}

