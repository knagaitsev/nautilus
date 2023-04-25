
#include<nautilus/idt.h>
#include<nautilus/irq.h>
#include<nautilus/printk.h>

struct __attribute__((packed)) excp_entry_info {
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
  uint64_t _zero;
  uint64_t frame_ptr;
  uint64_t link_ptr;
  uint64_t esr;
  uint64_t far;
};

void el1_to_el1_interrupt(struct excp_entry_info *info) {
  printk("EL1 to EL1 Interrupt: :( Not an error we just haven't implemented correct handling yet\n");
  while(1) {}
}

void el0_to_el1_interrupt(struct excp_entry_info *info) {
  printk("EL0 to EL1 Interrupt: :( Not an error we just haven't implemented correct handling yet\n");
  while(1) {}
}

// Double fault
void el1_to_el1_exception(struct excp_entry_info *info) {
  printk("EL1 to EL1 EXCEPTION: ESR = 0x%x, FAR = %p\n", info->esr, (void*)info->far);
  while(1) {}
}

// User-mode exception
void el0_to_el1_exception(struct excp_entry_info *info) {
  printk("EL0 to EL1 EXCEPTION: ESR = 0x%x, FAR = %p\n", info->esr, (void*)info->far);
  while(1) {}
}
