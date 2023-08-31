/* 
 * This file is part of the Nautilus AeroKernel developed
 * by the Hobbes and V3VEE Projects with funding from the 
 * United States National  Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  The Hobbes Project is a collaboration
 * led by Sandia National Laboratories that includes several national 
 * laboratories and universities. You can find out more at:
 * http://www.v3vee.org  and
 * http://xstack.sandia.gov/hobbes
 *
 * Copyright (c) 2015, Kyle C. Hale <kh@u.northwestern.edu>
 * Copyright (c) 2015, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Kyle C. Hale <kh@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#include <arch/x64/idt.h>

#include <nautilus/nautilus.h>
#include <nautilus/intrinsics.h>
#include <nautilus/naut_string.h>
#include <nautilus/paging.h>
#include <nautilus/percpu.h>
#include <nautilus/cpu.h>
#include <nautilus/thread.h>
#include <nautilus/irq.h>
#include <nautilus/backtrace.h>
#ifdef NAUT_CONFIG_WATCHDOG
#include <nautilus/watchdog.h>
#endif
#ifdef NAUT_CONFIG_ENABLE_MONITOR
#include <nautilus/monitor.h>
#endif
#include<nautilus/interrupt.h>

struct gate_desc64 idt64[NUM_IDT_ENTRIES] __align(8);

extern uint8_t cpu_info_ready;

#define EXCP_NAME 0
#define EXCP_MNEMONIC 1
const char * excp_codes[NUM_EXCEPTIONS][2] = {
    {"Divide By Zero",           "#DE"},
    {"Debug",                    "#DB"},
    {"Non-maskable Interrupt",   "N/A"},
    {"Breakpoint Exception",     "#BP"},
    {"Overflow Exception",       "#OF"},
    {"Bound Range Exceeded",     "#BR"},
    {"Invalid Opcode",           "#UD"},
    {"Device Not Available",     "#NM"},
    {"Double Fault",             "#DF"},
    {"Coproc Segment Overrun",   "N/A"},
    {"Invalid TSS",              "#TS"},
    {"Segment Not Present",      "#NP"},
    {"Stack Segment Fault",      "#SS"},
    {"General Protection Fault", "#GP"},
    {"Page Fault",               "#PF"},
    {"Reserved",                 "N/A"},
    {"x86 FP Exception",         "#MF"},
    {"Alignment Check",          "#AC"},
    {"Machine Check",            "#MC"},
    {"SIMD FP Exception",        "#XM"},
    {"Virtualization Exception", "#VE"},
    {"Reserved",                 "N/A"},
    {"Reserved",                 "N/A"},
    {"Reserved",                 "N/A"},
    {"Reserved",                 "N/A"},
    {"Reserved",                 "N/A"},
    {"Reserved",                 "N/A"},
    {"Reserved",                 "N/A"},
    {"Reserved",                 "N/A"},
    {"Reserved",                 "N/A"},
    {"Security Exception",       "#SX"},
    {"Reserved",                 "N/A"},
};


struct idt_desc idt_descriptor =
{
    .base_addr = (uint64_t)&idt64,
    .size      = (NUM_IDT_ENTRIES*16)-1,
};




int 
null_excp_handler (
    struct nk_irq_action *action,
    struct nk_regs *regs,
    void *state)
{
#ifdef NAUT_CONFIG_ENABLE_MONITOR
    return nk_monitor_excp_entry(action_regs);
#else
    
    cpu_id_t cpu_id = cpu_info_ready ? my_cpu_id() : 0xffffffff;
    /* TODO: this should be based on scheduler initialization, not CPU */
    unsigned tid = cpu_info_ready ? get_cur_thread()->tid : 0xffffffff;
    
    printk("\n+++ UNHANDLED EXCEPTION +++\n");
    
    if (action->ivec < 32) {
        printk("[%s] (0x%x) error=0x%x <%s>\n    RIP=%p      (core=%u, thread=%u)\n", 
	       excp_codes[action->ivec][EXCP_NAME],
                action->ivec,
	       regs->error_code,
	       excp_codes[action->ivec][EXCP_MNEMONIC],
	       (void*)regs->rip, 
	       cpu_id, tid);
    } else {
        printk("[Unknown Exception] (vector=0x%x)\n    RIP=(%p)     (core=%u)\n", 
	       action->ivec,
	       (void*)regs->rip,
	       cpu_id);
    }
    
#ifdef NAUT_CONFIG_ARCH_X86
    //struct nk_regs * r = (struct nk_regs*)((char*)excp - 128);
    arch_print_regs(regs);
    backtrace(regs->rbp);
#endif

    panic("+++ HALTING +++\n");
    
    return 0;
#endif
    
}


