#include<nautilus/irq.h>
#include<nautilus/idt.h>

#include<arch/arm64/gic.h>
#include<arch/arm64/unimpl.h>

extern gic_t *__gic_ptr;

void arch_irq_enable(int irq) {
  gicd_enable_int(__gic_ptr, irq);
}
void arch_irq_disable(int irq) {
  gicd_disable_int(__gic_ptr, irq);
}

void arch_irq_install(int irq, int (*handler)(excp_entry_t *excp,
                                            ulong_t vector,
                                            void *state)) {
  // Why do these function not pass the state?
  idt_assign_entry(irq, handler, NULL); 
  arch_irq_enable(irq);
}

void arch_irq_uninstall(int irq) {
  arch_irq_disable(irq);
  idt_assign_entry(irq, null_irq_handler, NULL);
}

// I don't know what  this function is meant to do
void
nk_unmask_irq (uint8_t irq)
{
  ARM64_ERR_UNIMPL;
}

