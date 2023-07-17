
#include<nautilus/nautilus.h>
#include<nautilus/interrupt.h>
#include<nautilus/irq.h>
#include<nautilus/printk.h>

#include<arch/arm64/excp.h>

#define EXCP_SYNDROME_BITS 6

excp_handler_desc_t *excp_handler_desc_table;

int unhandled_excp_handler(struct nk_regs *regs, struct excp_info *info, uint8_t el_from, uint8_t sync, void *state) {

    if(el_from == 0) {
      ERROR_PRINT("--- UNKNOWN USERMODE EXCEPTION ---\n"); 
    }
    else if(el_from == 1) {
      ERROR_PRINT("--- UNKNOWN KERNEL EXCEPTION ---\n");
    }
    else {
      ERROR_PRINT("--- UNKNOWN EL LEVEL %u EXCEPTION ---\n", el_from);
      ERROR_PRINT("--- THIS SHOULD NOT BE POSSIBLE! ---\n");
    }

    ERROR_PRINT("TYPE = %s\n", sync ? "Synchronous" : "SError");
    ERROR_PRINT("\tCPU = %u\n", my_cpu_id());
    ERROR_PRINT("\tTHREAD = %p\n", get_cur_thread());
    ERROR_PRINT("\tTHREAD_ID = %p\n", get_cur_thread()->tid);
    ERROR_PRINT("\tTHREAD_STACK_TOP = %p\n", get_cur_thread()->stack);
    ERROR_PRINT("\tTHREAD_STACK_PTR = %p\n", regs->sp);
    ERROR_PRINT("\tTHREAD_STACK_SIZE = %p\n", get_cur_thread()->stack_size);
    ERROR_PRINT("\tTHREAD_STACK_ALLOCATED = %p\n", get_cur_thread()->stack + get_cur_thread()->stack_size - (regs->sp));
    ERROR_PRINT("\tTHREAD_NAME = %s\n", get_cur_thread()->name);
    ERROR_PRINT("\tTHREAD_BOUND_CPU = %u\n", get_cur_thread()->bound_cpu);
    ERROR_PRINT("\tTHREAD_PLACEMENT_CPU = %u\n", get_cur_thread()->placement_cpu);
    ERROR_PRINT("\tTHREAD_CURRENT_CPU = %u\n", get_cur_thread()->current_cpu);
    ERROR_PRINT("\tTHREAD_IS_IDLE = %u\n", get_cur_thread()->is_idle);
    ERROR_PRINT("\tTHREAD_FUNC = %p\n", get_cur_thread()->fun);
    ERROR_PRINT("\tTHREAD_VIRTUAL_CONSOLE = %p\n", get_cur_thread()->vc);
    ERROR_PRINT("\tELR = %p\n", info->elr);
    ERROR_PRINT("\tESR = %p\n", info->esr.raw);
    ERROR_PRINT("\tFAR = %p\n", (void*)info->far);
    ERROR_PRINT("\t\tCLS = 0x%x\n", info->esr.syndrome);
    ERROR_PRINT("\t\tISS = 0x%x\n", info->esr.iss);
    ERROR_PRINT("\t\tISS_2 = 0x%x\n", info->esr.iss2);

    if(info->esr.inst_length){
        ERROR_PRINT("\t32bit Instruction\n");
    } else {
        ERROR_PRINT("\t16bit Instruction\n");
    }

    arch_print_regs(regs);

    return 0;
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

void route_exception(struct nk_regs *regs, struct excp_info *info, uint8_t el, uint8_t sync) {
  int(*handler)(struct nk_regs*, struct excp_info*, uint8_t, uint8_t, void*) = excp_handler_desc_table[info->esr.syndrome].handler;
  int ret = (*handler)(regs, info, el, sync, excp_handler_desc_table[info->esr.syndrome].state);
  if(ret) {
    ERROR_PRINT("Exception handler returned error code: %u\n", ret);
  }
}

void * route_interrupt(struct nk_regs *regs, struct excp_info *info, uint8_t el) 
{
  struct nk_irq_dev *irq_dev = per_cpu_get(irq_dev);
  
  nk_irq_t irq;
  nk_irq_dev_ack(irq_dev, &irq);

  struct nk_ivec_desc *desc = nk_irq_to_desc(irq);
  nk_handle_irq_actions(desc, regs);

  nk_irq_dev_eoi(irq_dev, irq);
 
  void *thread = NULL;
  if(per_cpu_get(in_timer_interrupt)) {
    thread = nk_sched_need_resched();
  }

  return thread;
}