int
null_irq_handler (struct nk_irq_action *action,
                  struct nk_regs *regs,
		  void       *state)
{
#ifdef NAUT_CONFIG_ENABLE_MONITOR
    return nk_monitor_irq_entry (action,
				 regs,
				 state);
#else
    
    printk("[Unhandled IRQ] (vector=0x%x)\n    RIP=(%p)     (core=%u)\n", 
            action->ivec,
            (void*)regs->rip,
            my_cpu_id());

#ifdef NAUT_CONFIG_ARCH_X86
    nk_print_regs(regs);
    backtrace(regs->rbp);
#endif

    panic("+++ HALTING +++\n");
    
    return 0;
#endif
}

int 
debug_excp_handler (struct nk_irq_action *action,
                   struct nk_regs *regs,
		   void *state)
{
#ifdef NAUT_CONFIG_ENABLE_MONITOR
    return nk_monitor_debug_entry (action,
				   regs,
				   state);
#else 
    cpu_id_t cpu_id = cpu_info_ready ? my_cpu_id() : 0xffffffff;
    /* TODO: this should be based on scheduler initialization, not CPU */
    unsigned tid = cpu_info_ready ? get_cur_thread()->tid : 0xffffffff;

    printk("\n+++ UNHANDLED DEBUG EXCEPTION +++\n");

    printk("[%s] (0x%x) error=0x%x <%s>\n    RIP=%p      (core=%u, thread=%u)\n", 
	   excp_codes[action->ivec][EXCP_NAME],
	   action->ivec,
	   regs->error_code,
	   excp_codes[action->ivec][EXCP_MNEMONIC],
	   (void*)regs->rip, 
	   cpu_id, tid);
    
    nk_print_regs(regs);
    backtrace(regs->rbp);
    
    panic("+++ HALTING +++\n");
    
    return 0;
#endif
}



int
reserved_irq_handler (struct nk_irq_action * action,
		      struct nk_regs *regs,
		      void       *state)
{
  printk("[Reserved IRQ] (vector=0x%x)\n    RIP=(%p)     (core=%u)\n", 
	 action->ivec,
	 (void*)regs->rip,
	 my_cpu_id());
  printk("You probably have a race between reservation and assignment....\n");
  printk("   you probably want to mask the interrupt first...\n");

  return 0;
}


static int
df_handler (struct nk_irq_action * action,
            struct nk_regs *regs,
            addr_t unused)
{
    panic("DOUBLE FAULT. Dying.\n");
    return 0;
}


static int
pic_spur_int_handler (struct nk_irq_action * action,
                      struct nk_regs *regs,
                      addr_t unused)
{
    WARN_PRINT("Received Spurious interrupt from PIC\n");
    // No EOI should be done for a spurious interrupt
    return 0;
}




/*

  NMIs are currently used for two purposes, the watchdog
  timer and the monitor.   

  When the monitor is entered on any CPU, the monitor
  NMIs all other CPUs to force them into the monitor as well,
  regardless of their current state.

  When the underlying watchdog timer fires, it NMIs all CPUs.

  As a consequence, NMIs can come from three possible places: monitor,
  watchdog timer and other NMI-triggering event on the machine.
  Disambiguating these cases is a challenge.

*/

int nmi_handler (struct nk_irq_action *action,
		 struct nk_regs *regs,
		 void *state)
{

#if defined(NAUT_CONFIG_WATCHDOG) && !defined(NAUT_CONFIG_ENABLE_MONITOR)
    int barking = 0;
    
    nk_watchdog_nmi();
    
    if (!nk_watchdog_check_any_cpu()) {
	return 0;
    } else {
	barking = 1;
	goto bad;
    }
    
#endif
    
#if defined(NAUT_CONFIG_WATCHDOG) && defined(NAUT_CONFIG_ENABLE_MONITOR)


    int cpu;
    
    if (nk_monitor_check(&cpu)) {
	// we are in the monitor on some cpu
	// so this NMI could be coming from the monitor
	// or it could be coming from the watchdog
	//   with us either in, or out of, the monitor
	if (my_cpu_id()!=cpu) { 
	    int rc = nk_monitor_sync_entry();
	    nk_watchdog_reset();
	    nk_watchdog_pet();
	    return rc;
	} else {
	    // we caused the entry and we are now seeing
	    // a spurious NMI because the timer is racing with
	    // the monitor
	    return 0;
	}
    } else {
	// it must be coming from the timer or
	// from some unknown external force
	nk_watchdog_nmi();
	if (nk_watchdog_check_this_cpu()) {
	    // we are hanging on this cpu...
	    int rc = nk_monitor_hang_entry(action,regs,state);
	    nk_watchdog_reset();
	    nk_watchdog_pet();
	    return rc;
	} else {
	    return 0;
	}
    }

    
#endif
    
#if !defined(NAUT_CONFIG_WATCHDOG) && defined(NAUT_CONFIG_ENABLE_MONITOR)

    int cpu;
    
    if (nk_monitor_check(&cpu)) {
	// we are in the monitor on some cpu
	// so this NMI must be coming from the monitor
	return nk_monitor_sync_entry();
    } else {
	// surprise NMI... 
	return nk_monitor_excp_entry(action, regs, state);
    }

#endif

    printk("\n+++ NON MASKABLE INTERRUPT +++\n");

#if defined(NAUT_CONFIG_WATCHDOG) && !defined(NAUT_CONFIG_ENABLE_MONITOR)
 bad:
    printk("\n+++ WATCHDOG BARKING (NMI) +++\n");
#endif

    cpu_id_t cpu_id = cpu_info_ready ? my_cpu_id() : 0xffffffff;
    unsigned tid = cpu_info_ready ? get_cur_thread()->tid : 0xffffffff;

    printk("[%s] (0x%x) error=0x%x <%s>\n    RIP=%p      (core=%u, thread=%u)\n", 
	   excp_codes[action->ivec][EXCP_NAME],
	   action->ivec,
	   regs->error_code,
	   excp_codes[action->ivec][EXCP_MNEMONIC],
	   (void*)regs->rip, 
	   cpu_id, tid);

#ifdef NAUT_CONFIG_ARCH_X86
    nk_print_regs(regs);
    backtrace(regs->rbp);
#endif

    panic("+++ HALTING +++\n");

    return 0;
}

