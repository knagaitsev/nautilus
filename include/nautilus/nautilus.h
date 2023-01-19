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
#ifndef __NAUTILUS_H__
#define __NAUTILUS_H__

typedef enum {UNCOND, IF_EARLIER, IF_LATER} nk_timer_condition_t;

#include <nautilus/percpu.h>
#include <nautilus/printk.h>
#include <dev/serial.h>
#include <nautilus/naut_types.h>
#include <nautilus/instrument.h>
#include <nautilus/smp.h>
#include <nautilus/thread.h>
#include <nautilus/vc.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DEBUG_PRINT(fmt, args...)					\
do {									\
    if (__cpu_state_get_cpu()) {					\
	int _p=preempt_is_disabled();					\
	preempt_disable();						\
	struct nk_thread *_t = get_cur_thread();				\
 	nk_vc_log_wrap("CPU %d (%s%s %lu \"%s\"): DEBUG: " fmt,		\
		       my_cpu_id(),					\
		       in_interrupt_context() ? "I" :"",		\
		       _p ? "" : "P",					\
		       _t ? _t->tid : 0,				\
		       _t ? _t->is_idle ? "*idle*" : _t->name[0]==0 ? "*unnamed*" : _t->name : "*none*", \
		       ##args);						\
	preempt_enable();						\
    } else {								\
	int _p=preempt_is_disabled();					\
	preempt_disable();						\
 	nk_vc_log_wrap("CPU ? (%s%s): DEBUG: " fmt,			\
		       in_interrupt_context() ? "I" :"",		\
		       _p ? "" : "P",					\
		       ##args);						\
	preempt_enable();						\
    }									\
} while (0)

#define ERROR_PRINT(fmt, args...)					\
do {									\
    if (__cpu_state_get_cpu()) {					\
	int _p=preempt_is_disabled();					\
	preempt_disable();						\
	struct nk_thread *_t = get_cur_thread();				\
 	nk_vc_log_wrap("CPU %d (%s%s %lu \"%s\"): ERROR at %s(%lu): " fmt,		\
		       my_cpu_id(),					\
		       in_interrupt_context() ? "I" :"",		\
		       _p ? "" : "P",					\
		       _t ? _t->tid : 0,					\
		       _t ? _t->is_idle ? "*idle*" : _t->name[0]==0 ? "*unnamed*" : _t->name : "*none*", \
	               __FILE__,__LINE__,                               \
		       ##args);						\
	preempt_enable();						\
    } else {								\
	int _p=preempt_is_disabled();					\
	preempt_disable();						\
 	nk_vc_log_wrap("CPU ? (%s%s): ERROR at %s(%lu): " fmt,			\
		       in_interrupt_context() ? "I" :"",		\
		       _p ? "" : "P",					\
                       __FILE__,__LINE__,                               \
		       ##args);						\
	preempt_enable();						\
    }									\
} while (0)


#define WARN_PRINT(fmt, args...)					\
do {									\
    if (__cpu_state_get_cpu()) {					\
	int _p=preempt_is_disabled();					\
	preempt_disable();						\
	struct nk_thread *_t = get_cur_thread();				\
 	nk_vc_log_wrap("CPU %d (%s%s %lu \"%s\"): WARNING : " fmt,	        \
		       my_cpu_id(),					\
		       in_interrupt_context() ? "I" :"",		\
		       _p ? "" : "P",					\
		       _t ? _t->tid : 0,					\
		       _t ? _t->is_idle ? "*idle*" : _t->name[0]==0 ? "*unnamed*" : _t->name : "*none*", \
		       ##args);						\
	preempt_enable();						\
    } else {								\
	int _p=preempt_is_disabled();					\
	preempt_disable();						\
 	nk_vc_log_wrap("CPU ? (%s%s): WARNING: " fmt,			\
		       in_interrupt_context() ? "I" :"",		\
		       _p ? "" : "P",					\
		       ##args);						\
	preempt_enable();						\
    }									\
} while (0)

#define INFO_PRINT(fmt, args...)					\
do {									\
    if (__cpu_state_get_cpu()) {					\
	int _p=preempt_is_disabled();					\
	preempt_disable();						\
	struct nk_thread *_t = get_cur_thread();				\
 	nk_vc_log_wrap("CPU %d (%s%s %lu \"%s\"): " fmt,	        \
		       my_cpu_id(),					\
		       in_interrupt_context() ? "I" :"",		\
		       _p ? "" : "P",					\
		       _t ? _t->tid : 0,					\
		       _t ? _t->is_idle ? "*idle*" : _t->name[0]==0 ? "*unnamed*" : _t->name : "*none*", \
		       ##args);						\
	preempt_enable();						\
    } else {								\
	int _p=preempt_is_disabled();					\
	preempt_disable();						\
 	nk_vc_log_wrap("CPU ? (%s%s): " fmt,       			\
		       in_interrupt_context() ? "I" :"",		\
		       _p ? "" : "P",					\
		       ##args);						\
	preempt_enable();						\
    }									\
} while (0)

void panic(const char *, ...) __attribute__((noreturn));

#define panic(fmt, args...)         panic("PANIC at %s(%d): " fmt, __FILE__, __LINE__, ##args)


#ifndef NAUT_CONFIG_DEBUG_PRINTS
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif


#include <dev/ioapic.h>
#include <nautilus/smp.h>
#include <nautilus/paging.h>
#include <nautilus/limits.h>
#include <nautilus/naut_assert.h>
#include <nautilus/barrier.h>
#include <nautilus/list.h>
#include <nautilus/numa.h>


struct ioapic;
struct irq_mapping {
    struct ioapic * ioapic;
    uint8_t vector;
    uint8_t assigned;
};


struct nk_int_info {
    struct list_head int_list; /* list of interrupts from MP spec */
    struct list_head bus_list; /* list of buses from MP spec */

    struct irq_mapping irq_map[256];
};

struct hpet_dev;
struct nk_locality_info;
struct pmc_info;
struct nk_link_info;
struct nk_prog_info;
struct sys_info {

    struct cpu * cpus[NAUT_CONFIG_MAX_CPUS];
    struct ioapic * ioapics[NAUT_CONFIG_MAX_IOAPICS];

    uint32_t num_cpus;
    uint32_t num_ioapics;

    uint64_t flags;
#define NK_SYS_LEGACY 1  // system has dual PIC and ISA devices

    nk_barrier_t * core_barrier;

    struct nk_mem_info mem;

    uint32_t bsp_id;

    uint8_t pic_mode_enabled;

    struct pci_info * pci;
    struct hpet_dev * hpet;

    struct multiboot_info * mb_info;

    struct nk_int_info int_info;

    struct nk_locality_info locality_info;

    struct pmc_info * pmc_info;

    struct nk_link_info * linker_info;

    struct nk_prog_info * prog_info;

    struct dtb_fdt_header * dtb; /* Device tree binary */

};

struct cmdline_state;
struct nk_test_harness;
struct naut_info {
    struct sys_info sys;
    struct cmdline_state * cmdline;
    struct nk_test_harness * test_info;

};

#ifdef __NAUTILUS_MAIN__
struct naut_info nautilus_info;
#else
extern struct naut_info nautilus_info;
#endif

static inline struct naut_info*
nk_get_nautilus_info (void)
{
    return &nautilus_info;
}

static inline double __math_invalid(double x) {
  return (x - x) / (x - x);
}

/* returns a*b*2^-32 - e, with error 0 <= e < 1.  */
static inline uint32_t mul32(uint32_t a, uint32_t b) { return (uint64_t)a * b >> 32; }

/* returns a*b*2^-64 - e, with error 0 <= e < 3.  */
static inline uint64_t mul64(uint64_t a, uint64_t b) {
	uint64_t ahi = a >> 32;
	uint64_t alo = a & 0xffffffff;
	uint64_t bhi = b >> 32;
	uint64_t blo = b & 0xffffffff;
	return ahi * bhi + (ahi * blo >> 32) + (alo * bhi >> 32);
}


static const uint16_t __rsqrt_tab[128] = {
    0xb451,
    0xb2f0,
    0xb196,
    0xb044,
    0xaef9,
    0xadb6,
    0xac79,
    0xab43,
    0xaa14,
    0xa8eb,
    0xa7c8,
    0xa6aa,
    0xa592,
    0xa480,
    0xa373,
    0xa26b,
    0xa168,
    0xa06a,
    0x9f70,
    0x9e7b,
    0x9d8a,
    0x9c9d,
    0x9bb5,
    0x9ad1,
    0x99f0,
    0x9913,
    0x983a,
    0x9765,
    0x9693,
    0x95c4,
    0x94f8,
    0x9430,
    0x936b,
    0x92a9,
    0x91ea,
    0x912e,
    0x9075,
    0x8fbe,
    0x8f0a,
    0x8e59,
    0x8daa,
    0x8cfe,
    0x8c54,
    0x8bac,
    0x8b07,
    0x8a64,
    0x89c4,
    0x8925,
    0x8889,
    0x87ee,
    0x8756,
    0x86c0,
    0x862b,
    0x8599,
    0x8508,
    0x8479,
    0x83ec,
    0x8361,
    0x82d8,
    0x8250,
    0x81c9,
    0x8145,
    0x80c2,
    0x8040,
    0xff02,
    0xfd0e,
    0xfb25,
    0xf947,
    0xf773,
    0xf5aa,
    0xf3ea,
    0xf234,
    0xf087,
    0xeee3,
    0xed47,
    0xebb3,
    0xea27,
    0xe8a3,
    0xe727,
    0xe5b2,
    0xe443,
    0xe2dc,
    0xe17a,
    0xe020,
    0xdecb,
    0xdd7d,
    0xdc34,
    0xdaf1,
    0xd9b3,
    0xd87b,
    0xd748,
    0xd61a,
    0xd4f1,
    0xd3cd,
    0xd2ad,
    0xd192,
    0xd07b,
    0xcf69,
    0xce5b,
    0xcd51,
    0xcc4a,
    0xcb48,
    0xca4a,
    0xc94f,
    0xc858,
    0xc764,
    0xc674,
    0xc587,
    0xc49d,
    0xc3b7,
    0xc2d4,
    0xc1f4,
    0xc116,
    0xc03c,
    0xbf65,
    0xbe90,
    0xbdbe,
    0xbcef,
    0xbc23,
    0xbb59,
    0xba91,
    0xb9cc,
    0xb90a,
    0xb84a,
    0xb78c,
    0xb6d0,
    0xb617,
    0xb560,
};

static double sqrt(double x) {
  uint64_t ix, top, m;

	/* special case handling.  */
	ix = (uint64_t)x;
	top = ix >> 52;
	if (unlikely(top - 0x001 >= 0x7ff - 0x001)) {
		/* x < 0x1p-1022 or inf or nan.  */
		if (ix * 2 == 0) return x;
		if (ix == 0x7ff0000000000000) return x;
		if (ix > 0x7ff0000000000000) return __math_invalid(x);
		/* x is subnormal, normalize it.  */
		ix = (uint64_t)(x * 0x1p52);
		top = ix >> 52;
		top -= 52;
	}
	int even = top & 1;
	m = (ix << 11) | 0x8000000000000000;
	if (even) m >>= 1;
	top = (top + 0x3ff) >> 1;
	static const uint64_t three = 0xc0000000;
	uint64_t r, s, d, u, i;

	i = (ix >> 46) % 128;
	r = (uint32_t)__rsqrt_tab[i] << 16;
	/* |r sqrt(m) - 1| < 0x1.fdp-9 */
	s = mul32(m >> 32, r);
	/* |s/sqrt(m) - 1| < 0x1.fdp-9 */
	d = mul32(s, r);
	u = three - d;
	r = mul32(r, u) << 1;
	/* |r sqrt(m) - 1| < 0x1.7bp-16 */
	s = mul32(s, u) << 1;
	/* |s/sqrt(m) - 1| < 0x1.7bp-16 */
	d = mul32(s, r);
	u = three - d;
	r = mul32(r, u) << 1;
	/* |r sqrt(m) - 1| < 0x1.3704p-29 (measured worst-case) */
	r = r << 32;
	s = mul64(m, r);
	d = mul64(s, r);
	u = (three << 32) - d;
	s = mul64(s, u); /* repr: 3.61 */
	/* -0x1p-57 < s - sqrt(m) < 0x1.8001p-61 */
	s = (s - 2) >> 9; /* repr: 12.52 */
	uint64_t d0, d1, d2;
	double y, t;
	d0 = (m << 42) - s * s;
	d1 = s - d0;
	d2 = d1 + s + 1;
	s += d1 >> 63;
	s &= 0x000fffffffffffff;
	s |= top << 52;
	y = (double)(s);
  	return y;
}



#include <nautilus/arch.h>
#ifdef NAUT_CONFIG_XEON_PHI
#include <arch/k1om/main.h>
#elif defined NAUT_CONFIG_HVM_HRT
#include <arch/hrt/main.h>
#elif defined NAUT_CONFIG_X86_64_HOST
#include <arch/x64/main.h>
#elif defined NAUT_CONFIG_GEM5
#include <arch/gem5/main.h>
#elif defined NAUT_CONFIG_RISCV_HOST
#include <arch/riscv/main.h>
#else
#error "Unsupported architecture"
#endif

#ifdef __cplusplus
}
#endif
                                               

#endif
