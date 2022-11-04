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

#include <dev/apic.h>

struct apic_dev * apic;

/*
  This is the run-time support code for compiler-based timing transforms
  and is meaningless without that feature enabled.
*/


/* Note that since code here can be called in interrupt context, it
   is potentially dangerous to turn on debugging or other output */

#ifndef NAUT_CONFIG_DEBUG_COMPILER_TIMING
#undef  DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

#define INFO(fmt, args...) INFO_PRINT("timehook: " fmt, ##args)
#define ERROR(fmt, args...) ERROR_PRINT("timehook: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("timehook: " fmt, ##args)
#define WARN(fmt, args...)  WARN_PRINT("timehook: " fmt, ##args)

// maximum number of hooks per CPU
#define MAX_HOOKS  16
#define CACHE_LINE 64

//
// Locking is done using a per-cpu lock, but the user must be explicit
// 
#define LOCAL_LOCK_DECL uint8_t __local_lock_flags
#define LOCAL_LOCK(cpu) __local_lock_flags = spin_lock_irq_save(&cms[cpu].lock)
#define LOCAL_TRYLOCK(cpu) spin_try_lock_irq_save(&cms[cpu].lock, &__local_lock_flags)
#define LOCAL_UNLOCK(cpu) spin_unlock_irq_restore(&cms[cpu].lock, __local_lock_flags)
#define LOCAL_LOCK_NO_IRQ(cpu) spin_lock(&cms[cpu].lock)
#define LOCAL_TRYLOCK_NO_IRQ(cpu) spin_try_lock(&cms[cpu].lock)
#define LOCAL_UNLOCK_NO_IRQ(cpu) spin_unlock(&cms[cpu].lock)


//
// Low-level debugging output to QEMU debug port
//
#define DB(x) outb(x, 0xe9)
#define DHN(x) outb(((x & 0xF) >= 10) ? (((x & 0xF) - 10) + 'a') : ((x & 0xF) + '0'), 0xe9)
#define DHB(x) DHN(x >> 4) ; DHN(x);
#define DHW(x) DHB(x >> 8) ; DHB(x);
#define DHL(x) DHW(x >> 16) ; DHW(x);
#define DHQ(x) DHL(x >> 32) ; DHL(x);
#define DS(x) { char *__curr = x; while(*__curr) { DB(*__curr); *__curr++; } }

#define MAX(x, y)((x > y) ? (x) : (y))
#define MIN(x, y)((x < y) ? (x) : (y))


// ready is set once the time hook framework is functional
// on all cpus.  Before that, compiler-injected calls to
// time hook fire must be ignored.
static int ready = 0;

static spinlock_t lock = 1;
static int numInvocations = 0;
static void* last_handler_called = 0;

static uint64_t timing_start = 0;
static uint64_t timing_end = -1;
static uint64_t timing_sum = 0;
static uint64_t timing_num = 0;


#define LAPIC_ID_REGISTER 0xFEE00020
#define APIC_READ_ID_MEM (*((volatile uint32_t*)(LAPIC_ID_REGISTER)))

// #define likely(x)      __builtin_expect(!!(x), 1)
// #define unlikely(x)    __builtin_expect(!!(x), 0)

__attribute((noinline, annotate("nohook"))) void handle_this_irr_interrupts(uint32_t this_irr, uint8_t which_irr) {
  ++numInvocations;
  // Figure out which interrupt handlers to fire based on set bits
  while (this_irr) {
    // Get number of trailing 0's, aka the index of the rightmost set bit
    uint8_t bit_pos = __builtin_ctz(this_irr);

    // scale bit position based on which irr this is
    uint8_t interrupt_vector_index = bit_pos + 32*which_irr;

    // Unset the rightmost set bit
    this_irr &= this_irr - 1;

    // Handle the interrupt
    // Invoke a function pointer stored in a global array maybe?
    // This global array would contain pointers to the handler functions for every interrupt,
    // indexed by interrupt vector
    
    int (*dp_handler)() = (int (*)())idt_get_dp_table_entry(interrupt_vector_index);
    if (dp_handler) {
      dp_handler();
      last_handler_called = dp_handler;
      // Interrupt is handled
      // Enable hardware interrupt
      // DANGER DANGER Potential race condition here where we read IER, an interrupt we previously enabled here writes the IER, then we overwrite the IER here
      uint32_t this_ier = apic_read(apic, APIC_GET_IER(which_irr)); // could avoid this apic_read if we keep an internal record of what ier should look like
      apic_write(apic, APIC_GET_IER(which_irr), this_ier | (1 << bit_pos));

      // Hardware interrupt will write back to ier to disable itself and issue seoi
      // Process is complete for this interrupt, continue to check this irr until no interrupts are left
    }

    
  }
}