int route_interrupt_vector(struct nk_regs *regs, nk_ivec_t ivec)
{
  struct nk_ivec_desc *desc = nk_ivec_to_desc(ivec);
  if(desc == NULL) {
    return ivec;
  }
  int ret = nk_handle_irq_actions(desc, regs); 
  if(ret) {
    return ivec;
  } else {
    return 0;
  }
}
/*
int idt_find_and_reserve_range(ulong_t numentries, int aligned, ulong_t *first)
{
  ulong_t h, s;
  int i,j;

  if (numentries>32) { 
    return -1;
  }

  for (i=32;i<(NUM_IDT_ENTRIES-numentries+1);) {

    if (idt_handler_table[i]==(ulong_t)null_irq_handler) { 
      for (j=0; 
	   (i+j)<NUM_IDT_ENTRIES && 
	     j<numentries &&
	     idt_handler_table[i+j]==(ulong_t)null_irq_handler;
	   j++) {
      }

      if (j==numentries) { 
	// found it!
	for (j=0;j<numentries;j++) { 
	  idt_handler_table[i+j]=(ulong_t)reserved_irq_handler;
	}
	*first=i;
	return 0;
      } else {
	if (aligned) { 
	  i=i+numentries;
	} else {
	  i=i+j;
	}
      }
    } else {
      i++;
    }
  }

  return -1;
}
*/

#ifdef NAUT_CONFIG_ARCH_X86
extern void early_irq_handlers(void);
extern void early_excp_handlers(void);

int
setup_idt (void)
{
    uint_t i;

    ulong_t irq_start = (ulong_t)&early_irq_handlers;
    ulong_t excp_start = (ulong_t)&early_excp_handlers;

    // clear the IDT out
    memset(&idt64, 0, sizeof(struct gate_desc64) * NUM_IDT_ENTRIES);

    for (i = 0; i < NUM_EXCEPTIONS; i++) {
        set_intr_gate(idt64, i, (void*)(excp_start + i*16));
        //nk_ivec_set_handler_early(i, (ulong_t)null_excp_handler, 0);
    }

    for (i = 32; i < NUM_IDT_ENTRIES; i++) {
        set_intr_gate(idt64, i, (void*)(irq_start + (i-32)*16));
        //nk_ivec_set_handler_early(i, (ulong_t)null_irq_handler, 0);
    }

    if (nk_ivec_set_handler_early(PF_EXCP, nk_pf_handler, 0) < 0) {
        ERROR_PRINT("Couldn't assign page fault handler\n");
        return -1;
    }

    if (nk_ivec_set_handler_early(GP_EXCP, nk_gpf_handler, 0) < 0) {
        ERROR_PRINT("Couldn't assign general protection fault handler\n");
        return -1;
    }

    if (nk_ivec_set_handler_early(DF_EXCP, df_handler, 0) < 0) {
        ERROR_PRINT("Couldn't assign double fault handler\n");
        return -1;
    }

    if (nk_ivec_set_handler_early(0xf, pic_spur_int_handler, 0) < 0) {
        ERROR_PRINT("Couldn't assign PIC spur int handler\n");
        return -1;
    }

#ifdef NAUT_CONFIG_ENABLE_MONITOR
    if (nk_ivec_set_handler_early(DB_EXCP, debug_excp_handler, 0) < 0) {
        ERROR_PRINT("Couldn't assign debug excp handler\n");
        return -1;
    }
#endif

#if defined(NAUT_CONFIG_ENABLE_MONITOR) || defined(NAUT_CONFIG_WATCHDOG)
    if (nk_ivec_set_handler_early(NMI_INT, nmi_handler, 0) < 0) {
        ERROR_PRINT("Couldn't assign NMI handler\n");
        return -1;
    }
#endif



    lidt(&idt_descriptor);

    return 0;
}
#endif
