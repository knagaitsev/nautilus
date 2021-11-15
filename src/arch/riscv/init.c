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
 * Authors: Kyle C. Hale <kh@u.northwestern.edu>
 *          Peter Dinda <pdinda@northwestern.edu> (Gem5 variant)
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#define __NAUTILUS_MAIN__

#include <nautilus/nautilus.h>
#include <nautilus/paging.h>
#include <nautilus/barrier.h>
#include <nautilus/blkdev.h>
#include <nautilus/chardev.h>
#include <nautilus/cmdline.h>
#include <nautilus/cpu.h>
#include <nautilus/dev.h>
#include <nautilus/devicetree.h>
#include <nautilus/errno.h>
#include <nautilus/fs.h>
#include <nautilus/future.h>
#include <nautilus/gpudev.h>
#include <nautilus/group.h>
#include <nautilus/group_sched.h>
#include <nautilus/idle.h>
#include <nautilus/libccompat.h>
#include <nautilus/linker.h>
#include <nautilus/mb_utils.h>
#include <nautilus/mm.h>
#include <nautilus/msg_queue.h>
#include <nautilus/netdev.h>
#include <nautilus/percpu.h>
#include <nautilus/prog.h>
#include <nautilus/random.h>
#include <nautilus/semaphore.h>
#include <nautilus/smp.h>
#include <nautilus/spinlock.h>
#include <nautilus/task.h>
#include <nautilus/thread.h>
#include <nautilus/timer.h>
#include <nautilus/waitqueue.h>

#ifdef NAUT_CONFIG_ENABLE_REMOTE_DEBUGGING
#include <nautilus/gdb-stub.h>
#endif

#include <arch/riscv/plic.h>
#include <arch/riscv/sbi.h>
#include <arch/riscv/trap.h>
#include <arch/riscv/sifive.h>

#define QUANTUM_IN_NS (1000000000ULL / NAUT_CONFIG_HZ)

struct nk_sched_config sched_cfg = {
    .util_limit = NAUT_CONFIG_UTILIZATION_LIMIT * 10000ULL,  // convert percent to 10^-6 units
    .sporadic_reservation = NAUT_CONFIG_SPORADIC_RESERVATION * 10000ULL,  // ..
    .aperiodic_reservation = NAUT_CONFIG_APERIODIC_RESERVATION * 10000ULL,  // ..
    .aperiodic_quantum = QUANTUM_IN_NS,
    .aperiodic_default_priority = QUANTUM_IN_NS,
};

static int sysinfo_init(struct sys_info *sys) {
  sys->core_barrier = (nk_barrier_t *)malloc(sizeof(nk_barrier_t));
  if (!sys->core_barrier) {
    ERROR_PRINT("Could not allocate core barrier\n");
    return -1;
  }
  memset(sys->core_barrier, 0, sizeof(nk_barrier_t));

  if (nk_barrier_init(sys->core_barrier, sys->num_cpus) != 0) {
    ERROR_PRINT("Could not create core barrier\n");
    goto out_err;
  }

  return 0;

out_err:
  free(sys->core_barrier);
  return -EINVAL;
}

#define NAUT_WELCOME                                      \
  "Welcome to                                         \n" \
  "    _   __               __   _  __                \n" \
  "   / | / /____ _ __  __ / /_ (_)/ /__  __ _____    \n" \
  "  /  |/ // __ `// / / // __// // // / / // ___/    \n" \
  " / /|  // /_/ // /_/ // /_ / // // /_/ /(__  )     \n" \
  "/_/ |_/ \\__,_/ \\__,_/ \\__//_//_/ \\__,_//____/  \n" \
  "+===============================================+  \n" \
  " Kyle C. Hale (c) 2014 | Northwestern University   \n" \
  "+===============================================+  \n\n"

extern addr_t init_smp_boot;
extern uint64_t secondary_core_stack;
extern uint64_t _bssStart[];
extern uint64_t _bssEnd[];

extern struct naut_info *smp_ap_stack_switch(uint64_t, uint64_t,
					     struct naut_info *);

bool second_done = false;

void secondary_entry(int hartid) {
  printk("RISCV: hart %d started!\n", hartid);

  struct naut_info *naut = &nautilus_info;

  w_sscratch(r_tp());

  w_tp((uint64_t)naut->sys.cpus[hartid]);

  /* Initialize the platform level interrupt controller for this HART */
  plic_init_hart();

  // Write supervisor trap vector location
  trap_init_hart();

  /* set the timer with sbi :) */
  // sbi_set_timer(rv::get_time() + TICK_INTERVAL);

  second_done = true;

  sti();

  while (1) {
  }
}

