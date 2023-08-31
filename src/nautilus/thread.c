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
 * Copyright (c) 2017, Peter A. Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2015, The V3VEE Project  <http://www.v3vee.org>
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Kyle C. Hale <kh@u.northwestern.edu>
 *          Peter A. Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#include <nautilus/nautilus.h>
#include <nautilus/cpu.h>
#include <nautilus/naut_assert.h>
#include <nautilus/irq.h>
#include <nautilus/idle.h>
#include <nautilus/paging.h>
#include <nautilus/thread.h>
#include <nautilus/waitqueue.h>
#include <nautilus/timer.h>
#include <nautilus/percpu.h>
#include <nautilus/atomic.h>
#include <nautilus/queue.h>
#include <nautilus/list.h>
#include <nautilus/errno.h>
#include <nautilus/mm.h>

#ifdef NAUT_CONFIG_ARCH_X86
#include<arch/x64/gdt.h>
#endif

#ifdef NAUT_CONFIG_ENABLE_BDWGC
#include <gc/bdwgc/bdwgc.h>
#endif

extern uint8_t malloc_cpus_ready;



#ifndef NAUT_CONFIG_DEBUG_THREADS
#undef  DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif
#define THREAD_INFO(fmt, args...) INFO_PRINT("Thread: " fmt, ##args)
#define THREAD_ERROR(fmt, args...) ERROR_PRINT("Thread: " fmt, ##args)
#define THREAD_DEBUG(fmt, args...) DEBUG_PRINT("Thread: " fmt, ##args)
#define THREAD_WARN(fmt, args...)  WARN_PRINT("Thread: " fmt, ##args)

static unsigned long next_tid = 0;

extern addr_t boot_stack_start;
extern void nk_thread_switch(nk_thread_t*);
extern void nk_thread_entry(void *);
static struct nk_tls tls_keys[TLS_MAX_KEYS];

static void * nk_thread_set_tls(int placement_cpu);


/****** SEE BELOW FOR EXTERNAL THREAD INTERFACE ********/



/*
 * thread_detach
 *
 * detaches a child from its parent
 *
 * @t: the thread to detach
 *
 */
static int
thread_detach (nk_thread_t * t)
{
    preempt_disable();

    ASSERT(t->refcount > 0);

    /* remove me from my parent's child list */
    list_del(&(t->child_node));

    --t->refcount;

    // conditional reaping is done by the scheduler when threads are created
    // this makes the join+exit path much faster in the common case and
    // bulks reaping events together
    // the user can also explictly reap when needed
    // plus the autoreaper thread can be enabled
    // the following code can be enabled if you want to reap immediately once
    // a thread's refcount drops to zero
    //
    //if (t->refcount==0) {
    //   nk_thread_destroy(t);
    //}

    preempt_enable();

    return 0;
}




static void
tls_exit (void)
{
    nk_thread_t * t = get_cur_thread();
    unsigned i, j;
    uint8_t called = 0;

    for (i = 0; i < MIN_DESTRUCT_ITER; i++) {
        for (j = 0 ; j < TLS_MAX_KEYS; j++) {
            void * val = (void*)t->tls[j];
            if (val && tls_keys[j].destructor) {
                called = 1;
                t->tls[j] = NULL;
                tls_keys[j].destructor(val);
            }

            if (!called) {
                break;
            }
        }
    }
}

