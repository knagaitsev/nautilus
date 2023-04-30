
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

#include<arch/arm64/unimpl.h>
#include<arch/arm64/gic.h>
#include<arch/arm64/sys_reg.h>
#include<arch/arm64/excp.h>

#include<dev/pl011.h>

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

uint64_t unsafe_counter = 0;
int timer_interrupt_handler(excp_entry_t *info, excp_vec_t vec, void *state) {
  printk("TIMER: %u\n", unsafe_counter);
  unsafe_counter += 1;
  asm volatile (
    "mrs x1, CNTFRQ_EL0;"
    "msr CNTP_TVAL_EL0, x1;");
  return 0;
}

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

  // Initialize NUMA (Fake)
  arch_numa_init(&naut->sys);

  // Initialize (but not enable) the Generic Interrupt Controller
  if(global_init_gic((void*)dtb, &_main_gic)) {
    printk("Failed to globally initialize the GIC!\n");
    return;
  }
  
  // Locally Initialize the GIC on the init processor 
  // (will need to call this on every processor which can receive interrupts)
  if(per_cpu_init_gic()) {
    printk("Failed to initialize the init processor's GIC registers!\n");
    return;
  }

  print_gic();

  // Initialize the structures which hold info about 
  // interrupt/exception handlers and routing
  if(excp_init()) {
    printk("Failed to initialize the excp handler tables!\n");
    return;
  }

  // Start using the main kernel allocator
  nk_kmem_init();
  mm_boot_kmem_init();

  // Now we should be able to install irq handlers
  arch_irq_install(30, timer_interrupt_handler);

  // Enable interrupts
  arch_enable_ints();
  
  printk("Interrupts are now enabled\n");

  nk_wait_queue_init();
  nk_future_init();
  nk_timer_init();
  nk_rand_init(naut->sys.cpus[0]);
  nk_semaphore_init();
  nk_msg_queue_init();

  //nk_sched_init(&sched_cfg);

  printk("End of current boot process\n");
}
