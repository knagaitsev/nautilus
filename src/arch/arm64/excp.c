
#include<nautilus/nautilus.h>
#include<nautilus/idt.h>
#include<nautilus/irq.h>
#include<nautilus/printk.h>

#include<arch/arm64/gic.h>
#include<arch/arm64/excp.h>

#define EXCP_SYNDROME_BITS 6

//#define NAUT_CONFIG_DEBUG_EXCPS

// This doesn't exist yet
#ifndef NAUT_CONFIG_DEBUG_EXCPS
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

#define EXCP_PRINT(fmt, args...) printk("excp: " fmt, ##args)
#define EXCP_DEBUG(fmt, args...) DEBUG_PRINT("excp: " fmt, ##args)
#define EXCP_ERROR(fmt, args...) ERROR_PRINT("excp: " fmt, ##args)
#define EXCP_WARN(fmt, args...) WARN_PRINT("excp: " fmt, ##args)

irq_handler_desc_t *irq_handler_desc_table;
excp_handler_desc_t *excp_handler_desc_table;

int unhandled_excp_handler(struct nk_regs *regs, struct excp_entry_info *info, uint8_t el_from, void *state) {
  if(el_from == 0) {
    EXCP_PRINT("--- UNKNOWN USERMODE EXCEPTION ---\n"); 
  }
  else if(el_from == 1) {
    EXCP_PRINT("--- UNKNOWN KERNEL EXCEPTION ---\n");
  }
  else {
    EXCP_PRINT("--- UNKNOWN EL LEVEL %u EXCEPTION ---\n", el_from);
    EXCP_PRINT("--- THIS SHOULD NOT BE POSSIBLE! ---\n");
  }
  EXCP_PRINT("\tCPU = %u\n", my_cpu_id());
  EXCP_PRINT("\tTHREAD = %p\n", get_cur_thread());
  EXCP_PRINT("\tTHREAD_NAME = %s\n", get_cur_thread()->name);
  EXCP_PRINT("\tTHREAD_BOUND_CPU = %u\n", get_cur_thread()->bound_cpu);
  EXCP_PRINT("\tTHREAD_PLACEMENT_CPU = %u\n", get_cur_thread()->placement_cpu);
  EXCP_PRINT("\tTHREAD_CURRENT_CPU = %u\n", get_cur_thread()->current_cpu);
  EXCP_PRINT("\tTHREAD_IS_IDLE = %u\n", get_cur_thread()->is_idle);
  EXCP_PRINT("\tTHREAD_FUNC = %p\n", get_cur_thread()->fun);
  EXCP_PRINT("\tTHREAD_VIRTUAL_CONSOLE = %p\n", get_cur_thread()->vc);
  EXCP_PRINT("\tELR = 0x%x\n", info->elr);
  EXCP_PRINT("\tESR = 0x%x\n", info->esr.raw);
  EXCP_PRINT("\tFAR = %p\n", (void*)info->far);
  EXCP_PRINT("\t\tCLS = 0x%x\n", info->esr.syndrome);
  EXCP_PRINT("\t\tISS = 0x%x\n", info->esr.iss);
  EXCP_PRINT("\t\tISS_2 = 0x%x\n", info->esr.iss2);
  if(info->esr.inst_length){
    EXCP_PRINT("\t32bit Instruction\n");
  } else {
    EXCP_PRINT("\t16bit Instruction\n");
  }
  arch_print_regs(regs);
  return 0;
}

// Requires mm_boot to be initialized
int excp_init(void) {
  irq_handler_desc_table = mm_boot_alloc(sizeof(irq_handler_desc_t) * gic_max_irq());
  for(uint_t i = 0; i < gic_max_irq(); i++) {
    irq_handler_desc_table[i].handler = unhandled_irq_handler;
    irq_handler_desc_table[i].state = NULL;
  }
  excp_handler_desc_table = mm_boot_alloc(sizeof(excp_handler_desc_t) * (1<<EXCP_SYNDROME_BITS));
  for(uint_t i = 0; i < (1<<EXCP_SYNDROME_BITS); i++) {
    excp_handler_desc_table[i].handler = unhandled_excp_handler;
    excp_handler_desc_table[i].state = NULL;
  }
  return 0;
}

void excp_assign_irq_handler(int irq, irq_handler_t handler, void *state) {
  irq_handler_desc_table[irq].handler = handler;
  irq_handler_desc_table[irq].state = state;
}
void *excp_remove_irq_handler(int irq) {
  irq_handler_desc_table[irq].handler = unhandled_irq_handler;
  void *ret = irq_handler_desc_table[irq].state;
  irq_handler_desc_table[irq].state = NULL;
  return ret;
}

void excp_assign_excp_handler(uint32_t syndrome, excp_handler_t handler, void *state) {
  excp_handler_desc_table[syndrome].handler = handler;
  excp_handler_desc_table[syndrome].state = state;
}
void *excp_remove_excp_handler(uint32_t syndrome) {
  excp_handler_desc_table[syndrome].handler = unhandled_excp_handler;
  void *ret = excp_handler_desc_table[syndrome].state;
  excp_handler_desc_table[syndrome].state = NULL;
  return ret;
}

void *route_interrupt(struct nk_regs *regs, struct excp_entry_info *excp_info, uint8_t el) {

  gic_int_info_t int_info;
  gic_ack_int(&int_info);

  if(int_info.int_id < 16) {
    EXCP_DEBUG("--- Software Generated Interrupt ---\n"); 
    EXCP_DEBUG("\tEL = %u\n", el);
    EXCP_DEBUG("\tINT ID = %u\n", int_info.int_id);
    EXCP_DEBUG("\tELR = 0x%x\n", excp_info->elr);
  }
  else {
    EXCP_DEBUG("--- Interrupt ---\n");
    EXCP_DEBUG("\tEL = %u\n", el);
    EXCP_DEBUG("\tINT ID = %u\n", int_info.int_id);
    EXCP_DEBUG("\tELR = 0x%x\n", excp_info->elr);

#ifdef NAUT_CONFIG_DEBUG_EXCPS
    arch_print_regs(regs);
#endif
  }
  
  int(*handler)(excp_entry_t*, excp_vec_t, void*);
  void *state;

  handler = irq_handler_desc_table[int_info.int_id].handler;
  state = irq_handler_desc_table[int_info.int_id].state;

  excp_entry_t entry = {
    .rip = excp_info->far,
    .rflags = excp_info->status,
    .rsp = regs->frame_ptr
  };

  int status = (*handler)(&entry, int_info.int_id, state);

  void *thread = NULL;
  if(per_cpu_get(in_timer_interrupt)) {
    thread = nk_sched_need_resched();
    EXCP_DEBUG("nk_sched_need_resched() returned 0x%x\n", (uint64_t)thread);
  }

  EXCP_DEBUG("END OF INTERRUPT\n");

  gic_end_of_int(&int_info);

  return thread;
}

void route_exception(struct nk_regs *regs, struct excp_entry_info *info, uint8_t el) {
  int(*handler)(struct nk_regs*, struct excp_entry_info*, uint8_t, void*) = excp_handler_desc_table[info->esr.syndrome].handler;
  int ret = (*handler)(regs, info, 1, excp_handler_desc_table[info->esr.syndrome].state);
  if(ret) {
    EXCP_ERROR("Exception handler returned error code: %u\n", ret);
  }
}