int
_nk_thread_init (nk_thread_t * t,
		 void * stack,
		 uint8_t is_detached,
		 int bound_cpu,
		 int placement_cpu,
		 nk_thread_t * parent)
{
    struct sys_info * sys = per_cpu_get(system);

    if (!t) {
        THREAD_ERROR("Given NULL thread pointer...\n");
        return -EINVAL;
    }

    if (bound_cpu>=0 && bound_cpu>=sys->num_cpus) {
	THREAD_ERROR("Impossible CPU binding %d\n",bound_cpu);
	return -EINVAL;
    }


    t->stack      = stack;
    t->rsp        = (uint64_t)stack + t->stack_size - (2*sizeof(uint64_t));
    t->tid        = atomic_inc(next_tid) + 1;
    t->refcount   = is_detached ? 1 : 2; // thread references itself as well
    t->parent     = parent;
    t->bound_cpu  = bound_cpu;
    t->placement_cpu = placement_cpu;
    t->current_cpu = placement_cpu;
    t->fpu_state_offset = offsetof(struct nk_thread, fpu_state);

    INIT_LIST_HEAD(&(t->children));

    /* I go on my parent's child list if I'm not detached */
    if (parent && !is_detached) {
        list_add_tail(&(t->child_node), &(parent->children));
    }

    // scheduler will handle reanimated thread correctly here
    if (!(t->sched_state = nk_sched_thread_state_init(t,0))) {
	THREAD_ERROR("Could not create scheduler state for thread\n");
	return -EINVAL;
    }

#ifdef NAUT_CONFIG_ENABLE_BDWGC
    if (!(t->gc_state = nk_gc_bdwgc_thread_state_init(t))) {
	THREAD_ERROR("Failed to initialize GC state for thread\n");
	return -1;
    }
#endif

    // skip wait queue allocation for a reanimated thread

    if (!t->waitq) {
	t->waitq = nk_wait_queue_create(0);
	if (!t->waitq) {
	    THREAD_ERROR("Could not create thread's wait queue\n");
	    return -EINVAL;
	}
    }

    // update the wait queue name given the current thread id
    snprintf(t->waitq->name,NK_WAIT_QUEUE_NAME_LEN,"thread-%lu-wait",t->tid);

    // the timer is allocated at first use, not here, although it
    // is possible for reanimated thread to already have a timer
    // if so, also update its name
    if (t->timer) {
	snprintf(t->timer->name,NK_TIMER_NAME_LEN,"thread-%lu-timer",t->tid);
    }


    return 0;
}

uint64_t __nk_thread_fpu_state_offset = offsetof(struct nk_thread, fpu_state);

void
thread_cleanup (void)
{
    THREAD_DEBUG("Thread (%d) exiting on core %d\n", get_cur_thread()->tid, my_cpu_id());
    nk_thread_exit(0);
}

static inline void thread_print_stack(nk_thread_t *t) {
  uint64_t *base = (uint64_t*)(((uint8_t*)t->stack) + t->stack_size);
  THREAD_DEBUG("Thread %p's Stack: base = %p, rsp = %p\n", (void*)t, base, (uint64_t*)t->rsp);
  while((uint64_t)base > t->rsp) {
    base--;
    THREAD_DEBUG("\t0x%016x\n", *base);
  }
  THREAD_DEBUG("End of stack\n");
}

/*
 * utility function for setting up
 * a thread's stack
 */
void
thread_push (nk_thread_t * t, uint64_t x)
{
    THREAD_DEBUG("Pushing 0x%016x onto thread %p's stack\n", x, t);
    t->rsp -= 8;
    *(uint64_t*)(t->rsp) = x;
}

