
#include<nautilus/nautilus.h>
#include<nautilus/idt.h>
#include<nautilus/irq.h>
#include<nautilus/printk.h>

#include<arch/arm64/gic.h>
#include<arch/arm64/excp.h>

#define EXCP_SYNDROME_BITS 6

typedef struct irq_handler_desc {
  irq_handler_t handler;
  void *state;
} irq_handler_desc_t;

irq_handler_desc_t *__excp_irq_handler_desc_table;

typedef struct excp_handler_desc {
  excp_handler_t handler;
  void *state;
} excp_handler_desc_t;

excp_handler_desc_t *__excp_excp_handler_desc_table;

static int __unhandled_irq_handler(excp_entry_t *entry, excp_vec_t vec, void *state) {
  printk("\n--- UNHANDLED INTERRUPT ---\n");
  printk("\texcp_vec = %x\n\n", vec);
  return 1;
}

static int __unhandled_excp_handler(struct nk_regs *regs, struct excp_entry_info *info, uint8_t el_from, void *state) {
  if(el_from == 0) {
    printk("\n--- UNKNOWN USERMODE EXCEPTION ---\n"); 
  }
  else if(el_from == 1) {
    printk("\n--- UNKNOWN KERNEL EXCEPTION ---\n");
  }
  else {
    printk("\n--- UNKNOWN EL LEVEL %u EXCEPTION---\n", el_from);
  }
  printk("\tELR = %p\n", info->elr);
  printk("\tESR = 0x%x\n", info->esr.raw);
  printk("\tFAR = %p\n", (void*)info->far);
  printk("\t\tCLS = 0x%x\n", info->esr.class);
  printk("\t\tISS = 0x%x\n", info->esr.iss);
  printk("\t\tISS_2 = 0x%x\n", info->esr.iss2);
  if(info->esr.inst_length){
    printk("\t32bit Instruction\n");
  } else {
    printk("\t16bit Instruction\n");
  }
  while(1) {}
}

// Requires mm_boot to be initialized
int excp_init(void) {
  __excp_irq_handler_desc_table = mm_boot_alloc(sizeof(irq_handler_desc_t) * __gic_ptr->max_ints);
  for(uint_t i = 0; i < __gic_ptr->max_ints; i++) {
    __excp_irq_handler_desc_table[i].handler = __unhandled_irq_handler;
    __excp_irq_handler_desc_table[i].state = NULL;
  }
  __excp_excp_handler_desc_table = mm_boot_alloc(sizeof(excp_handler_desc_t) * (1<<EXCP_SYNDROME_BITS));
  for(uint_t i = 0; i < (1<<EXCP_SYNDROME_BITS); i++) {
    __excp_excp_handler_desc_table[i].handler = __unhandled_excp_handler;
    __excp_excp_handler_desc_table[i].state = NULL;
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

void excp_assign_excp_handler(uint32_t syndrome, excp_handler_t handler, void *state) {
  __excp_excp_handler_desc_table[syndrome].handler = handler;
  __excp_excp_handler_desc_table[syndrome].state = state;
}
void *excp_remove_excp_handler(uint32_t syndrome) {
  __excp_excp_handler_desc_table[syndrome].handler = __unhandled_excp_handler;
  void *ret = __excp_excp_handler_desc_table[syndrome].state;
  __excp_excp_handler_desc_table[syndrome].state = NULL;
  return ret;
}

void route_interrupt_from_kernel(struct nk_regs *regs, struct excp_entry_info *info) {
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
    printk("\tINT ID = %u\n\n", gic_info.int_id);
  }

  int(*handler)(excp_entry_t*, excp_vec_t, void*);
  void *state;

  handler = __excp_irq_handler_desc_table[gic_info.int_id].handler;
  state = __excp_irq_handler_desc_table[gic_info.int_id].state;

  excp_entry_t entry = {
    .rip = info->far,
    .rflags = info->status,
    .rsp = regs->frame_ptr,
  };

  int status = (*handler)(&entry, gic_info.int_id, state);
  if(status) {
    printk("Error Code Returned from Handling EL1 to EL1 Exception!\n");
    printk("EC: %u, INT_ID: %u, CPUID: %u\n", status, gic_info.int_id, gic_info.cpu_id);
  }

  gic_end_of_int(__gic_ptr, &gic_info);
}

void route_interrupt_from_user(struct nk_regs *regs, struct excp_entry_info *info) {
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
    printk("\tINT ID = %u\n\n", gic_info.int_id);
  }


  int(*handler)(excp_entry_t*, excp_vec_t, void*);
  void *state;

  handler = __excp_irq_handler_desc_table[gic_info.int_id].handler;
  state = __excp_irq_handler_desc_table[gic_info.int_id].state;

  excp_entry_t entry = {
    .rip = info->far,
    .rflags = info->status,
    .rsp = regs->frame_ptr
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
void route_exception_from_kernel(struct nk_regs *regs, struct excp_entry_info *info) {
  int(*handler)(struct nk_regs*, struct excp_entry_info*, uint8_t, void*) = __excp_excp_handler_desc_table[info->esr.class].handler;
  int ret = (*handler)(regs, info, 1, __excp_excp_handler_desc_table[info->esr.class].state);
  if(ret) {
    printk("Exception handler returned error code: %u\n", ret);
  }
}

// User-mode exception
void route_exception_from_user(struct nk_regs *regs, struct excp_entry_info *info) {
  int(*handler)(struct nk_regs*, struct excp_entry_info*, uint8_t, void*) = __excp_excp_handler_desc_table[info->esr.class].handler;
  int ret = (*handler)(regs, info, 0, __excp_excp_handler_desc_table[info->esr.class].state);
  if(ret) {
    printk("Exception handler returned error code: %u\n", ret);
  }
}

