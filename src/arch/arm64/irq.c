#include <nautilus/irq.h>

#include<arch/arm64/gic.h>
#include<arch/arm64/unimpl.h>

gic_t __gic;

void arch_irq_enable(int irq) {
  gicd_enable_int(&__gic, irq);
}
void arch_irq_disable(int irq) {
  gicd_disable_int(&__gic, irq);
}

void arch_irq_install(int irq, int (*handler)(excp_entry_t *excp,
                                            ulong_t vector,
                                            void *state)) {
  ARM64_ERR_UNIMPL;  
}

void arch_irq_uninstall(int irq) {
  ARM64_ERR_UNIMPL;
}

void
nk_unmask_irq (uint8_t irq)
{
  ARM64_ERR_UNIMPL;
}

