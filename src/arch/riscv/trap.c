#include <nautilus/arch.h>
#include <nautilus/printk.h>
#include <arch/riscv/plic.h>
#include <arch/riscv/trap.h>
#include <arch/riscv/riscv_idt.h>

// static volatile int x = 0;

struct trap_regs {
  uint64_t sepc;
  uint64_t status;
  uint64_t tval;
  uint64_t cause;
  uint64_t sscratch;
  uint64_t badaddr;
};

void kernel_vec();

// set up to take exceptions and traps while in the kernel.
void trap_init(void) { write_csr(stvec,(uint64_t)kernel_vec); }

static void print_regs(struct nk_regs *r) {
  int i = 0;

  if(r_tp() == NULL) {
    printk("Occurred before percpu system is active!\n");
  }
  else {
    printk("HART = %u\n", my_cpu_id());
    printk("Current Thread=0x%x (%p) \"%s\"\n",
      get_cur_thread() ? get_cur_thread()->tid : -1,
      get_cur_thread() ? (void*)get_cur_thread() :  NULL,
      !get_cur_thread() ? "NONE" : get_cur_thread()->is_idle ? "*idle*" : get_cur_thread()->name);
  }

  //printk("[-------------- Register Contents --------------]\n");
  arch_print_regs(r);
  /*
  printk("RA:  %016lx SP:  %016lx\n", r->ra, r->sp);
  printk("GP:  %016lx TP:  %016lx\n", r->gp, r->tp);
  printk("T00: %016lx T01: %016lx T02: %016lx\n", r->t0, r->t1, r->t2);
  printk("S00: %016lx S01: %016lx A00: %016lx\n", r->s0, r->s1, r->a0);
  printk("A01: %016lx A02: %016lx A03: %016lx\n", r->a1, r->a2, r->a3);
  printk("A04: %016lx A05: %016lx A06: %016lx\n", r->a4, r->a5, r->a6);
  printk("A07: %016lx S02: %016lx S03: %016lx\n", r->a7, r->s2, r->s3);
  printk("S04: %016lx S05: %016lx S06: %016lx\n", r->s4, r->s5, r->s6);
  printk("S07: %016lx S08: %016lx S09: %016lx\n", r->s7, r->s8, r->s9);
  printk("S10: %016lx S11: %016lx T03: %016lx\n", r->s10, r->s11, r->t3);
  printk("T04: %016lx T05: %016lx T06: %016lx\n", r->t4, r->t5, r->t6);
  */

  //printk("[-----------------------------------------------]\n");
}

static void kernel_unhandled_trap(struct nk_regs *regs, struct trap_regs *tregs, const char *type) 
{
  printk("===========================================================================================\n");
  printk("+++ Unhandled Trap: %s +++\n", type);
  printk("SEPC:   %016lx CAUSE:   %016lx TVAL: %016lx\n", tregs->sepc, tregs->cause, tregs->tval);
  printk("STATUS: %016lx SCRATCH: %016lx BADADDR: %016lx\n", tregs->status, tregs->sscratch, tregs->badaddr);
  print_regs(regs);
  printk("===========================================================================================\n");
  panic("Halting hart!\n");
}

static struct nk_irq_dev *riscv_root_irq_dev = NULL;
int riscv_set_root_irq_dev(struct nk_irq_dev *dev) {
  if(riscv_root_irq_dev == NULL) {
    riscv_root_irq_dev = dev;
    return 0;
  } else {
    return -1;
  }
}

/* Supervisor Trap Function */
void * kernel_trap(struct nk_regs *regs, struct trap_regs *tregs) {

  // if it was an interrupt, the top bit it set.
  int interrupt = (tregs->cause >> 63);
  // the type of trap is the rest of the bits.
  int nr = tregs->cause & ~(1llu << 63);

  if (interrupt) 
  {
    //printk("int!, nr = %d, sie=%p, pending=%p\n", nr, read_csr(sie), read_csr(sip)); 

    // supervisor external interrupt
    // first, we claim the irq from the PLIC
    struct nk_irq_dev *irq_dev = riscv_root_irq_dev;
    if(nk_handle_interrupt_generic(NULL, regs, irq_dev)) {
      // Nothing we can do
    }

  } else {
    // it's a fault/trap (like illegal instruction)
    switch (nr) {
      case 0:
        kernel_unhandled_trap(regs, tregs, "Instruction address misaligned");
        break;
      case 1:
        kernel_unhandled_trap(regs, tregs, "Instruction access fault");
        break;
      case 2:
        kernel_unhandled_trap(regs, tregs, "Illegal instruction");
        break;
      case 3:
        kernel_unhandled_trap(regs, tregs, "Breakpoint");
        break;
      case 4:
        kernel_unhandled_trap(regs, tregs, "Load address misaligned");
        break;
      case 5:
        kernel_unhandled_trap(regs, tregs, "Load access fault");
        break;
      case 6:
        kernel_unhandled_trap(regs, tregs, "Store/AMO address misaligned");
        break;
      case 7:
        kernel_unhandled_trap(regs, tregs, "Store/AMO access fault");
        break;
      case 8:
        kernel_unhandled_trap(regs, tregs, "Environment call from U-mode");
        break;
      case 9:
        kernel_unhandled_trap(regs, tregs, "Environment call from S-mode");
        break;
      case 12:
        kernel_unhandled_trap(regs, tregs, "Instruction page fault");
        break;
      case 13:
        kernel_unhandled_trap(regs, tregs, "Load page fault");
        break;
      case 15:
        kernel_unhandled_trap(regs, tregs, "Store/AMO page fault");
        break;
      default:
        kernel_unhandled_trap(regs, tregs, "Reserved");
        break;
    }
  }

  // restore trap registers for use by kernelvec.S's sepc instruction.
  write_csr(sepc, tregs->sepc);
  write_csr(sstatus, tregs->status);

  void *thread = NULL;
  if(per_cpu_get(in_timer_interrupt)) {
    thread = nk_sched_need_resched();
  }

  return thread;
}
