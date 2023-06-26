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

struct nk_regs {
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
  uint64_t sp;
  uint64_t frame_ptr;
  uint64_t link_ptr;
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

static inline uint8_t inb(uint64_t addr) {
  return *(volatile uint8_t*)addr;
}

static inline uint16_t inw(uint64_t addr) {
  return *(volatile uint16_t*)addr;
}

static inline uint32_t inl(uint64_t addr) {
  return *(volatile uint32_t*)addr;
}

static inline void outb(uint8_t val, uint64_t addr) {
  *(volatile uint8_t*)addr = val;
}

static inline void outw(uint16_t val, uint64_t addr) {
  *(volatile uint16_t*)addr = val;
}

static inline void outl(uint32_t val, uint64_t addr) {
  *(volatile uint64_t*)addr = val;
}

/* Status register flags */
#endif
