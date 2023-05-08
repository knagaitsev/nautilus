
#define __NAUTILUS_MAIN__

#include<nautilus/nautilus.h>
#include<nautilus/printk.h>
#include<nautilus/fpu.h>
#include<nautilus/naut_string.h>
#include<nautilus/fdt.h>
#include<nautilus/arch.h>
#include<nautilus/dev.h>
#include<nautilus/chardev.h>
#include<nautilus/blkdev.h>
#include<nautilus/netdev.h>
#include<nautilus/gpudev.h>
#include<nautilus/arch.h>
#include<nautilus/irq.h>
#include<nautilus/idt.h>
#include<nautilus/mm.h>
#include<nautilus/waitqueue.h>
#include<nautilus/future.h>
#include<nautilus/timer.h>
#include<nautilus/random.h>
#include<nautilus/semaphore.h>
#include<nautilus/msg_queue.h>
#include<nautilus/group_sched.h>
#include<nautilus/idle.h>
#include<nautilus/barrier.h>
#include<nautilus/smp.h>

#include<arch/arm64/unimpl.h>
#include<arch/arm64/gic.h>
#include<arch/arm64/sys_reg.h>
#include<arch/arm64/excp.h>
#include<arch/arm64/timer.h>

#include<dev/pl011.h>

#ifndef NAUT_CONFIG_DEBUG_PRINTS
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

#define INIT_PRINT(fmt, args...) printk("init: " fmt, ##args)
#define INIT_DEBUG(fmt, args...) DEBUG_PRINT("init: " fmt, ##args)
#define INIT_ERROR(fmt, args...) ERROR_PRINT("init: " fmt, ##args)
#define INIT_WARN(fmt, args...) WARN_PRINT("init: " fmt, ##args)

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

#define QEMU_PL011_VIRT_BASE_ADDR 0x9000000
#define QEMU_VIRT_BASE_CLOCK 24000000 // 24 MHz Clock

struct pl011_uart _main_pl011_uart;
struct gic _main_gic;

#define QUANTUM_IN_NS (1000000000ULL / NAUT_CONFIG_HZ)
struct nk_sched_config sched_cfg = {
    .util_limit = NAUT_CONFIG_UTILIZATION_LIMIT * 10000ULL,  // convert percent to 10^-6 units
    .sporadic_reservation = NAUT_CONFIG_SPORADIC_RESERVATION * 10000ULL,  // ..
    .aperiodic_reservation = NAUT_CONFIG_APERIODIC_RESERVATION * 10000ULL,  // ..
    .aperiodic_quantum = QUANTUM_IN_NS,
    .aperiodic_default_priority = QUANTUM_IN_NS,
};

static inline void set_tpid_reg(struct naut_info *naut) {
  mpid_reg_t mpid_reg;
  load_mpid_reg(&mpid_reg);
  STORE_SYS_REG(TPIDR_EL1, naut->sys.cpus[mpid_reg.aff0]);
  INIT_DEBUG("Wrote into thread pointer register of CPU: %u, ptr = %p\n", mpid_reg.aff0, naut->sys.cpus[mpid_reg.aff0]);
}

static inline int init_core_barrier(struct sys_info *sys) {
  sys->core_barrier = (nk_barrier_t *)malloc(sizeof(nk_barrier_t));
  if (!sys->core_barrier) {
    ERROR_PRINT("Could not allocate core barrier\n");
    return -1;
  }
  memset(sys->core_barrier, 0, sizeof(nk_barrier_t));

  if (nk_barrier_init(sys->core_barrier, sys->num_cpus) != 0) {
    ERROR_PRINT("Could not create core barrier\n");
    return -1;
  }

  DEBUG_PRINT("Initialized the core barrier\n");

  return 0;
}

void secondary_entry(void) {

  struct naut_info *naut = &nautilus_info;

  set_tpid_reg(naut);

  DEBUG_PRINT("Starting CPU: %d\n", my_cpu_id());

  // Initialize the stack
  uint64_t *stack = (uint64_t)malloc(2 * 4096);
  stack += 2 * 4096;
  get_cur_thread()->rsp = stack;
  naut = smp_ap_stack_switch(get_cur_thread()->rsp, get_cur_thread()->rsp, naut);

  DEBUG_PRINT("ARM64: successfully started CPU %d\n", my_cpu_id());

  nk_core_barrier_arrive(); 

  idle(NULL, NULL);
}

static int start_secondaries(struct sys_info *sys) {
  DEBUG_PRINT("Starting secondary processors\n");

  asm volatile ("sev");

  nk_core_barrier_raise();
  nk_core_barrier_wait(); 

  DEBUG_PRINT("All CPU's are initialized!\n");
}


/* Faking some vc stuff */

uint16_t
vga_make_entry (char c, uint8_t color)
{
    uint16_t c16 = c;
    uint16_t color16 = color;
    return c16 | color16 << 8;
}
// (I want to stick this somewhere else later on (or make it not needed))
//


extern void *_bssEnd;
extern void *_bssStart;