void
thread_setup_init_stack (nk_thread_t * t, nk_thread_fun_t fun, void * arg)
{

  if(t == NULL) {
    ERROR_PRINT("Trying to initialize the stack of a NULL thread!\n");
    panic("Trying to initialize the stack of a NULL thread!\n");
  }

#ifdef NAUT_CONFIG_ARCH_RISCV
#define GPR_SAVE_SIZE      31*8
#define GPR_RDI_OFFSET     176
    if (fun) {
        thread_push(t, (uint64_t)&thread_cleanup);
        thread_push(t, (uint64_t)fun);
        thread_push(t, (uint64_t)0);
        *(uint64_t*)(t->rsp-GPR_RDI_OFFSET) = (uint64_t)arg;
        *(uint64_t*)(t->rsp-GPR_SAVE_SIZE)  = (uint64_t)nk_thread_entry;
    }

#elif NAUT_CONFIG_ARCH_X86
#define RSP_STACK_OFFSET   8
#define GPR_RDI_OFFSET     48
#define GPR_RAX_OFFSET     8
#define GPR_SAVE_SIZE      120
#define STACK_SAVE_SIZE    64
#define THREAD_SETUP_SIZE  (STACK_SAVE_SIZE + GPR_SAVE_SIZE)

    /*
     * if this is a thread fork, this part is taken care of
     * in _thread_fork(). There is no function!
     */
    if (fun) {
        thread_push(t, (uint64_t)&thread_cleanup);
        thread_push(t, (uint64_t)fun);
    }

    thread_push(t, (uint64_t)KERNEL_SS);                 // SS
    thread_push(t, (uint64_t)(t->rsp+RSP_STACK_OFFSET)); // rsp
    thread_push(t, (uint64_t)0UL);                       // rflags
    thread_push(t, (uint64_t)KERNEL_CS);
    thread_push(t, (uint64_t)&nk_thread_entry);
    thread_push(t, 0);                                   // dummy error code
    thread_push(t, 0);                                   // intr no

    /*
     * if we have a function, it needs an input argument
     * so we overwrite its RDI
     */
    if (fun) {
        *(uint64_t*)(t->rsp-GPR_RDI_OFFSET) = (uint64_t)arg;
    }

    /*
     * if this is a thread fork, we return 0 to the child
     * via RAX - note that _fork_return will not restore RAX
     */
    if (!fun) {
        *(uint64_t*)(t->rsp-GPR_RAX_OFFSET) = 0;
    }
#elif NAUT_CONFIG_ARCH_ARM64
    #define GPR_SAVE_SIZE 0x120
    #define GPR_X0_OFFSET (GPR_SAVE_SIZE - 0x70 - 0x00)
    #define GPR_LR_OFFSET (GPR_SAVE_SIZE - 0x70 - 0xa0)
    #define INTERRUPT_RETURN_OFFSET (GPR_SAVE_SIZE - 0x50)
    #define DAIF_OFFSET (GPR_SAVE_SIZE - 0x60)

    if (fun) {
        thread_push(t, (uint64_t)thread_cleanup);
        thread_push(t, (uint64_t)fun);
        *(uint64_t*)(t->rsp-GPR_X0_OFFSET) = (uint64_t)arg;
        *(uint64_t*)(t->rsp-GPR_LR_OFFSET)  = (uint64_t)nk_thread_entry;
        *(uint64_t*)(t->rsp-INTERRUPT_RETURN_OFFSET) = (uint64_t)0;
        *(uint64_t*)(t->rsp-DAIF_OFFSET) = (uint64_t)0b1011;
    }
#endif

    t->rsp -= GPR_SAVE_SIZE;                             // account for the GPRS;
}


static void nk_thread_brain_wipe(nk_thread_t *t);


#ifdef NAUT_CONFIG_HARDWARE_TLS
static int hwtls_config_kernel_tls(nk_thread_t *t)
{

    int placement_cpu = t->placement_cpu;
    
    extern addr_t _tbss_end, _tdata_start,_tdata_end;

    addr_t tdata_start = (addr_t) &_tdata_start;
    addr_t tdata_end = (addr_t) &_tdata_end;
    addr_t tbss_end = (addr_t) &_tbss_end;
    uint64_t datasize = tdata_end-tdata_start;
    
    //tbss follows closely after tdata;
    uint64_t bsssize = tbss_end-tdata_end;
    
    // nk_dump_mem(tdata_start, datasize+bsssize);
    // printf("========size tdata  %d tbss  %d\n",datasize, bsssize);
    // printf("====+++++======= tdata start %04x tdata end %04x \n", tdata_start, tdata_end);


    
    //malloc problem: increment tls_loc gives error fs;
    //uint64_t align = 16;
    uint64_t slack = 64;

    if (!t->tls_loc) { // need to allocate it
	t->tls_loc = malloc_specific(datasize+bsssize+slack, placement_cpu);
	
	if (!t->tls_loc) {
	    THREAD_ERROR("Cannot allocate tls space for thread\n");
	    return -1;
	}
    }

    //printf("=======tls loc %p \n", tls_loc);

    memset(t->tls_loc, 0, datasize+bsssize);

    /*
     * -------------------------------------------------------
     *  |  |  |  |  | tdata | tdata | tdata | tbss |tbss|tcb|  
     * ------------------------------------------------------
    */
    memcpy(t->tls_loc, (void*)tdata_start, datasize);

    t->hwtls = t->tls_loc + datasize + bsssize;
    
    //printf("hwtls : %p \n", t->hwtls);
    //    uint64_t fsbase = (uint64_t)(tls_loc+datasize+bsssize);
    
    //force fs:0x0 to have value fsbase    
    *((void**)t->hwtls) = t->hwtls;

    return 0;

}
#endif



/****** EXTERNAL THREAD INTERFACE ******/



