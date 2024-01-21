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
#include <nautilus/interrupt.h>
#include <dev/sifive_gpio.h>

#define TIMES_COUNT 10000

static int inited = 0;

volatile int init_counter = 0;

static uint64_t times[TIMES_COUNT];
static uint64_t counter = 0;

extern void poll_irq();
extern void nk_thread_switch(struct nk_thread *thread);

__attribute__((annotate("nohook"))) void poll_handle_irq(int irq)
{
#ifdef NAUT_CONFIG_ARCH_RISCV
  ulong_t irq_handler = 0;
  riscv_idt_get_entry(irq, &irq_handler);
  ((int (*)())irq_handler)(irq);
  plic_complete(irq);
#endif
}

#ifdef NAUT_CONFIG_ARCH_ARM64
int in_time_hook = 1;
#endif

__attribute__((annotate("nohook"))) void nk_time_hook_fire()
{
#ifdef NAUT_CONFIG_ARCH_RISCV
  poll_irq();
#endif
#ifdef NAUT_CONFIG_ARCH_ARM64
#ifndef NAUT_CONFIG_GIC_VERSION_3
#error "ARM64 BEANDIP Requires GICv3!"
#endif
  if(atomic_lock_test_and_set(in_time_hook,1)) {
    return;
  }
  //printk("Timehook Fire!\n");

  // KJH - This is very hardcoded to the GICv3
  uint64_t hppir;
  LOAD_SYS_REG(ICC_HPPIR0_EL1, hppir);
  if(hppir >= 1020 && hppir < 1024) {
    LOAD_SYS_REG(ICC_HPPIR1_EL1, hppir);
  }
  if(hppir < 1020 || hppir > 1023) {
    // an interrupt!
    extern struct nk_irq_dev *arm64_root_irq_dev;
    if(nk_handle_interrupt_generic(
        NULL, // action
        NULL, // regs
        arm64_root_irq_dev)) 
    {
      panic("Could not handle interrupt!\n");
    }
  }

  if(per_cpu_get(in_timer_interrupt)) {
    void *thread = nk_sched_need_resched();
    atomic_lock_release(in_time_hook);
    if(thread != NULL) {
      nk_thread_switch(thread);
    }
  } else {
    atomic_lock_release(in_time_hook);
  }

#endif

  //counter += 1;
  // int irq = plic_claim();
  // if (irq) {
  //   ulong_t irq_handler = 0;
  //   riscv_idt_get_entry(irq, &irq_handler);
  //   ((int (*)())irq_handler)(irq);
  //   plic_complete(irq);
  // }


  // printk("I'm a timehook whoopee!\n");
  // while (irq) {
  //   // printk("got irq: %d\n", irq);
  //   riscv_handle_irq(irq);
  //   plic_complete(irq);

  //   irq = plic_claim();
  // }
  // if (inited) {
  //   inited = 0;

  //   // init_counter++;

  //   // if (init_counter > 100000) {
  //   //   uint64_t t = read_csr(cycle);

  //   //   if (counter < TIMES_COUNT) {
  //   //     times[counter] = t;
  //   //   } else if (counter == TIMES_COUNT) {
  //   //     uint64_t total = 0;
  //   //     for (int i = 0; i < TIMES_COUNT; i++) {
  //   //       if (i != 0) {
  //   //         uint64_t diff = times[i] - times[i - 1];
  //   //         total += diff;
  //   //         // printk("%ld\n", diff);
  //   //       }
  //   //     }

  //   //     uint64_t avg = total / (TIMES_COUNT - 1);
  //   //     printk("Avg: %d\n", avg);
  //   //   }

  //   //   counter++;
  //   // }

  //   int irq = plic_claim();
  //   // printk("I'm a timehook whoopee!\n");
  //   while (irq) {
  //     // printk("got irq: %d\n", irq);
  //     riscv_handle_irq(irq);
  //     plic_complete(irq);

  //     irq = plic_claim();
  //   }


  //   inited = 1;
  // }
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

