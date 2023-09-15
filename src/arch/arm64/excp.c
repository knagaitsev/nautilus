
#include<nautilus/nautilus.h>
#include<nautilus/interrupt.h>
#include<nautilus/printk.h>

#include<arch/arm64/excp.h>

#define EXCP_SYNDROME_BITS 6

int unhandled_excp_handler(struct nk_regs *regs, struct excp_info *info, uint8_t el_from_same, uint8_t sync, void *state) {

    int curr_el = arm64_get_current_el();

    printk("--- UNKNOWN %s EXCEPTION %s ---\n",
           curr_el == 0 ? "USERMODE (ERROR)" :
           curr_el == 1 ? "KERNEL" :
           curr_el == 2 ? "HYPERVISOR" :
           curr_el == 3 ? "SECURE MONITOR" :
           "(ERROR)",
           el_from_same == 0 ? "FROM A LOWER EXCEPTION LEVEL" : ""); 

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

    sctlr_el1_t sctlr_el1;
    LOAD_SYS_REG(SCTLR_EL1, sctlr_el1.raw);
    dump_sctlr_el1(sctlr_el1);

    tcr_el1_t tcr_el1;
    LOAD_SYS_REG(TCR_EL1, tcr_el1.raw);
    dump_tcr_el1(tcr_el1);

    mair_el1_t mair_el1;
    LOAD_SYS_REG(MAIR_EL1, mair_el1.raw);
    printk("MAIR_EL1.raw = 0x%016x\n", mair_el1.raw);

    rockchip_halt_and_flash(1,2,0);
    
    panic("END OF EXCEPTION\n");
    return 0;
}

void route_exception(struct nk_regs *regs, struct excp_info *info, uint8_t el_from_same, uint8_t sync) {
  unhandled_excp_handler(regs, info, el_from_same, sync, NULL);
}

struct nk_irq_dev *arm64_root_irq_dev = NULL;

int arm64_set_root_irq_dev(struct nk_irq_dev *dev) {
  if(arm64_root_irq_dev == NULL) {
    arm64_root_irq_dev = dev;
    return 0;
  }
  else {
    return -1;
  }
}

void * route_interrupt(struct nk_regs *regs, struct excp_info *info, uint8_t el_from_same) 
{
  struct nk_irq_dev *irq_dev = arm64_root_irq_dev;

  if(nk_handle_interrupt_generic(NULL, regs, irq_dev)) {
    // Nothing we can really do    
  }

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