/*
 * nk_thread_create
 *
 * creates a thread.
 * its stack wil not be initialized with any intial data,
 * and it will go on the thread list, but it wont be runnable
 *
 * @fun: the function to run
 * @input: the argument to the thread
 * @output: where the thread should store its output
 * @is_detached: true if this thread won't be attached to a parent (it will
 *               die immediately when it exits)
 * @stack_size: size of the thread's stack. 0 => let us decide
 * @tid: opaque user object for the thread to be set (this is the output)
 * @bound_cpu: cpu on which to bind the thread. CPU_ANY means any CPU
 *
 * return: on error returns -EINVAL, returns 0 on success
 *
 */
int
nk_thread_create (nk_thread_fun_t fun,
                  void * input,
                  void ** output,
                  uint8_t is_detached,
                  nk_stack_size_t stack_size,
                  nk_thread_id_t * tid,
                  int bound_cpu)
{
    struct sys_info * sys = per_cpu_get(system);
    nk_thread_t * t = NULL;
    int placement_cpu = bound_cpu<0 ? nk_sched_initial_placement() : bound_cpu;
    nk_stack_size_t required_stack_size = stack_size ? stack_size: PAGE_SIZE;


    // First try to get a thread from the scheduler's pools
    if ((t=nk_sched_reanimate(required_stack_size,
			      placement_cpu))) {
	// we have succeeded in reanimating a dead thread, so
	// now all we need to do is the management that
	// nk_thread_destroy() would otherwise have done
	nk_thread_brain_wipe(t);


    } else {

	// failed to reanimate existing dead thread, so we need to
	// make our own

	t = malloc_specific(sizeof(nk_thread_t),placement_cpu);

	if (!t) {
	    THREAD_ERROR("Could not allocate thread struct\n");
	    return -EINVAL;
	}

	memset(t, 0, sizeof(nk_thread_t));

	t->stack_size = required_stack_size;

	t->stack = (void*)malloc_specific(required_stack_size,placement_cpu);

	if (!t->stack) {

	    THREAD_ERROR("Failed to allocate a stack\n");
	    free(t);
	    return -EINVAL;
	}

    }

    // from this point, if we are using a reanimated thread, and
    // we fail, we can safely free the thread the same as we
    // would if we were newly allocating it.

    if (_nk_thread_init(t, t->stack, is_detached, bound_cpu, placement_cpu, get_cur_thread()) < 0) {
        THREAD_ERROR("Could not initialize thread\n");
        goto out_err;
    }

    t->status = NK_THR_INIT;

    // a thread joins its creator's address space
    t->aspace = get_cur_thread()->aspace;

#ifdef NAUT_CONFIG_HARDWARE_TLS
    if (hwtls_config_kernel_tls(t)) {
        THREAD_ERROR("Could not clone kernel tls\n");
        goto out_err;
    t->signal_state = 0;
    }
#endif

#ifdef NAUT_CONFIG_PROCESSES
    // a thread will not belong to a process by default
    t->process = NULL;
#endif

#ifdef NAUT_CONFIG_ENABLE_SIGNALS
    if (nk_signal_init_task_state(&t->signal_state, t)) {
        THREAD_ERROR("Failed to initialize signal state :(\n");
        return -1;
    }
#endif

    t->fun = fun;
    t->input = input;
    t->output_loc = output;
    t->output = 0;

    // scheduler will handle reanimated thread correctly
    if (nk_sched_thread_post_create(t)) {
	THREAD_ERROR("Scheduler does not accept thread creation\n");
	goto out_err;
    }

    if (tid) {
        *tid = (nk_thread_id_t)t;
    }

    THREAD_DEBUG("Thread create creating new thread with t=%p, tid=%lu\n", t, t->tid);

    return 0;

out_err:
    if (t->timer) {
	nk_timer_destroy(t->timer);
    }
    if (t->waitq) {
	nk_wait_queue_destroy(t->waitq);
    }
    if (t->sched_state) {
	nk_sched_thread_state_deinit(t);
    }

#ifdef NAUT_CONFIG_ENABLE_BDWGC
    if (t->gc_state) {
	nk_gc_bdwgc_thread_state_deinit(t);
    }
#endif

    // note that VC is not assigned on thread creation
    // so we do not need to clean it up

    free(t->stack);
    free(t);

    return -EINVAL;
}


/*
 * nk_thread_start
 *
 * creates a thread and puts it on the specified cpu's run
 * queue
 *
 * @fun: the function to run
 * @input: the argument to the thread
 * @output: where the thread should store its output
 * @is_detached: true if this thread won't be attached to a parent (it will
 *               die immediately when it exits)
 * @stack_size: size of the thread's stack. 0 => let us decide
 * @tid: the opaque pointer passed to the user (output variable)
 * @bound_cpu: cpu on which to bind the thread. CPU_ANY means any CPU
 *
 *
 * on error, returns -EINVAL, otherwise 0
 */
