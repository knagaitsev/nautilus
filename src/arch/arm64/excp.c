
#include<nautilus/nautilus.h>
#include<nautilus/interrupt.h>
#include<nautilus/irq.h>
#include<nautilus/printk.h>

#include<arch/arm64/excp.h>

#define EXCP_SYNDROME_BITS 6

int unhandled_excp_handler(struct nk_regs *regs, struct excp_info *info, uint8_t el_from, uint8_t sync, void *state) {

    if(el_from == 0) {
      printk("--- UNKNOWN USERMODE EXCEPTION ---\n"); 
    }
    else if(el_from == 1) {
      printk("--- UNKNOWN KERNEL EXCEPTION ---\n");
    }
    else {
      printk("--- UNKNOWN EL LEVEL %u EXCEPTION ---\n", el_from);
      printk("--- THIS SHOULD NOT BE POSSIBLE! ---\n");
    }

    printk("TYPE = %s\n", sync ? "Synchronous" : "SError");
    void *cpu = get_cpu();
    if(cpu != NULL){
      printk("\tCPU = %u\n", my_cpu_id());
    } else {
      printk("\tTaken before per_cpu system is intialized\n");
    }
    if(cpu != NULL) {
      printk("\tTHREAD = %p\n", get_cur_thread());
      if(get_cur_thread() != NULL) {
        printk("\tTHREAD_ID = %p\n", get_cur_thread()->tid);
        printk("\tTHREAD_STACK_TOP = %p\n", get_cur_thread()->stack);
        printk("\tTHREAD_STACK_PTR = %p\n", regs->sp);
        printk("\tTHREAD_STACK_SIZE = %p\n", get_cur_thread()->stack_size);
        printk("\tTHREAD_STACK_ALLOCATED = %p\n", get_cur_thread()->stack + get_cur_thread()->stack_size - (regs->sp));
        printk("\tTHREAD_NAME = %s\n", get_cur_thread()->name);
        printk("\tTHREAD_BOUND_CPU = %u\n", get_cur_thread()->bound_cpu);
        printk("\tTHREAD_PLACEMENT_CPU = %u\n", get_cur_thread()->placement_cpu);
        printk("\tTHREAD_CURRENT_CPU = %u\n", get_cur_thread()->current_cpu);
        printk("\tTHREAD_IS_IDLE = %u\n", get_cur_thread()->is_idle);
        printk("\tTHREAD_FUNC = %p\n", get_cur_thread()->fun);
        printk("\tTHREAD_VIRTUAL_CONSOLE = %p\n", get_cur_thread()->vc);
      }
    } 

    printk("\tELR = %p\n", info->elr);
    printk("\tESR = %p\n", info->esr.raw);
    printk("\tFAR = %p\n", (void*)info->far);
    printk("\t\tCLS = 0x%x\n", info->esr.syndrome);
    printk("\t\tISS = 0x%x\n", info->esr.iss);
    printk("\t\tISS_2 = 0x%x\n", info->esr.iss2);

    if(info->esr.inst_length){
        printk("\t32bit Instruction\n");
    } else {
        printk("\t16bit Instruction\n");
    }

    arch_print_regs(regs);

    panic("EXCEPTION\n");
    return 0;
}

void route_exception(struct nk_regs *regs, struct excp_info *info, uint8_t el, uint8_t sync) {
  unhandled_excp_handler(regs, info, el, sync, NULL);
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

int arm64_get_current_el(void) {
  uint64_t current_el;
  asm volatile ("mrs %0, CurrentEL" : "=r" (current_el));
  return (current_el >> 2) & 3;
}

