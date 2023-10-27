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
 * http://xtack.sandia.gov/hobbes
 *
 * Copyright (c) 2017, Peter Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2017, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

/* Wrappers for preemption, interrupt level, etc, for use when */
/* struct cpu isn't available  */

#ifndef __CPU_STATE
#define __CPU_STATE

#ifdef NAUT_CONFIG_ARCH_X86
#include <arch/x64/msr.h>
#endif

//
// This code assumes that %gs base (or tp) is pointing to
// the struct cpu of the running cpu and it assumes the
// specific layout of struct cpu
//


static inline void *__cpu_state_get_cpu()
{
    // there must be a smarter way to do this....
    // leaq %gs:... does not do it though
#ifdef NAUT_CONFIG_ARCH_RISCV
    uint64_t tp;
    asm volatile("mv %0, tp" : "=r" (tp) );
    return (void *) tp;
#endif

#ifdef NAUT_CONFIG_ARCH_ARM64
    uint64_t tid;
    asm volatile("mrs %0, TPIDR_EL1" : "=r" (tid));
    return (void*)tid;
#endif

#ifdef NAUT_CONFIG_ARCH_X86
    return (void *) msr_read(MSR_GS_BASE);
#endif

		// TODO: unreachable
		return 0;
}


#define INL_OFFSET 8
#if defined(NAUT_CONFIG_ARCH_RISCV) || defined(NAUT_CONFIG_ARCH_ARM64)
#define PREEMPT_DISABLE_OFFSET 12
#else
#define PREEMPT_DISABLE_OFFSET 10
#endif

static inline void preempt_disable() 
{
    void *base = __cpu_state_get_cpu();
    if (base) {
	// per-cpu functional
#if defined(NAUT_CONFIG_ARCH_RISCV) || defined(NAUT_CONFIG_ARCH_ARM64)
	atomic_add(*(uint32_t *)((uint64_t)base+PREEMPT_DISABLE_OFFSET),1);
#else
	atomic_add(*(uint16_t *)((uint64_t)base+PREEMPT_DISABLE_OFFSET),1);
#endif
    } else {
	// per-cpu is not running, so we are not going to get preempted anyway
    }
}

static inline void preempt_enable() 
{
    void *base = __cpu_state_get_cpu();
    if (base) {
	// per-cpu functional
#if defined(NAUT_CONFIG_ARCH_RISCV) || defined(NAUT_CONFIG_ARCH_ARM64)
	atomic_sub(*(uint32_t *)((uint64_t)base+PREEMPT_DISABLE_OFFSET),1);
#else
	atomic_sub(*(uint16_t *)((uint64_t)base+PREEMPT_DISABLE_OFFSET),1);
#endif
    } else {
	// per-cpu is not running, so we are not going to get preempted anyway
    }
}

// make cpu preeemptible once again
// this should only be called by scheduler in handling
// special context switch requests
static inline void preempt_reset() 
{
    void *base = __cpu_state_get_cpu();
    if (base) {
	// per-cpu functional
#if defined(NAUT_CONFIG_ARCH_RISCV) || defined(NAUT_CONFIG_ARCH_ARM64)
	atomic_and(*(uint32_t *)((uint64_t)base+PREEMPT_DISABLE_OFFSET),0);
#else
	atomic_and(*(uint16_t *)((uint64_t)base+PREEMPT_DISABLE_OFFSET),0);
#endif
    } else {
	// per-cpu is not running, so we are not going to get preempted anyway
    }
}

static inline int preempt_is_disabled()
{
    void *base = __cpu_state_get_cpu();
    if (base) {
	// per-cpu functional
#if defined(NAUT_CONFIG_ARCH_RISCV) || defined(NAUT_CONFIG_ARCH_ARM64)
	return atomic_add(*(uint32_t *)((uint64_t)base+PREEMPT_DISABLE_OFFSET),0);
#else
	return atomic_add(*(uint16_t *)((uint64_t)base+PREEMPT_DISABLE_OFFSET),0);
#endif
    } else {
	// per-cpu is not running, so we are not going to get preempted anyway
	return 1;
    }
}

static inline uint16_t interrupt_nesting_level()
{
    void *base = __cpu_state_get_cpu();
    if (base) {
#if defined(NAUT_CONFIG_ARCH_RISCV) || defined(NAUT_CONFIG_ARCH_ARM64)
	return atomic_add(*(uint32_t *)((uint64_t)base+INL_OFFSET),0);
#else
	return atomic_add(*(uint16_t *)((uint64_t)base+INL_OFFSET),0);
#endif
    } else {
	return 0; // no interrupt should be on if we don't have percpu
    }
    
}

static inline int in_interrupt_context()
{
    return interrupt_nesting_level()>0;
}



void arch_enable_ints(void);
void arch_disable_ints(void);
int arch_ints_enabled(void);

// Do not directly use these functions or sti/cli unless you know
// what you are doing...  
// Instead, use irq_disable_save and a matching irq_enable_restore
#define enable_irqs() arch_enable_ints()
#define disable_irqs() arch_disable_ints()

static inline uint8_t irqs_enabled (void)
{
    return arch_ints_enabled();
    // uint64_t rflags = read_rflags();
    // return (rflags & RFLAGS_IF) != 0;
}

static inline uint8_t irq_disable_save (void)
{
    preempt_disable();

    uint8_t enabled = irqs_enabled();

    if (enabled) {
	arch_disable_ints();
        // disable_irqs();
    }

    preempt_enable();

    return enabled;
}
 

static inline void irq_enable_restore (uint8_t iflag)
{
    if (iflag) {
        /* Interrupts were originally enabled, so turn them back on */
	arch_enable_ints();
        // enable_irqs();
    }
}

#endif
