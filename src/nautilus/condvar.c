#include <nautilus/nautilus.h>
#include <nautilus/condvar.h>
#include <nautilus/queue.h>
#include <nautilus/thread.h>
#include <nautilus/errno.h>
#include <nautilus/irq.h>
#include <nautilus/cpu.h>
#include <nautilus/atomic.h>

#include <dev/apic.h>
#include <lib/liballoc.h>

#ifndef NAUT_CONFIG_DEBUG_SYNCH
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

int
nk_condvar_init (nk_condvar_t * c)
{
    DEBUG_PRINT("Condvar init\n");

    memset(c, 0, sizeof(nk_condvar_t));

    c->wait_queue = nk_thread_queue_create();
    if (!c->wait_queue) {
        ERROR_PRINT("Could not create wait queue for cond var\n");
        return -EINVAL;
    }

    NK_LOCK_INIT(&c->lock);

    return 0;
}


int
nk_condvar_destroy (nk_condvar_t * c)
{
    DEBUG_PRINT("Destroying condvar (%p)\n", (void*)c);

    NK_LOCK(&c->lock);
    if (c->nwaiters != 0) {
        NK_UNLOCK(&c->lock);
        return -EINVAL;
    }

    nk_thread_queue_destroy(c->wait_queue);
    NK_UNLOCK(&c->lock);
    memset(c, 0, sizeof(nk_condvar_t));
    return 0;
}


uint8_t
nk_condvar_wait (nk_condvar_t * c, NK_LOCK_T * l)
{
    NK_PROFILE_ENTRY();

    DEBUG_PRINT("Condvar wait on (%p) mutex=%p\n", (void*)c, (void*)l);

    NK_LOCK(&c->lock);

    /* now we can unlock the mutex and go to sleep */
    NK_UNLOCK(l);

    ++c->nwaiters;
    ++c->main_seq;

    unsigned long long val;
    unsigned long long seq;
    unsigned bc = *(volatile unsigned*)&(c->bcast_seq);
    val = seq = c->wakeup_seq;

    do {

        NK_UNLOCK(&c->lock);
        nk_thread_queue_sleep(c->wait_queue);
        NK_LOCK(&c->lock);

        if (bc != *(volatile unsigned*)&(c->bcast_seq)) {
            goto bcout;
        }

        val = *(volatile unsigned long long*)&(c->wakeup_seq);

    } while (val == seq || val == *(volatile unsigned long long*)&(c->woken_seq));

    ++c->woken_seq;

bcout:

    --c->nwaiters;

    NK_UNLOCK(&c->lock);

    /* reacquire lock */
    NK_LOCK(l);

    NK_PROFILE_EXIT();

    return 0;
}


int 
nk_condvar_signal (nk_condvar_t * c)
{
    uint64_t start, end;

    NK_PROFILE_ENTRY();

    NK_LOCK(&c->lock);

    // do we have anyone to signal?
    if (c->main_seq > c->wakeup_seq) {

        ++c->wakeup_seq;

        DEBUG_PRINT("Condvar signaling on (%p)\n", (void*)c);

        if (unlikely(nk_thread_queue_wake_one(c->wait_queue) != 0)) {
            ERROR_PRINT("Could not signal on condvar\n");
            return -1;
        }

    }

    NK_UNLOCK(&c->lock);
    NK_PROFILE_EXIT();
    return 0;
}


int
nk_condvar_bcast (nk_condvar_t * c)
{
    NK_PROFILE_ENTRY();

    NK_LOCK(&c->lock);

    // do we have anyone to wakeup?
    if (c->main_seq > c->wakeup_seq) {

        c->woken_seq = c->main_seq;
        c->wakeup_seq = c->main_seq;
        ++c->bcast_seq;

        NK_UNLOCK(&c->lock);

        DEBUG_PRINT("Condvar broadcasting on (%p) (core=%u)\n", (void*)c, my_cpu_id());
        if (unlikely(nk_thread_queue_wake_all(c->wait_queue) != 0)) {
            ERROR_PRINT("Could not broadcast on condvar\n");
            return -1;
        }
        return 0;

    }

    NK_UNLOCK(&c->lock);
    NK_PROFILE_EXIT();
    return 0;
}


static void 
test1 (void * in, void ** out) 
{
    nk_condvar_t * c = (nk_condvar_t*)in;
    NK_LOCK_T lock;
    NK_LOCK_INIT(&lock);
    printk("test 1 is starting to wait on condvar\n");
    NK_LOCK(&lock);

    nk_condvar_wait(c, &lock);

    NK_UNLOCK(&lock);

    printk("test one is out of wait on condvar\n");
}

static void 
test2 (void * in, void ** out) 
{
    nk_condvar_t * c = (nk_condvar_t*)in;
    NK_LOCK_T lock;
    NK_LOCK_INIT(&lock);
    printk("test 2 is starting to wait on condvar\n");
    NK_LOCK(&lock);

    nk_condvar_wait(c, &lock);

    NK_UNLOCK(&lock);

    printk("test 2 is out of wait on condvar\n");
}

static void
test3 (void * in, void ** out)
{
    unsigned long long n = 1024*1024*100;
    nk_condvar_t * c = (nk_condvar_t*)in;
    printk("test 3 signalling cond var\n");
    nk_condvar_signal(c);
    printk("test 3 signalled first time\n");

    while (--n){
        io_delay();
    }

    printk("test 3 signalling 2nd time\n");
    nk_condvar_signal(c);

    while (nk_condvar_destroy(c) < 0) {
        nk_yield();
    }
    free(c);
}


void 
nk_condvar_test (void)
{

    nk_condvar_t * c = malloc(sizeof(nk_condvar_t));
    if (!c) {
        ERROR_PRINT("Could not allocate condvar\n");
        return;
    }

    nk_condvar_init(c);

    nk_thread_start(test1, c, NULL, 1, TSTACK_DEFAULT, NULL, 1);
    nk_thread_start(test2, c, NULL, 1, TSTACK_DEFAULT, NULL, 2);
    nk_thread_start(test3, c, NULL, 1, TSTACK_DEFAULT, NULL, 3);

}