int
nk_thread_start (nk_thread_fun_t fun,
                 void * input,
                 void ** output,
                 uint8_t is_detached,
                 nk_stack_size_t stack_size,
                 nk_thread_id_t * tid,
                 int bound_cpu)
{
    nk_thread_id_t newtid   = NULL;
    nk_thread_t * newthread = NULL;

    THREAD_DEBUG("Start thread, caller %p\n", __builtin_return_address(0));

    if (nk_thread_create(fun, input, output, is_detached, stack_size, &newtid, bound_cpu) < 0) {
        THREAD_ERROR("Could not create thread\n");
        return -1;
    }

    newthread = (nk_thread_t*)newtid;

    if (tid) {
        *tid = newtid;
    }

    return nk_thread_run(newthread);
}

int nk_thread_run(nk_thread_id_t t)
{
  nk_thread_t * newthread = (nk_thread_t*)t;

  THREAD_DEBUG("Trying to execute thread %p (tid %lu)\n", newthread,newthread->tid);

  THREAD_DEBUG("RUN: Function: %p\n", newthread->fun);
  THREAD_DEBUG("RUN: Bound_CPU: %llu\n", newthread->bound_cpu);
  THREAD_DEBUG("RUN: Current_CPU: %llu\n", newthread->current_cpu);

  thread_setup_init_stack(newthread, newthread->fun, newthread->input);


  THREAD_DEBUG("Run thread initialized: %p (tid=%lu) stack=%p size=%lu rsp=%p\n",newthread,newthread->tid,newthread->stack,newthread->stack_size,newthread->rsp);

#ifdef NAUT_CONFIG_FPU_SAVE
    // clone the floating point state
    extern void nk_fp_save(void *dest);
    nk_fp_save(newthread->fpu_state);
#endif

  if (nk_sched_make_runnable(newthread, newthread->current_cpu,1)) {
      THREAD_ERROR("Scheduler failed to run thread (%p, tid=%u) on cpu %u\n",
		  newthread, newthread->tid, newthread->current_cpu);
      return -1;
  }

#ifdef NAUT_CONFIG_DEBUG_THREADS
  if (newthread->bound_cpu == CPU_ANY) {
      THREAD_DEBUG("Running thread (%p, tid=%u) on [ANY CPU] current_cpu=%d\n", newthread, newthread->tid,newthread->current_cpu);
  } else {
      THREAD_DEBUG("Running thread (%p, tid=%u) on bound cpu %u\n", newthread, newthread->tid, newthread->current_cpu);
  }
#endif
 
  nk_sched_kick_cpu(newthread->current_cpu);

  return 0;
}

int nk_thread_name(nk_thread_id_t tid, char *name)
{
  nk_thread_t * t = (nk_thread_t*)tid;
  strncpy(t->name,name,MAX_THREAD_NAME);
  t->name[MAX_THREAD_NAME-1] = 0;
  // update wait queue based on name
  snprintf(t->waitq->name,NK_WAIT_QUEUE_NAME_LEN,"thread-%s-wait",t->name);
  // update timer, if exists, based on name
  if (t->timer) {
      snprintf(t->timer->name,NK_TIMER_NAME_LEN,"thread-%s-timer",t->name);
  }
  return 0;
}


/*
 * wake_waiters
 *
 * wake all threads that are waiting on me
 *
 */
void
nk_wake_waiters (void)
{
    nk_thread_t * me  = get_cur_thread();
    nk_wait_queue_wake_all(me->waitq);
}

void nk_yield()
{
    struct nk_thread *me = get_cur_thread();

    spin_lock(&me->lock);

    nk_sched_yield(&me->lock);
}



/*
 * nk_thread_exit
 *
 * exit from this thread
 *
 * @retval: the value to return to the parent
 *
 * If there is someone waiting on this thread, this
 * function will wake them up. This will also call
 * any destructors for thread local storage
 *
 */
