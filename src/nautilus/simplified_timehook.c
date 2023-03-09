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
 * Copyright (c) 2019, Peter Dinda
 * Copyright (c) 2019, Souradip Ghosh
 * Copyright (c) 2019, The Interweaving Project <https://interweaving.org>
 *                     The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Peter Dinda <pdinda@northwestern.edu>
 *          Souradip Ghosh <sgh@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#include <nautilus/nautilus.h>
#include <nautilus/cpu.h>
#include <nautilus/cpu_state.h>
#include <nautilus/naut_assert.h>
#include <nautilus/percpu.h>
#include <nautilus/list.h>
#include <nautilus/atomic.h>
#include <nautilus/timehook.h>
#include <nautilus/spinlock.h>
#include <nautilus/shell.h>
#include <nautilus/backtrace.h>
#include <arch/riscv/riscv_idt.h>
#include <arch/riscv/plic.h>

volatile int inited = 0;

__attribute__((noinline, annotate("nohook"))) void nk_time_hook_fire()
{
  if (inited) {
    inited = 0;
    int irq = plic_claim();
    // printk("I'm a timehook whoopee!\n");
    while (irq) {
      // printk("got irq: %d\n", irq);
      riscv_handle_irq(irq);
      plic_complete(irq);

      irq = plic_claim();
    }
    inited = 1;
  }
  return;
}

// Called fairly early in boot. Interrupts are not enabled and a few devices have not been initialized (including the apic). Many devices have though
// This is mostly a relic of the orignal timehook code and doesn't actually do anything important
int nk_time_hook_init()
{
    return 0;
}

// This is pretty much the last thing called during boot. Interrupts are on at this point
// Maybe we can change this though...maybe interrupts that we don't want on should never turn on
__attribute__((annotate("nohook"))) int nk_time_hook_start()
{
  inited = 1;
  return 0;
}

__attribute__((annotate("nohook"))) int nk_time_hook_stop()
{
  inited = 0;
  return 0;
}

