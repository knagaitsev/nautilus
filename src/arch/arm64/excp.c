
#include<nautilus/nautilus.h>
#include<nautilus/idt.h>
#include<nautilus/irq.h>
#include<nautilus/printk.h>

#include<arch/arm64/gic.h>
#include<arch/arm64/excp.h>

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

typedef struct irq_handler_desc {
  irq_handler_t handler;
  void *state;
} irq_handler_desc_t;

irq_handler_desc_t *__excp_irq_handler_desc_table;

static int __unhandled_irq_handler(excp_entry_t *entry, excp_vec_t vec, void *state) {
  printk("\n--- UNHANDLED INTERRUPT ---\n");
  printk("\texcp_vec = %x\n\n", vec);
  return 1;
}

// Requires mm_boot to be initialized
int excp_init(void) {
  __excp_irq_handler_desc_table = mm_boot_alloc(sizeof(irq_handler_desc_t) * __gic_ptr->max_ints);
  for(uint_t i = 0; i < __gic_ptr->max_ints; i++) {
    __excp_irq_handler_desc_table[i].handler = __unhandled_irq_handler;
    __excp_irq_handler_desc_table[i].state = NULL;
  }
  return 0;
}

void excp_assign_irq_handler(int irq, irq_handler_t handler, void *state) {
  __excp_irq_handler_desc_table[irq].handler = handler;
  __excp_irq_handler_desc_table[irq].state = state;
}
void *excp_remove_irq_handler(int irq) {
  __excp_irq_handler_desc_table[irq].handler = __unhandled_irq_handler;
  void *ret = __excp_irq_handler_desc_table[irq].state;
  __excp_irq_handler_desc_table[irq].state = NULL;
  return ret;
}

void route_interrupt_from_kernel(struct excp_entry_info *info) {
  gic_int_info_t gic_info;
  gic_get_int_info(__gic_ptr, &gic_info);

  if(gic_info.int_id < 16) {
    printk("\n--- Software Generated Interrupt ---\n"); 
    printk("\tEL = 1\n");
    printk("\tINT ID = %u\n", gic_info.int_id);
    printk("\tCPU ID = %u\n\n", gic_info.cpu_id);
  }
  else {
    printk("\n--- Interrupt ---\n");
    printk("\tEL = 1\n");
    printk("\tINT ID = %u\n\n", gic_info.cpu_id);
  }

  int(*handler)(excp_entry_t*, excp_vec_t, void*);
  void *state;

  handler = __excp_irq_handler_desc_table[gic_info.int_id].handler;
  state = __excp_irq_handler_desc_table[gic_info.int_id].state;

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

  gic_end_of_int(__gic_ptr, &gic_info);
}

void route_interrupt_from_user(struct excp_entry_info *info) {
  gic_int_info_t gic_info;
  gic_get_int_info(__gic_ptr, &gic_info);

  if(gic_info.int_id < 16) {
    printk("\n--- Software Generated Interrupt ---\n");
    printk("\tEL = 0\n");
    printk("\tINT ID = %u\n", gic_info.int_id);
    printk("\tCPU ID = %u\n\n", gic_info.cpu_id);
  }
  else {
    printk("\n--- Interrupt ---\n");
    printk("\tEL = 0\n");
    printk("\tINT ID = %u\n\n", gic_info.cpu_id);
  }


  int(*handler)(excp_entry_t*, excp_vec_t, void*);
  void *state;

  handler = __excp_irq_handler_desc_table[gic_info.int_id].handler;
  state = __excp_irq_handler_desc_table[gic_info.int_id].state;

  excp_entry_t entry = {
    .rip = info->far,
    //.rflags = The CPU saves these for us so we need to get them from a specific register
    .rsp = info->sp
  };

  int status = (*handler)(&entry, gic_info.int_id, state);
  /*
  if(status) {
    printk("Error Code Returned from Handling EL1 to EL1 Exception!\n");
    printk("EC: %u, INT_ID: %u, CPUID: %u\n", status, gic_info.int_id, gic_info.cpu_id);
  }
  */

  gic_end_of_int(__gic_ptr, &info);
}

// Double fault
void route_exception_from_kernel(struct excp_entry_info *info) {
  printk("KERNEL EXCEPTION: ESR = 0x%x, FAR = %p\n", info->esr, (void*)info->far);
  while(1) {}
}

// User-mode exception
void route_exception_from_user(struct excp_entry_info *info) {
  printk("USER-MODE EXCEPTION: ESR = 0x%x, FAR = %p\n", info->esr, (void*)info->far);
  while(1) {}
}

