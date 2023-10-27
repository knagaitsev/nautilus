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
 * Copyright (c) 2023, Nick Wanninger <ncw@u.northwestern.edu>
 * Copyright (c) 2015, The V3VEE Project  <http://www.v3vee.org>
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Nick Wanninger <ncw@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#ifdef __CPU_H__

struct nk_regs 
{
  uint64_t x0;
  uint64_t x1;
  uint64_t x2;
  uint64_t x3;
  uint64_t x4;
  uint64_t x5;
  uint64_t x6;
  uint64_t x7;
  uint64_t x8;
  uint64_t x9;
  uint64_t x10;
  uint64_t x11;
  uint64_t x12;
  uint64_t x13;
  uint64_t x14;
  uint64_t x15;
  uint64_t x16;
  uint64_t x17;
  uint64_t x18;
  uint64_t frame_ptr;
  uint64_t link_ptr;
  uint64_t sp;
  
  // Extended registers, only exist on the stack of 
  // a thread which isn't being run or an unhandled exception
  uint64_t x19;
  uint64_t x20;
  uint64_t x21;
  uint64_t x22;
  uint64_t x23;
  uint64_t x24;
  uint64_t x25;
  uint64_t x26;
  uint64_t x27;
  uint64_t x28;
};

#define pause()

#define PAUSE_WHILE(x)                                                         \
  while ((x)) {                                                                \
    pause();                                                                   \
  }

#define mbarrier() __sync_synchronize()

#define BARRIER_WHILE(x)                                                       \
  while ((x)) {                                                                \
    mbarrier();                                                                \
  }

static inline void halt(void) {
  asm volatile ("wfi");
}

static inline void invlpg(unsigned long addr) {}

static inline void wbinvd(void) {}

static inline void clflush(void *ptr) {}

static inline void clflush_unaligned(void *ptr, int size) {}

/**
 * Flush all non-global entries in the calling CPU's TLB.
 *
 * Flushing non-global entries is the common-case since user-space
 * does not use global pages (i.e., pages mapped at the same virtual
 * address in *all* processes).
 *
 */
static inline void tlb_flush(void) { /* TODO(arm64) */ }

static inline void io_delay(void) {
  /* TODO(arm64) */
  pause();
}

static void udelay(uint_t n) {
  while (n--) {
    io_delay();
  }
}
/* Status register flags */
#endif