void init(unsigned long dtb, unsigned long x1, unsigned long x2, unsigned long x3) {

  struct naut_info *naut = &nautilus_info;

  // Zero out .bss
  nk_low_level_memset(_bssStart, 0, (off_t)_bssEnd - (off_t)_bssStart);
  
  // Enable the FPU
  fpu_init(naut, 0);

  naut->sys.dtb = (struct dtb_fdt_header*)dtb;

  mpid_reg_t mpid_reg;
  load_mpid_reg(&mpid_reg);
  naut->sys.bsp_id = mpid_reg.aff0; 
 
  // Initialize serial output
  pl011_uart_init(&_main_pl011_uart, (void*)QEMU_PL011_VIRT_BASE_ADDR, QEMU_VIRT_BASE_CLOCK);

  // Enable printk by handing it the uart
  extern struct pl011_uart *printk_uart;
  printk_uart = &_main_pl011_uart;

  printk(NAUT_WELCOME);

  printk("--- Device Tree ---\n");
  print_fdt((void*)dtb);

  // Init devices (these don't seem to do anything for now)
  nk_dev_init();
  nk_char_dev_init();
  nk_block_dev_init();
  nk_net_dev_init();
  nk_gpu_dev_init();

  // Setup the temporary boot-time allocator
  mm_boot_init(dtb);

  // Initialize SMP using the dtb
  smp_early_init(naut);

  // Set our thread pointer id register for the BSP
  set_tpid_reg(naut);
  
  // Initialize NUMA (Fake)
  arch_numa_init(&naut->sys);

  // Initialize (but not enable) the Generic Interrupt Controller
  if(global_init_gic((void*)dtb, &_main_gic)) {
    INIT_ERROR("Failed to globally initialize the GIC!\n");
    return;
  }
  
  // Locally Initialize the GIC on the init processor 
  // (will need to call this on every processor which can receive interrupts)
  if(per_cpu_init_gic()) {
    INIT_ERROR("Failed to initialize the init processor's GIC registers!\n");
    return;
  }

  print_gic();

  // Initialize the structures which hold info about 
  // interrupt/exception handlers and routing
  if(excp_init()) {
    INIT_ERROR("Failed to initialize the excp handler tables!\n");
    return;
  }

  // Start using the main kernel allocator
  mm_dump_page_map();
  nk_kmem_init();
  mm_boot_kmem_init();

  // Now we should be able to install irq handlers

  nk_wait_queue_init();
  nk_future_init();
  nk_timer_init();
  nk_rand_init(naut->sys.cpus[0]);
  nk_semaphore_init();
  nk_msg_queue_init();

  nk_sched_init(&sched_cfg);

  nk_thread_group_init();
  nk_group_sched_init();

  init_core_barrier(&(naut->sys));

  // Swap stacks (now we definitely cannot return from this function
  naut = smp_ap_stack_switch(get_cur_thread()->rsp, get_cur_thread()->rsp, naut);

  nk_thread_name(get_cur_thread(), "init");
  
  //start_secondaries(&(naut->sys));

  // Start the scheduler
  nk_sched_start();
  
  // Enable interrupts
  arch_enable_ints(); 

  INIT_DEBUG("Interrupts are now enabled\n");
  
  //enable the timer
  percpu_timer_init();

  INIT_DEBUG("About to set timer\n");

  arch_set_timer(1000);

  INIT_DEBUG("Timer set\n");

/* 
  while(1){
    for(volatile uint64_t i = 0; i < 100000000; i++) {}
    INIT_DEBUG("1, tval = %d, cval = %u, pcnt = %u, ctl = 0x%x, DAIF = 0x%x, preempt_dis_level = 0x%x\n", 
          arch_read_timer(),
          ({uint64_t cval; asm volatile ("mrs %0, CNTP_CVAL_EL0" : "=r" (cval)); cval;}),
          ({uint64_t pcnt; asm volatile ("mrs %0, CNTPCT_EL0" : "=r" (pcnt)); pcnt;}),
          ({uint64_t ctl; asm volatile ("mrs %0, CNTP_CTL_EL0" : "=r" (ctl)); ctl;}),
          ({uint64_t daif; asm volatile ("mrs %0, DAIF" : "=r" (daif)); daif;}),
          per_cpu_get(preempt_disable_level)
        );  
    print_gic();
  }
 */
  execute_threading(NULL);

  INIT_DEBUG("End of current boot process!\n");

  idle(NULL,NULL);
}


volatile static bool_t done = false;

static void print_ones(void*, void**)
{
    while (!done) {
      INIT_DEBUG("1\n");
    }

    INIT_DEBUG("End of print_ones\n");
}

static void print_twos(void*, void**)
{
    while (!done) {
      INIT_DEBUG("2\n");
    }

    INIT_DEBUG("End of print_twos\n");
}

int execute_threading(char command[])
{
    nk_thread_id_t a, b;

    printk("print_ones = %p\n", (nk_thread_fun_t)print_ones);
    printk("print_twos = %p\n", (nk_thread_fun_t)print_twos);

    nk_thread_start((nk_thread_fun_t)print_ones, 0, 0, 0, 0, &a, my_cpu_id());
    nk_thread_start((nk_thread_fun_t)print_twos, 0, 0, 0, 0, &b, my_cpu_id());

    nk_thread_name(a, "print_ones");
    nk_thread_name(b, "print_twos");

    done = false;
    int i = 0;
    while (i < 10) {
        i++;
        nk_yield();
    }
    printk("\n");
    done = true;
    nk_join(a, NULL);
    nk_join(b, NULL);

    return 0;
}