void nk_thread_exit (void * retval)
{
    nk_thread_t * me = get_cur_thread();
    nk_wait_queue_t * wq = me->waitq;
    uint8_t flags;

    THREAD_DEBUG("Thread %p (tid=%u (%s)) exiting, joining with children\n", me, me->tid, me->name);

    /* wait for my children to finish */
    nk_join_all_children(NULL);

    THREAD_DEBUG("Children joined\n");

    /* clear any thread local storage that may have been allocated */
    tls_exit();

    THREAD_DEBUG("TLS exit complete\n");

    // lock out anyone else looking at my wait queue
    // we need to do this before we change our own state
    // so we can avoid racing with someone who is attempting
    // to join with us and is putting themselves on our wait queue
    flags = spin_lock_irq_save(&wq->lock);
    preempt_disable();
    irq_enable_restore(flags);   // we only need interrupts off long enough to lock out the scheduler

    THREAD_DEBUG("Lock acquired\n");

    // at this point, we have the lock on our wait queue and preemption is disabled

    // we must not overwrite a previous setting of the output value
    // for example from a fork...
    if (!me->output) {
	me->output      = retval;
	if (me->output_loc) {
	    *me->output_loc = retval;
	}
    }

    me->status      = NK_THR_EXITED;

    // force arch and compiler to do above writes now
    mbarrier();

    THREAD_DEBUG("State update complete\n");

    /* wake up everyone who is waiting on me */
    nk_wait_queue_wake_all_extended(wq,1);

    /* fire timer, if it exists */
    if (me->timer) {
	nk_timer_cancel(me->timer);
    }

    THREAD_DEBUG("Waiting wakeup complete\n");

    me->refcount--;

    THREAD_DEBUG("Thread %p (tid=%u (%s)) exit complete - invoking scheduler\n", me, me->tid, me->name);

    // the scheduler will reenable preemption and release the lock
    nk_sched_exit(&wq->lock);

    /* we should never get here! */
    panic("Should never get here!\n");
}


/*
 * nk_thread_destroy
 *
 * destroys a thread and reclaims its memory (its stack page mostly)
 *
 * @t: the thread to destroy
 *
 */
void
nk_thread_destroy (nk_thread_id_t t)
{
    nk_thread_t * thethread = (nk_thread_t*)t;

    THREAD_DEBUG("Destroying thread (%p, tid=%lu)\n", (void*)thethread, thethread->tid);

    preempt_disable();

    nk_sched_thread_pre_destroy(thethread);

    // If we are on any wait queue at this point, it is an error
    if (thethread->num_wait) {
	THREAD_ERROR("Destroying thread %p (tid=%lu name=%s) that is on %d wait queues...\n",
		     thethread, thethread->tid, thethread->name, thethread->num_wait);
    }

    /* remove its own wait queue
     * (waiters should already have been notified */
    nk_wait_queue_destroy(thethread->waitq);

    if (thethread->timer) {
	// cancel + destroy the timer if it exists
	nk_timer_destroy(thethread->timer);
    }

    nk_sched_thread_state_deinit(thethread);

#ifdef NAUT_CONFIG_ENABLE_BDWGC
    nk_gc_bdwgc_thread_state_deinit(thethread);
#endif

#ifdef NAUT_CONFIG_HARDWARE_TLS
    if (thethread->tls_loc) {
	free(thethread->tls_loc);
    }
#endif

    free(thethread->stack);
    free(thethread);

    preempt_enable();
}

/*
 * nk_thread_brain_wipe
 *
 * "destroys" a thread without deallocating it so that
 * it can be reused.  This needs to mirror nk_thread_destroy
 *
 * Only threads returned from nk_sched_reanimate() should be
 * be brain-wiped.   The caller also needs to initialize
 * the thread as normal, except no allocations
 *
 * @t: the thread to brain-wipe
 *
 */
