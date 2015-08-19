#ifndef __CONDVAR_H__
#define __CONDVAR_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <nautilus/thread.h>

typedef struct nk_condvar {
    NK_LOCK_T lock;
    nk_thread_queue_t * wait_queue;
    unsigned nwaiters;
    unsigned long long wakeup_seq;
    unsigned long long woken_seq;
    unsigned long long main_seq;
    unsigned bcast_seq;
} nk_condvar_t;

int nk_condvar_init(nk_condvar_t * c);
int nk_condvar_destroy(nk_condvar_t * c);
uint8_t nk_condvar_wait(nk_condvar_t * c, NK_LOCK_T * l);
int nk_condvar_signal(nk_condvar_t * c);
int nk_condvar_bcast(nk_condvar_t * c);
void nk_condvar_test(void);

#ifdef __cplusplus
}
#endif

#endif