int start_secondary(struct sys_info *sys) {
  for (int i = 0; i < NAUT_CONFIG_MAX_CPUS; i++) {
    if (i == my_cpu_id() || !sys->cpus[i] || !sys->cpus[i]->enabled) continue;

    secondary_core_stack = (uint64_t)malloc(2 * 4096);
    secondary_core_stack += 2 * 4096;

    second_done = false;
    __sync_synchronize();

    struct sbiret ret =
	sbi_call(SBI_EXT_HSM, SBI_EXT_HSM_HART_START, i, &init_smp_boot);
    if (ret.error != SBI_SUCCESS) {
      continue;
    }

    printk("RISCV: hart %d is trying to start hart %d\n", my_cpu_id(), i);

    while (second_done != true) {
      /*
      struct sbiret ret = sbi_call(SBI_EXT_HSM, SBI_EXT_HSM_HART_START, i);
      switch(ret.value) {
	      case SBI_HSM_HART_STATUS_STARTED:
		      printk("started\n");
		      break;
	      case SBI_HSM_HART_STATUS_STOPPED:
		      printk("stopped\n");
		      break;
	      case SBI_HSM_HART_STATUS_START_PENDING:
		      printk("start pending\n");
		      break;
	      case SBI_HSM_HART_STATUS_STOP_PENDING:
		      printk("stop pending\n");
		      break;
      }
      */
      __sync_synchronize();
    }

    printk("RISCV: hart %d successfully started hart %d\n", my_cpu_id(), i);
  }

  return 0;
}

void my_monitor_entry(void);

void init(unsigned long hartid, unsigned long fdt) {

  if (!fdt) panic("Invalid FDT: %p\n", fdt);

  nk_low_level_memset(_bssStart, 0, (off_t)_bssEnd - (off_t)_bssStart);

  // Get necessary information from SBI
  sbi_early_init();

  // M-Mode passes scratch struct through tp. Move it to sscratch
  w_sscratch(r_tp());

  // Zero out tp for now until cls is set up
  w_tp(0);

  struct naut_info *naut = &nautilus_info;
  nk_low_level_memset(naut, 0, sizeof(struct naut_info));

  nk_vc_print(NAUT_WELCOME);

  naut->sys.bsp_id = hartid;
  naut->sys.dtb = (struct dtb_fdt_header *)fdt;

  if (!dtb_parse(naut->sys.dtb)) {
    panic("Problem parsing devicetree header\n");
  }

  // We now have serial output without SBI
  sifive_uart_init();

  printk("RISCV: hart %d mvendorid: %llx\n", hartid, sbi_call(SBI_GET_MVENDORID).value);
  printk("RISCV: hart %d marchid:   %llx\n", hartid, sbi_call(SBI_GET_MARCHID).value);
  printk("RISCV: hart %d mimpid:    %llx\n", hartid, sbi_call(SBI_GET_MIMPID).value);

  nk_dev_init();
  nk_char_dev_init();
  nk_block_dev_init();
  nk_net_dev_init();
  nk_gpu_dev_init();

  // Setup the temporary boot-time allocator
  mm_boot_init(fdt);

  // Enumate CPUs and initialize them
  smp_early_init(naut);

  /* this will populate NUMA-related structures */
  arch_numa_init(&naut->sys);

  /* this will finish up the identity map */
  nk_paging_init(&(naut->sys.mem), fdt);

  // Setup the main kernel memory allocator
  nk_kmem_init();

  // Setup per-core area for BSP
  w_tp((uint64_t)naut->sys.cpus[hartid]);

  /* now we switch to the real kernel memory allocator, pages
   * allocated in the boot mem allocator are kept reserved */
  mm_boot_kmem_init();

  // Write supervisor trap vector location
  trap_init_hart();

  // Initialize platform level interrupt controller for this HART
  plic_init();

  plic_init_hart();

  /* from this point on, we can use percpu macros (even if the APs aren't up) */

  sbi_init();

  sysinfo_init(&(naut->sys));

  nk_wait_queue_init();

  nk_future_init();

  nk_timer_init();

  nk_rand_init(naut->sys.cpus[hartid]);

  nk_semaphore_init();

  nk_msg_queue_init();

  nk_sched_init(&sched_cfg);

  nk_thread_group_init();
  nk_group_sched_init();

  /* we now switch away from the boot-time stack */
  naut = smp_ap_stack_switch(get_cur_thread()->rsp, get_cur_thread()->rsp, naut);

  mm_boot_kmem_cleanup();

  sti();

  /* interrupts are now on */

  while(1) {
    my_monitor_entry();
  }

  /* set the timer with sbi :) */
  // sbi_set_timer(r_time() + TICK_INTERVAL);

  // start_secondary(&(naut->sys));

  // nk_sched_start();

  while (1);
}

/* Faking some vc stuff */

inline int nk_vc_is_active()
{
  return 0;
}

#include <nautilus/printk.h>
#include <stdarg.h>

int nk_vc_print(char *s)
{
    printk(s);
    return 0;
}

#define PRINT_MAX 1024

int nk_vc_printf(char *fmt, ...)
{
  char buf[PRINT_MAX];

  va_list args;
  int i;

  va_start(args, fmt);
  i=vsnprintf(buf,PRINT_MAX,fmt,args);
  va_end(args);
  nk_vc_print(buf);
  return i;
}

int nk_vc_log(char *fmt, ...)
{
  char buf[PRINT_MAX];

  va_list args;
  int i;
  
  va_start(args, fmt);
  i=vsnprintf(buf,PRINT_MAX,fmt,args);
  va_end(args);
  
  return i;
}