static void nk_thread_brain_wipe(nk_thread_t *t)
{
    nk_thread_t * thethread = (nk_thread_t*)t;

    THREAD_DEBUG("Brain-wiping thread (%p, tid=%lu)\n", (void*)thethread, thethread->tid);

    // no pre-destroy is done as nk_sched_reanimate has already
    // removed it from the relevant scheduler thread lists/queues

    // If we are on any wait list at this point, it is an error
    if (thethread->num_wait) {
	THREAD_ERROR("Destroying thread %p (tid=%lu name=%s) that is on %d wait queues...\n",
		     thethread, thethread->tid, thethread->name, thethread->num_wait);
	//nk_dequeue_entry(&(thethread->wait_node));
    }

    // we do not delete its own wait queue as we will simply
    // reuse it

    // we do not destroy the timer, if it exists, but we will fire it
    if (t->timer) {
	nk_timer_cancel(t->timer);
    }

    // We do not deinit its scheduler state, since since
    // the scheduler will reuse the state in
    // nk_sched_thread_state_init()

    // GC handling must happen - only relevant for
    // Boehm at this point
#ifdef NAUT_CONFIG_ENABLE_BDWGC
    nk_gc_bdwgc_thread_state_deinit(thethread);
#endif

    // nothing else is freed

    // do only absolutely minimal cleanup so we don't need to zero the whole thing
    thethread->tid=0;
    thethread->name[0]=0;

    // vc management and other handles are up to caller, similar to
    // thread destroy

}



static int exit_check(void *state)
{
    volatile nk_thread_t *thethread = (nk_thread_t *)state;

    THREAD_DEBUG("exit_check: thread (%lu %s) status is %u\n",thethread->tid,thethread->name,thethread->status);
    return thethread->status==NK_THR_EXITED;
}


/*
 * nk_join
 *
 * join (wait) on the given thread
 *
 * t: the thread to wait on
 * retval: where the waited-on thread should
 *         put its output
 *
 * returns  -EINVAL on error, 0 on success
 *
 */
int
nk_join (nk_thread_id_t t, void ** retval)
{
    nk_thread_t *thethread = (nk_thread_t*)t;

    THREAD_DEBUG("Join initiated for thread %lu \"%s\"\n", thethread->tid, thethread->name);

    ASSERT(thethread->parent == get_cur_thread());

    nk_wait_queue_sleep_extended(thethread->waitq, exit_check, thethread);

    THREAD_DEBUG("Join commenced for thread %lu \"%s\"\n", thethread->tid, thethread->name);

    ASSERT(exit_check(thethread));

    if (retval) {
        *retval = thethread->output;
    }

    thread_detach(thethread);

    THREAD_DEBUG("Join completed for thread %lu \"%s\"\n", thethread->tid, thethread->name);

    return 0;
}





/*
 * nk_join_all_children
 *
 * Join all threads that the current thread
 * has either forked or spawned
 *
 * @func: this function will be called with each
 *        output value generated by this threads
 *        children
 *
 *  returns -EINVAL on error, 0 on success
 *
 */
int
nk_join_all_children (int (*func)(void * res))
{
    nk_thread_t * elm = NULL;
    nk_thread_t * tmp = NULL;
    nk_thread_t * me         = get_cur_thread();
    void * res               = NULL;
    int ret                  = 0;

    list_for_each_entry_safe(elm, tmp, &(me->children), child_node) {

        if (nk_join(elm, &res) < 0) {
            THREAD_ERROR("Could not join child thread (t=%p)\n", elm);
            ret = -1;
            continue;
        }

        if (func) {
            if (func(res) < 0) {
                THREAD_ERROR("Consumer indicated error for child thread (t=%p, output=%p)\n", elm,res);
                ret = -1;
                continue;
            }
        }

    }

    return ret;
}




/*
 * nk_set_thread_output
 *
 * @result: the output to set
 *
 */
void
nk_set_thread_output (void * result)
{
    nk_thread_t* t = get_cur_thread();
    // output written exactly once
    if (!t->output) {
	t->output = result;
	if (t->output_loc) {
	    *t->output_loc = result;
	}
    }
}


int nk_thread_change_hw_tls(nk_thread_id_t tid, void *hwtls)
{
// RISCV HACK
#ifdef NAUT_CONFIG_ARCH_X86
    ((nk_thread_t*)tid)->hwtls = hwtls;
    msr_write(MSR_FS_BASE,(uint64_t)hwtls);
#endif
    return 0;
}


/*
 * nk_tls_key_create
 *
 * create thread local storage
 *
 * @key: where to stash the created key
 * @destructor: function pointer to be called when the thread
 *              exits
 *
 * returns -EAGAIN on error, 0 on success
 *
 */
int
nk_tls_key_create (nk_tls_key_t * key, void (*destructor)(void*))
{
    int i;

    for (i = 0; i < TLS_MAX_KEYS; i++) {
        unsigned sn = tls_keys[i].seq_num;

        if (TLS_KEY_AVAIL(sn) && TLS_KEY_USABLE(sn) &&
            atomic_cmpswap(tls_keys[i].seq_num, sn, sn+1) == sn) {

            tls_keys[i].destructor = destructor;
            *key = i;

            /* we're done */
            return 0;
        }
    }

    return -EAGAIN;
}