// At the point this can fire it's assumed that the apic is configured appropriately
// The body of this should be small, it's all being inlined
__attribute__((noinline, annotate("nohook"))) void nk_time_hook_fire()
{
  // if (!ready) return;

  // if (spin_try_lock(&lock)) return;
  
  if (lock) return;

  disable_irqs();

  timing_start = rdtscp();
  lock = 1;

  // DEBUG("\ntime being hooked\n");

  /**
   * There are a couple ways we can do this.
   * 1) (current way) poll, check, and handle one register at a time
   * 2) Poll all, check all, then handle all interrupts
   * 
   * More ways I'm sure...another question is when to actually fire the null interrupts
   * Let's say we get two interrupts from IER 0 - do we handle both then enable both null interrupts?
   * Handle one, null interrupt, handle the other, null interrupt? Might be a difference in efficiency
   * There's an advantage to firing the null handler as soon as possible, as that opens up the IRR bit to new interrupts that could be missed if we wait to clear it
   * 
   */
  // handle interrupts one register at a time
  // if we do it this way, this loop should be fully unrolled to maximize interleaving of other kernel code
  for (int i = 0; i < 8; i++) {
    // Poll the apic IRR to find pending interrupts
    uint32_t this_irr = apic_read(apic, APIC_GET_IRR(i));

    // Check for set bits
    if (this_irr) {
      handle_this_irr_interrupts(this_irr, i);
    }
  }

  // spin_unlock(&lock);
  lock = 0;
  timing_end = rdtscp();

  enable_irqs();

  uint64_t this_time = timing_end - timing_start >= 0 ? timing_end - timing_start : timing_start - timing_end;

  uint64_t old_sum = timing_sum;
  uint64_t new_sum = timing_sum + this_time;
  if (new_sum < old_sum) {
    timing_sum = this_time;
    timing_num = 1;
  } else{
    timing_sum = new_sum;
    ++timing_num;
  }

  return;
}

// q&a
// How do we pass the right info? extra function to register irq/int rn
// Where do we define the dp handler? with driver code
// Where do we define the null handler? Every interrupt following our scheme should be able to use the same one
// How do we decide to register the dp handler or not? Maybe always register and just not use if not needed. Hard coded for now
// How do we decide to register the null handler instead of the real handler? hard coded for now into device init
// Does the real handler need to be enabled for proper boot? Depends on device? assuming not for now. if it is needed we will need to change interrupt registrations outside of init functions
// Can we deregister a handler to replace with null handler? Might be able to just register a new handler over it
// When do we disable interrupts with the IER? Do we disable as part of the timehook setup process? right now disabled during device init when null handler and dp handler are registered

static int
dp_null_handler(excp_entry_t * excp, excp_vec_t vec, void *state) {
  struct apic_dev* apic;
  apic = per_cpu_get(apic);
  uint8_t which_ier = vec / 32;
  uint8_t bit_pos = vec % 32;

  // Disable this interrupts from firing
  // DANGER DANGER potential race condition if another null_handler interrupts us between the read and write
  uint32_t ier = apic_read(apic, APIC_GET_IER(which_ier));
  apic_write(apic, APIC_GET_IER(which_ier), ier & ~(1 << bit_pos));

  // Do SEOI
  apic_write(apic, APIC_REG_SEOI, vec);

  return 0;
}

static int shared_init()
{

    INFO("inited\n");
    
    return 0;
    
}

// Called fairly early in boot. Interrupts are not enabled and a few devices have not been initialized (including the apic). Many devices have though
// This is mostly a relic of the orignal timehook code and doesn't actually do anything important
int nk_time_hook_init()
{
    return shared_init();
}

// Don't know where this is called
int nk_time_hook_init_ap()
{
    return shared_init();
}

static int cpu_count = 0;

// This is pretty much the last thing called during boot. Interrupts are on at this point
// Maybe we can change this though...maybe interrupts that we don't want on should never turn on
__attribute__((annotate("nohook")))int nk_time_hook_start()
{
  
  // DPCODE initialize the apic stuff
  // This will just fault if the apic doesn't have the amd extended feature set...which is okay for now
  apic = per_cpu_get(apic);
  apic_write(apic, APIC_REG_EXFC, 3);

  // We need to loop through the dp_handler_table. for each entry that has a handler:
  // 1. Disable interrupts through IER
  // 2. Set idt handler to the null handler
  for (int entry = 0; entry < 256; ++entry) {
    if (idt_get_dp_table_entry(entry)) {
      // INFO("disabling interrupts for entry: %d\n", entry);
      uint32_t ier = apic_read(apic, APIC_VEC_TO_IER(entry));
      ier = ier & ~(1 << APIC_VEC_TO_BIT(entry));
      apic_write(apic, APIC_VEC_TO_IER(entry), ier);

      idt_set_entry_to_handler(entry, (ulong_t)dp_null_handler);
    }
  }  

  ready = 1;
  lock = 0;
  INFO("time hook ready set\n");
  
  return 0;
}


// These two do nothing right now as we aren't using ready, so enabling dp from shell is not supported
static int
handle_dpe(char * buf, void * priv)
{  
  nk_vc_printf("Enabling distributed polling time hook executions\n");
  ready = 1;
}
static int
handle_dpd(char * buf, void * priv)
{  
  nk_vc_printf("Disabling distributed polling time hook executions\n");
  ready = 0;
}

static int
handle_ths(char * buf, void * priv)
{
  uint64_t avg = timing_sum / timing_num;
  // uint64_t last_measurement = timing_end - timing_start;
  nk_vc_printf("Average measure of polling overhead: %d cycles\n", avg);

  nk_vc_printf("Address of last handler called: %x\n", last_handler_called);
  nk_vc_printf("Number of times irr_handling function has been invoked: %d\n", numInvocations);
  // nk_vc_printf("Last measure of polling overhead: %d cycles\n", last_measurement);
  return 0;
}

static struct shell_cmd_impl dpe_impl = {
    .cmd      = "dpe",
	.help_str = "dpe",
	.handler  = handle_dpe,
};

static struct shell_cmd_impl dpd_impl = {
    .cmd      = "dpd",
	.help_str = "dpd",
	.handler  = handle_dpd,
};

static struct shell_cmd_impl ths_impl = {
    .cmd      = "ths",
	.help_str = "ths",
	.handler  = handle_ths,
};

nk_register_shell_cmd(dpe_impl);
nk_register_shell_cmd(dpd_impl);
nk_register_shell_cmd(ths_impl);

