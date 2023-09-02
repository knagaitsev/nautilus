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
#include <nautilus/nautilus.h>
#include <nautilus/smp.h>
#include <nautilus/paging.h>
#include <nautilus/cpu.h>
#include <nautilus/naut_assert.h>
#include <nautilus/thread.h>
#include <nautilus/queue.h>
#include <nautilus/idle.h>
#include <nautilus/atomic.h>
#include <nautilus/numa.h>
#include <nautilus/mm.h>
#include <nautilus/percpu.h>

#ifdef NAUT_CONFIG_ARCH_X86
#include <arch/x64/irq.h>
#endif

#ifdef NAUT_CONFIG_ALLOCS
#include <nautilus/alloc.h>
#endif

#ifdef NAUT_CONFIG_ASPACES
#include <nautilus/aspace.h>
#endif

#ifdef NAUT_CONFIG_FIBER_ENABLE
#include <nautilus/fiber.h>
#endif

#ifdef NAUT_CONFIG_ENABLE_REMOTE_DEBUGGING 
#include <nautilus/gdb-stub.h>
#endif

#ifdef NAUT_CONFIG_CACHEPART
#include <nautilus/cachepart.h>
#endif

#ifdef NAUT_CONFIG_LINUX_SYSCALLS
#include <nautilus/syscalls/kernel.h>
#endif


#ifndef NAUT_CONFIG_DEBUG_SMP
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif
#define SMP_PRINT(fmt, args...) printk("SMP: " fmt, ##args)
#define SMP_DEBUG(fmt, args...) DEBUG_PRINT("SMP: " fmt, ##args)

uint32_t
nk_get_num_cpus (void)
{
    struct sys_info * sys = per_cpu_get(system);
    return sys->num_cpus;
}

static void
init_xcall (struct nk_xcall * x, void * arg, nk_xcall_func_t fun, int wait)
{
    x->data       = arg;
    x->fun        = fun;
    x->xcall_done = 0;
    x->has_waiter = wait;
}


static inline void
wait_xcall (struct nk_xcall * x)
{

    while (atomic_cmpswap(x->xcall_done, 1, 0) != 1) {
        arch_relax();
    }
}


static inline void
mark_xcall_done (struct nk_xcall * x) 
{
    atomic_cmpswap(x->xcall_done, 0, 1);
}


#ifdef NAUT_CONFIG_ARCH_X86
int
xcall_handler (struct nk_irq_action * action, struct nk_regs * regs, void *state) 
{
    nk_queue_t * xcq = per_cpu_get(xcall_q); 
    struct nk_xcall * x = NULL;
    nk_queue_entry_t * elm = NULL;

    if (!xcq) {
        ERROR_PRINT("Badness: no xcall queue on core %u\n", my_cpu_id());
        goto out_err;
    }

    elm = nk_dequeue_first_atomic(xcq);
    x = container_of(elm, struct nk_xcall, xcall_node);
    if (!x) {
        ERROR_PRINT("No XCALL request found on core %u\n", my_cpu_id());
        goto out_err;
    }

    if (x->fun) {

        // we ack the IPI before calling the handler funciton,
        // because it may end up blocking (e.g. core barrier)
        IRQ_HANDLER_END(); 

        x->fun(x->data);

        /* we need to notify the waiter we're done */
        if (x->has_waiter) {
            mark_xcall_done(x);
        }

    } else {
        ERROR_PRINT("No XCALL function found on core %u\n", my_cpu_id());
        goto out_err;
    }


    return 0;

out_err:
    IRQ_HANDLER_END();
    return -1;
}
#endif

/* 
 * smp_xcall
 *
 * initiate cross-core call. 
 * 
 * @cpu_id: the cpu to execute the call on
 * @fun: the function to invoke
 * @arg: the argument to the function
 * @wait: this function should block until the reciever finishes
 *        executing the function
 *
 */
int
smp_xcall (cpu_id_t cpu_id, 
           nk_xcall_func_t fun,
           void * arg,
           uint8_t wait)
{
#ifdef NAUT_CONFIG_ARCH_X86
    struct sys_info * sys = per_cpu_get(system);
    nk_queue_t * xcq  = NULL;
    struct nk_xcall x;
    uint8_t flags;

    SMP_DEBUG("Initiating SMP XCALL from core %u to core %u\n", my_cpu_id(), cpu_id);

    if (cpu_id > nk_get_num_cpus()) {
        ERROR_PRINT("Attempt to execute xcall on invalid cpu (%u)\n", cpu_id);
        return -1;
    }

    if (cpu_id == my_cpu_id()) {

        flags = irq_disable_save();
        fun(arg);
        irq_enable_restore(flags);

    } else {
        struct nk_xcall * xc = &x;

        if (!wait) {
            xc = &(sys->cpus[cpu_id]->xcall_nowait_info);
        }

        init_xcall(xc, arg, fun, wait);

        xcq = sys->cpus[cpu_id]->xcall_q;
        if (!xcq) {
            ERROR_PRINT("Attempt by cpu %u to initiate xcall on invalid xcall queue (for cpu %u)\n", 
                        my_cpu_id(),
                        cpu_id);
            return -1;
        }

        flags = irq_disable_save();

        if (!nk_queue_empty_atomic(xcq)) {
            ERROR_PRINT("XCALL queue for core %u is not empty, bailing\n", cpu_id);
            irq_enable_restore(flags);
            return -1;
        }

        nk_enqueue_entry_atomic(xcq, &(xc->xcall_node));

        irq_enable_restore(flags);

        struct apic_dev * apic = per_cpu_get(apic);

        apic_ipi(apic, sys->cpus[cpu_id]->apic->id, IPI_VEC_XCALL);

        if (wait) {
            wait_xcall(xc);
        }

    }
#endif

    return 0;
}