/*
 * nk_tls_key_delete
 *
 * @key: the key to delete
 *
 * returns -EINVAL on error, 0 on success
 *
 */
int
nk_tls_key_delete (nk_tls_key_t key)
{
    if (likely(key < TLS_MAX_KEYS)) {
        unsigned sn = tls_keys[key].seq_num;

        if (likely(!TLS_KEY_AVAIL(sn)) &&
            atomic_cmpswap(tls_keys[key].seq_num, sn, sn+1) == sn) {
            return 0;
        }
    }

    return -EINVAL;
}


/*
 * nk_tls_get
 *
 * get the value stored for this key for this
 * thread
 *
 * @key: the key to act as index for value lookup
 *
 * returns NULL on error, the value otherwise
 *
 */
void*
nk_tls_get (nk_tls_key_t key)
{
    nk_thread_t * t;

    if (unlikely(key >= TLS_MAX_KEYS)) {
        return NULL;
    }

    t = get_cur_thread();
    return (void*)t->tls[key];
}


/*
 * nk_tls_set
 *
 * @key: the key to use for index lookup
 * @val: the new value to set at this key
 *
 * returns -EINVAL on error, 0 on success
 *
 */
int
nk_tls_set (nk_tls_key_t key, const void * val)
{
    nk_thread_t * t;
    unsigned sn;

    if (key >= TLS_MAX_KEYS ||
        TLS_KEY_AVAIL((sn = tls_keys[key].seq_num))) {
        return -EINVAL;
    }

    t = get_cur_thread();
    t->tls[key] = val;
    return 0;
}


/*
 * nk_get_tid
 *
 * get the opaque thread id for the currently
 * running thread
 *
 */
nk_thread_id_t
nk_get_tid (void)
{
    nk_thread_t * t = (nk_thread_t*)get_cur_thread();
    return (nk_thread_id_t)t;
}


/*
 * nk_get_parent_tid
 *
 * get this thread's parent tid
 *
 */
nk_thread_id_t
nk_get_parent_tid (void)
{
    nk_thread_t * t = (nk_thread_t*)get_cur_thread();
    if (t && t->parent) {
        return (nk_thread_id_t)t->parent;
    }
    return NULL;
}





/********** END EXTERNAL INTERFACE **************/


static void
tls_dummy (void * in, void ** out)
{
    unsigned i;
    nk_tls_key_t * keys = NULL;

    //THREAD_INFO("Beginning test of thread local storage...\n");
    keys = malloc(sizeof(nk_tls_key_t)*TLS_MAX_KEYS);
    if (!keys) {
        THREAD_ERROR("could not allocate keys\n");
        return;
    }

    for (i = 0; i < TLS_MAX_KEYS; i++) {
        if (nk_tls_key_create(&keys[i], NULL) != 0) {
            THREAD_ERROR("Could not create TLS key (%u)\n", i);
            goto out_err;
        }

        if (nk_tls_set(keys[i], (const void *)(i + 100L)) != 0) {
            THREAD_ERROR("Could not set TLS key (%u)\n", i);
            goto out_err;
        }

    }

    for (i = 0; i < TLS_MAX_KEYS; i++) {
        if (nk_tls_get(keys[i]) != (void*)(i + 100L)) {
            THREAD_ERROR("Mismatched TLS val! Got %p, should be %p\n", nk_tls_get(keys[i]), (void*)(i+100L));
            goto out_err;
        }

        if (nk_tls_key_delete(keys[i]) != 0) {
            THREAD_ERROR("Could not delete TLS key %u\n", i);
            goto out_err;
        }
    }

    if (nk_tls_key_create(&keys[0], NULL) != 0) {
        THREAD_ERROR("2nd key create failed\n");
        goto out_err;
    }

    if (nk_tls_key_delete(keys[0]) != 0) {
        THREAD_ERROR("2nd key delete failed\n");
        goto out_err;
    }

    THREAD_INFO("Thread local storage test succeeded\n");

out_err:
    free(keys);
}


void
nk_tls_test (void)
{
    nk_thread_start(tls_dummy, NULL, NULL, 1, TSTACK_DEFAULT, NULL, 1);
}

