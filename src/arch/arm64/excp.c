
#include<nautilus/nautilus.h>
#include<nautilus/idt.h>
#include<nautilus/irq.h>
#include<nautilus/printk.h>

#include<arch/arm64/gic.h>
#include<arch/arm64/excp.h>

typedef union esr_el1 {
  uint64_t raw;
  struct {
    uint_t iss : 25;
    uint_t inst_length : 1;
    uint_t class : 6;
    uint_t iss2 : 5;
    // Rest reserved
  };
} esr_el1_t;

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
  uint64_t frame_ptr;
  uint64_t link_ptr;
  uint64_t elr;
  esr_el1_t esr;
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
    printk("\tINT ID = %u\n\n", gic_info.int_id);
  }

  int(*handler)(excp_entry_t*, excp_vec_t, void*);
  void *state;

  handler = __excp_irq_handler_desc_table[gic_info.int_id].handler;
  state = __excp_irq_handler_desc_table[gic_info.int_id].state;

  excp_entry_t entry = {
    .rip = info->far,
    //.rflags = The CPU saves these for us so we need to get them from a specific register
    .rsp = info->frame_ptr,
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
    printk("\tINT ID = %u\n\n", gic_info.int_id);
  }


  int(*handler)(excp_entry_t*, excp_vec_t, void*);
  void *state;

  handler = __excp_irq_handler_desc_table[gic_info.int_id].handler;
  state = __excp_irq_handler_desc_table[gic_info.int_id].state;

  excp_entry_t entry = {
    .rip = info->far,
    //.rflags = The CPU saves these for us so we need to get them from a specific register
    .rsp = info->frame_ptr
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

// These are not all possible valid values of ESR's class field
#define ESR_CLS_UNKNOWN 0x0
#define ESR_CLS_TRAPPED_WF_INSTR 0x1
#define ESR_CLS_BRANCH_TARGET 0xD
#define ESR_CLS_ILLEGAL_EXEC_STATE 0xE
#define ESR_CLS_SYSCALL32 0x11
#define ESR_CLS_SYSCALL64 0x15
#define ESR_CLS_SVE_DISABLED 0x19
#define ESR_CLS_IABORT_FROM_LOWER_EL 0x20
#define ESR_CLS_IABORT_FROM_SAME_EL 0x21
#define ESR_CLS_PC_UNALIGNED 0x22
#define ESR_CLS_DABORT_FROM_LOWER_EL 0x24
#define ESR_CLS_DABORT_FROM_SAME_EL 0x25
#define ESR_CLS_SP_UNALIGNED 0x26
#define ESR_CLS_FP32_TRAP 0x28
#define ESR_CLS_FP64_TRAP 0x2C
#define ESR_CLS_SERROR_INT 0x2F

// Double fault
void route_exception_from_kernel(struct excp_entry_info *info) {
  printk("\n--- KERNEL EXCEPTION ---\n"); 
  printk("\tELR = %p\n", info->elr);
  printk("\tFAR = %p\n", (void*)info->far);
  printk("\tESR = 0x%x\n", info->esr.raw);
  printk("\t\tCLS = 0x%x\n", info->esr.class);
  printk("\t\tISS = 0x%x\n", info->esr.iss);
  printk("\t\tISS2 = 0x%x\n", info->esr.iss2);
  if(info->esr.inst_length){
    printk("\t32bit Instruction\n");
  } else {
    printk("\t16bit Instruction\n");
  }
}

// User-mode exception
void route_exception_from_user(struct excp_entry_info *info) {
  printk("\n--- USERMODE EXCEPTION ---\n"); 
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
}

