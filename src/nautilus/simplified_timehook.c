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
#include <dev/sifive_gpio.h>

#define TIMES_MAX 10000

static int inited = 0;

static uint64_t hit_count = 0;
static uint64_t miss_count = 0;

typedef struct time_data {
	uint64_t times[TIMES_MAX];
	uint64_t count;
} time_data_t;

static time_data_t *plic_claim_no_int_times = NULL;
static time_data_t *plic_claim_int_times = NULL;
static time_data_t *call_handler_times = NULL;
static time_data_t *plic_complete_times = NULL;

void add_time(time_data_t *times, uint64_t new_time) {
  if (times->count < TIMES_MAX) {
    times->times[times->count] = new_time;
    times->count++;
  }
}

uint64_t get_mean(time_data_t *times) {
  uint64_t total = 0;
  for (uint64_t i = 0; i < times->count; i++) {
    total += times->times[i];
  }

  if (times->count == 0) {
    return 0;
  }
  return total / times->count;
}

__attribute__((annotate("nohook"))) void nk_time_hook_fire()
{
  if (!inited) {
    return;
  }

  uint64_t t1 = read_csr(cycle);
  int irq = plic_claim();
  uint64_t t2 = read_csr(cycle);
  uint64_t plic_claim_diff = t2 - t1;

  if (irq) {
    hit_count++;
    add_time(plic_claim_int_times, plic_claim_diff);

    t1 = read_csr(cycle);
    ulong_t irq_handler = 0;
    riscv_idt_get_entry(irq, &irq_handler);
    ((int (*)())irq_handler)(irq);
    t2 = read_csr(cycle);
    add_time(call_handler_times, t2 - t1);

    t1 = read_csr(cycle);
    plic_complete(irq);
    t2 = read_csr(cycle);
    add_time(plic_complete_times, t2 - t1);
  } else {
    miss_count++;
    add_time(plic_claim_no_int_times, plic_claim_diff);
  }
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
  plic_claim_no_int_times = malloc(sizeof(time_data_t));
  plic_claim_no_int_times->count = 0;

  plic_claim_int_times = malloc(sizeof(time_data_t));
  plic_claim_int_times->count = 0;

  call_handler_times = malloc(sizeof(time_data_t));
  call_handler_times->count = 0;

  plic_complete_times = malloc(sizeof(time_data_t));
  plic_complete_times->count = 0;

  inited = 1;
  return 0;
}

__attribute__((annotate("nohook"))) int nk_time_hook_stop()
{
  inited = 0;
  return 0;
}

void nk_time_hook_dump() {
  // get the means of the collected times, and print the counts of those times too

  printk("Hit count: %ld, Miss count: %ld\n", hit_count, miss_count);

  uint64_t m1 = get_mean(plic_claim_no_int_times);
  uint64_t m2 = get_mean(plic_claim_int_times);
  uint64_t m3 = get_mean(call_handler_times);
  uint64_t m4 = get_mean(plic_complete_times);

  printk("Means - plic claim no ints: %ld, plic claim ints: %ld, handler: %ld, plic complete: %ld\n", m1, m2, m3, m4);
}

