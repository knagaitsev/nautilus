#ifndef __PLIC_H__
#define __PLIC_H__

#include <nautilus/nautilus.h>

void plic_init_hart(int hart);
void plic_init(unsigned long fdt, struct naut_info *naut);
inline int  plic_claim(void);
int plic_pending(void);
inline void plic_complete(int irq);
void plic_enable(int irq, int priority);
void plic_disable(int irq);
void plic_timer_handler(void);
void plic_dump(void);

#endif
