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
#ifndef __SMP_H__
#define __SMP_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <nautilus/naut_types.h>

/******* EXTERNAL INTERFACE *********/

uint32_t nk_get_num_cpus (void);

/******* !EXTERNAL INTERFACE! *********/

#include <nautilus/spinlock.h>
#include <nautilus/mm.h>
#include <nautilus/queue.h>

#ifdef NAUT_CONFIG_ARCH_X86

#include <dev/apic.h>

#ifdef NAUT_CONFIG_USE_IST
#include <arch/x86/gdt.h>
#endif

#endif

struct naut_info;
struct nk_topo_params;
struct nk_cpu_coords;

struct nk_queue;
struct nk_thread;

//typedef struct nk_wait_queue nk_wait_queue_t;
//typedef struct nk_thread nk_thread_t;
typedef void (*nk_xcall_func_t)(void * arg);

struct nk_xcall {
    nk_queue_entry_t xcall_node;
    void * data;
    nk_xcall_func_t fun;
    uint8_t xcall_done;
    uint8_t has_waiter;
};


#ifdef NAUT_CONFIG_PROFILE
    struct nk_instr_data;
#endif

struct cpu {
    struct nk_thread * cur_thread;             /* +0  KCH: this must be first! */

#if defined(NAUT_CONFIG_ARCH_RISCV) || defined(NAUT_CONFIG_ARCH_ARM64)
    uint32_t interrupt_nesting_level; /* +8 PAD: DO NOT MOVE */
    uint32_t preempt_disable_level; /* +12 PAD: DO NOT MOVE */
#else
    // track whether we are in an interrupt, nested or otherwise
    // this is intended for use by the scheduler (any scheduler)
    uint16_t interrupt_nesting_level;          /* +8  PAD: DO NOT MOVE */
    // track whether the scheduler (any scheduler) should be able to preempt
    // the current thread (whether cooperatively or via any
    uint16_t preempt_disable_level;            /* +10 PAD: DO NOT MOVE */
#endif

    // Track statistics of interrupts and exceptions
    // these counts are updated by the low-level interrupt handling code
    uint64_t interrupt_count;                  /* +16 PAD: DO NOT MOVE */
    uint64_t exception_count;                  /* +24 PAD: DO NOT MOVE */

    struct nk_irq_dev *irq_dev;

    // this field is only used if aspace are enabled
    struct nk_aspace    *cur_aspace;            /* +32 PAD: DO NOT MOVE */

#if NAUT_CONFIG_FIBER_ENABLE
    struct nk_fiber_percpu_state *f_state; /* Fiber state for each CPU */
#endif

#ifdef NAUT_CONFIG_WATCHDOG
    uint64_t watchdog_count; /* Number of times the watchdog timer has been triggered */
#endif

    cpu_id_t id;

#ifdef NAUT_CONFIG_ARCH_RISCV
    uint32_t enabled;
    uint32_t is_bsp;
#else
    uint8_t enabled;
    uint8_t is_bsp;
#endif

    uint32_t cpu_sig;
    uint32_t feat_flags;

    volatile uint8_t booted;

    int in_timer_interrupt;
    int in_kick_interrupt;


    struct sys_info * system;

    spinlock_t lock;

    struct nk_sched_percpu_state *sched_state;

    nk_queue_t * xcall_q;
    struct nk_xcall xcall_nowait_info;

    ulong_t cpu_khz; 
    
    /* NUMA info */
    struct nk_topo_params * tp;
    struct nk_cpu_coords * coord;
    struct numa_domain * domain;

    struct kmem_data kmem;


    struct nk_rand_info * rand;

#ifdef NAUT_CONFIG_ARCH_X86

    struct apic_dev * apic;
    uint32_t lapic_id;   

#ifdef NAUT_CONFIG_USE_IST
    /* CPU-specific GDT, for separate IST per-CPU */
    // Set up in smp_ap_setup
    struct gdt_desc64 gdtr64;
    uint64_t gdt64_entries[5];
    struct tss64 tss;
#endif

#endif

    /* temporary */
#ifdef NAUT_CONFIG_PROFILE
    struct nk_instr_data * instr_data;
#endif
};

struct nk_irq_action;
struct nk_regs;
int xcall_handler(struct nk_irq_action *, struct nk_regs *, void *);
int smp_xcall(cpu_id_t cpu_id, nk_xcall_func_t fun, void * arg, uint8_t wait);

#ifdef __cplusplus
}
#endif

#endif
