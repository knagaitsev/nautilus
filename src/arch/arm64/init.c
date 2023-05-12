
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
#include<arch/arm64/psci.h>

#include<dev/pl011.h>
#include<dev/pci.h>

#ifndef NAUT_CONFIG_DEBUG_PRINTS
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

#define INIT_PRINT(fmt, args...) INFO_PRINT("init: " fmt, ##args)
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

  INIT_PRINT("Initialized the core barrier\n");

  return 0;
}

volatile static uint8_t __secondary_init_finish;

void secondary_init(uint64_t context_id) {

  fpu_init(&nautilus_info, 1);

  set_tpid_reg(&nautilus_info);

  INIT_PRINT("Starting CPU %u...\n", my_cpu_id());
 
  // Locally Initialize the GIC on the init processor 
  // (will need to call this on every processor which can receive interrupts)
  if(per_cpu_init_gic()) {
    INIT_ERROR("Failed to initialize CPU %u's GIC registers!\n", my_cpu_id());
    return;
  }
  INIT_PRINT("Initialized the GIC!\n", my_cpu_id());

  nk_sched_init_ap(&sched_cfg);

  // nk_sched_init_ap should have allocated us a stack, so we need to switch to it
  // (previously we should have been using the shared boot stack)
  smp_ap_stack_switch(get_cur_thread()->rsp, get_cur_thread()->rsp, &nautilus_info);
  nk_thread_name(get_cur_thread(), "secondary_init");

  INIT_PRINT("ARM64: successfully started CPU %d\n", my_cpu_id());

  __secondary_init_finish = 1;

  nk_sched_start();
  
  // Enable interrupts
  arch_enable_ints(); 

  INIT_PRINT("Interrupts are now enabled\n");
 
  //enable the timer
  percpu_timer_init();

  idle(NULL, NULL);
}

extern void secondary_start(void);
void *__secondary_stack = NULL;

static int start_secondaries(struct sys_info *sys) {
  INIT_PRINT("Starting secondary processors\n");

  for(uint64_t i = 1; i < sys->num_cpus; i++) {
    __secondary_init_finish = 0;
    // Initialize the stack
    __secondary_stack = (uint8_t*)malloc(2 * PAGE_SIZE_4KB);
    __secondary_stack += 2*PAGE_SIZE_4KB;

    INIT_PRINT("Trying to start secondary core: %u\n", i);
    if(psci_cpu_on(i, (void*)secondary_start, i)) {
      INIT_ERROR("PSCI Error: psci_cpu_on failed for CPU %u!\n", i);
    }
    while(!__secondary_init_finish) {
      // Wait for the secondary cpu to finish initializing
      // (We need to start them one by one because we're re-using
      //  the boot stack)
      //
      //  We could preallocate stacks for each core and let them use those instead
      //  of the boot stack, if this becomes a bottleneck when booting large MP systems.
    }
  }

  INIT_PRINT("All CPU's are initialized!\n");
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

  // Zero out .bss
  nk_low_level_memset(_bssStart, 0, (off_t)_bssEnd - (off_t)_bssStart);
  
  // Enable the FPU
  fpu_init(&nautilus_info, 0);

  nautilus_info.sys.dtb = (struct dtb_fdt_header*)dtb;

  mpid_reg_t mpid_reg;
  load_mpid_reg(&mpid_reg);
  nautilus_info.sys.bsp_id = mpid_reg.aff0; 
 
  // Initialize serial output
  pl011_uart_early_init(&_main_pl011_uart, (void*)QEMU_PL011_VIRT_BASE_ADDR, QEMU_VIRT_BASE_CLOCK);

  // Enable printk by handing it the uart
  extern struct pl011_uart *printk_uart;
  printk_uart = &_main_pl011_uart;

  printk(NAUT_WELCOME);

  INIT_PRINT("--- Device Tree ---\n");
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
  smp_early_init(&nautilus_info);

  // Set our thread pointer id register for the BSP
  set_tpid_reg(&nautilus_info);
  
  // Initialize NUMA (Fake)
  arch_numa_init(&(nautilus_info.sys));

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

  // Initialize the structures which hold info about 
  // interrupt/exception handlers and routing
  if(excp_init()) {
    INIT_ERROR("Failed to initialize the excp handler tables!\n");
    return;
  }

  // Start using the main kernel allocator
  //mm_dump_page_map();
  nk_kmem_init();
  mm_boot_kmem_init();

  // Now we should be able to install irq handlers

  pl011_uart_dev_init("serial0", &_main_pl011_uart);

  nk_wait_queue_init();
  nk_future_init();
  nk_timer_init();
  nk_rand_init(nautilus_info.sys.cpus[0]);
  nk_semaphore_init();
  nk_msg_queue_init();

  nk_sched_init(&sched_cfg);

  nk_thread_group_init();
  nk_group_sched_init();

  init_core_barrier(&(nautilus_info.sys));

  // Swap stacks (now we definitely cannot return from this function
  smp_ap_stack_switch(get_cur_thread()->rsp, get_cur_thread()->rsp, &nautilus_info);

  nk_thread_name(get_cur_thread(), "init");
  INIT_PRINT("Swapped to new stack\n");

  psci_init();

  pci_init(&nautilus_info);
  
  start_secondaries(&(nautilus_info.sys));
  
  // Start the scheduler
  nk_sched_start();  

  // Enable interrupts
  arch_enable_ints(); 

  INIT_PRINT("Interrupts are now enabled\n");
   
  //enable the timer
  percpu_timer_init();

  arch_set_timer(arch_realtime_to_cycles(sched_cfg.aperiodic_quantum));

  execute_threading(NULL);

  INIT_PRINT("BSP Idling forever\n");
  idle(NULL,NULL);
}


volatile static uint64_t count;

static void print_ones(void*, void**)
{
    while (count) {
      //INIT_DEBUG("1 : count = %u, preempt_disable_level = %u\n", count, nautilus_info.sys.cpus[0]->preempt_disable_level - 1);
    }

    INIT_PRINT("End of print_ones\n");
}

static void print_twos(void*, void**)
{
  while(count){
    //INIT_DEBUG("2 : count = %u\n", count);
  }
  INIT_PRINT("End of print_twos\n");
}

int execute_threading(char command[])
{
    nk_thread_id_t a, b;

    INIT_PRINT("print_ones = %p\n", (nk_thread_fun_t)print_ones);
    INIT_PRINT("print_twos = %p\n", (nk_thread_fun_t)print_twos);

    nk_thread_start((nk_thread_fun_t)print_ones, 0, 0, 0, 0, &a, -1);
    nk_thread_start((nk_thread_fun_t)print_twos, 0, 0, 0, 0, &b, -1);

    nk_thread_name(a, "print_ones");
    nk_thread_name(b, "print_twos");

    count = 10;
    while (count > 0) {
        count--;
        nk_yield();
    }
    INIT_PRINT("\n");
    nk_join(a, NULL);
    nk_join(b, NULL);

    return 0;
}
