
/*
* note that this isn't called directly. It is vectored
* into from an assembly stub
*
* On success, parent gets child's tid, child gets 0
* On failure, parent gets NK_BAD_THREAD_ID
*/

#include <nautilus/nautilus.h>
#include <nautilus/thread.h>
#include <nautilus/scheduler.h>

#ifndef NAUT_CONFIG_DEBUG_THREADS
#undef  DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif
#define THREAD_INFO(fmt, args...) INFO_PRINT("Thread: " fmt, ##args)
#define THREAD_ERROR(fmt, args...) ERROR_PRINT("Thread: " fmt, ##args)
#define THREAD_DEBUG(fmt, args...) DEBUG_PRINT("Thread: " fmt, ##args)
#define THREAD_WARN(fmt, args...)  WARN_PRINT("Thread: " fmt, ##args)

extern void thread_cleanup (void);
extern void thread_push (nk_thread_t * t, uint64_t x);
extern void thread_setup_init_stack (nk_thread_t * t, nk_thread_fun_t fun, void * arg);

nk_thread_id_t
__thread_fork (void)
{

   nk_thread_t *parent = get_cur_thread();
   nk_thread_id_t  tid;
   nk_thread_t * t;
   nk_stack_size_t size, alloc_size;
   uint64_t     rbp1_offset_from_ret0_addr;
   uint64_t     rbp_stash_offset_from_ret0_addr;
   uint64_t     rbp_offset_from_ret0_addr;
   void         *child_stack;
   uint64_t     rsp;

   rsp = (uint64_t) arch_read_sp();

   printk("thread fork rsp = %p\n", rsp);

#ifdef NAUT_CONFIG_ENABLE_STACK_CHECK
   // now check again after update to see if we didn't overrun/underrun the stack in the parent...
   if ((uint64_t)rsp <= (uint64_t)parent->stack ||
       (uint64_t)rsp >= (uint64_t)(parent->stack + parent->stack_size)) {
       THREAD_ERROR("Parent's top of stack (%p) exceeds boundaries of stack (%p-%p)\n",
                    rsp, parent->stack, parent->stack+parent->stack_size);
       panic("Detected stack out of bounds in parent during fork\n");
       return NK_BAD_THREAD_ID;
   }
#endif

   THREAD_DEBUG("Forking thread from parent=%p tid=%lu stack=%p-%p rsp=%p\n", parent, parent->tid,parent->stack,parent->stack+parent->stack_size,rsp);

#ifdef NAUT_CONFIG_THREAD_OPTIMIZE
   THREAD_WARN("Thread fork may function incorrectly with aggressive threading optimizations\n");
#endif

   void *rbp0      = __builtin_frame_address(0);                   // current rbp, *rbp0 = rbp1
   void *rbp1      = __builtin_frame_address(1);                   // caller rbp, *rbp1 = rbp2  (forker's frame)
   void *rbp_tos   = __builtin_frame_address(STACK_CLONE_DEPTH);   // should scan backward to avoid having this be zero or crazy
   void *ret0_addr = rbp0 + 8;
   // this is the address at which the fork wrapper (nk_thread_fork) stashed
   // the current value of rbp - this must conform to the REG_SAVE model
   // in thread_low_level.S
   void *rbp_stash_addr = ret0_addr + 10*8;

   // we're being called with a stack not as deep as STACK_CLONE_DEPTH...
   // fail back to a single frame...
   if ((uint64_t)rbp_tos <= (uint64_t)parent->stack ||
       (uint64_t)rbp_tos >= (uint64_t)(parent->stack + parent->stack_size)) {
       THREAD_DEBUG("Cannot resolve %lu stack frames on fork, using just one\n", STACK_CLONE_DEPTH);
       rbp_tos = rbp1;
   }


   // from last byte of tos_rbp to the last byte of the stack on return from this function
   // (return address of wrapper)
   // the "launch pad" is added so that in the case where there is no stack frame above the caller
   // we still have the space to fake one.
   size = (rbp_tos + 8) - ret0_addr + LAUNCHPAD;

   //THREAD_DEBUG("rbp0=%p rbp1=%p rbp_tos=%p, ret0_addr=%p\n", rbp0, rbp1, rbp_tos, ret0_addr);

   rbp1_offset_from_ret0_addr = rbp1 - ret0_addr;

   rbp_stash_offset_from_ret0_addr = rbp_stash_addr - ret0_addr;
   rbp_offset_from_ret0_addr = (*(void**)rbp_stash_addr) - ret0_addr;

   alloc_size = parent->stack_size;

   if (nk_thread_create(NULL,        // no function pointer, we'll set rip explicity in just a sec...
                        NULL,        // no input args, it's not a function
                        NULL,        // no output args
                        0,           // this thread's parent will wait on it
                        alloc_size,  // stack size
                        &tid,        // give me a thread id
                        CPU_ANY)     // not bound to any particular CPU
           < 0) {
       THREAD_ERROR("Could not fork thread\n");
       return NK_BAD_THREAD_ID;
   }

   t = (nk_thread_t*)tid;

   THREAD_DEBUG("Forked thread created: %p (tid=%lu) stack=%p size=%lu rsp=%p\n",t,t->tid,t->stack,t->stack_size,t->rsp);

   child_stack = t->stack;

   // this is at the top of the stack, just in case something goes wrong
   thread_push(t, (uint64_t)&thread_cleanup);

   //THREAD_DEBUG("child_stack=%p, alloc_size=%lu size=%lu\n",child_stack,alloc_size,size);
   //THREAD_DEBUG("copy to %p-%p from %p\n", child_stack + alloc_size - size,
   //             child_stack + alloc_size - size + size - LAUNCHPAD, ret0_addr);

   // Copy stack frames of caller and up to stack max
   // this should copy from 1st byte of my_rbp to last byte of tos_rbp
   // notice that leaves ret
   memcpy(child_stack + alloc_size - size, ret0_addr, size - LAUNCHPAD);
   t->rsp = (uint64_t)(child_stack + alloc_size - size);

   // Update the child's snapshot of rbp on its stack (that was done
   // by nk_thread_fork()) with the corresponding position in the child's stack
   // when nk_thread_fork() unwinds the GPRs, it will end up with rbp pointing
   // into the cloned stack
   void **rbp_stash_ptr = (void**)(t->rsp + rbp_stash_offset_from_ret0_addr);
   *rbp_stash_ptr = (void*)(t->rsp + rbp_offset_from_ret0_addr);


   // Determine caller's rbp copy and return addres in the child stack
   void **rbp2_ptr = (void**)(t->rsp + rbp1_offset_from_ret0_addr);
   void **ret2_ptr = rbp2_ptr+1;

   THREAD_DEBUG("Fork: parent stashed rbp=%p, rsp=%p child stashed &rbp=%p rbp=%p, rsp=%p\n",
                rbp0, rsp, rbp_stash_ptr, *rbp_stash_ptr, t->rsp);


   // rbp2 we don't care about since we will not not
   // return from the caller in the child, but rather go into the thread cleanup
   *rbp2_ptr = 0x0ULL;

   // fix up the return address to point to our thread cleanup function
   // so when caller returns, the thread exists
   *ret2_ptr = &thread_cleanup;

   // now we need to setup the interrupt stack etc.
   // we provide null for thread func to indicate this is a fork
   thread_setup_init_stack(t, NULL, NULL);

   THREAD_DEBUG("Forked thread initialized: %p (tid=%lu) stack=%p size=%lu rsp=%p\n",t,t->tid,t->stack,t->stack_size,t->rsp);

#ifdef NAUT_CONFIG_ENABLE_STACK_CHECK
   // now check the child before we attempt to run it
   if ((uint64_t)t->rsp <= (uint64_t)t->stack ||
       (uint64_t)t->rsp >= (uint64_t)(t->stack + t->stack_size)) {
       THREAD_ERROR("Child's rsp (%p) exceeds boundaries of stack (%p-%p)\n",
                    t->rsp, t->stack, t->stack+t->stack_size);
       panic("Detected stack out of bounds in child during fork\n");
       return NK_BAD_THREAD_ID;
   }
#endif

#ifdef NAUT_CONFIG_FPU_SAVE
   // clone the floating point state
   extern void nk_fp_save(void *dest);
   nk_fp_save(t->fpu_state);
#endif

   if (nk_sched_make_runnable(t,t->current_cpu,1)) {
       THREAD_ERROR("Scheduler failed to run thread (%p, tid=%u) on cpu %u\n",
                   t, t->tid, t->current_cpu);
       return NK_BAD_THREAD_ID;
   }

   THREAD_DEBUG("Forked thread made runnable: %p (tid=%lu)\n",t,t->tid);

   // return child's tid to parent
   return tid;
}

