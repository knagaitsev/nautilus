#include<nautilus/irq.h>
#include<nautilus/idt.h>

#include<arch/arm64/gic.h>
#include<arch/arm64/unimpl.h>
#include<arch/arm64/excp.h>

void arch_irq_enable(int irq) {
  gic_set_target_all(irq);
  gic_enable_int(irq);
}
void arch_irq_disable(int irq) {
  gic_disable_int(irq);
}

void arch_irq_install(int irq, 
                      int (*handler)(excp_entry_t *excp,
                                     ulong_t vector,
                                     void *state),
                      void *state) {
  excp_assign_irq_handler(irq, handler, state); 
  arch_irq_enable(irq);
}

void arch_irq_uninstall(int irq) {
  arch_irq_disable(irq);
  excp_remove_irq_handler(irq);
}

// I don't know what this function is meant to do (why is it different than enable?)
void
nk_unmask_irq (uint8_t irq)
{
  arch_irq_enable(irq);
}


int unhandled_irq_handler(excp_entry_t *entry, excp_vec_t vec, void *state) {
  INFO_PRINT("--- UNHANDLED INTERRUPT ---\n");
  INFO_PRINT("\texcp_vec = %x\n", vec);
  return 1;
}

int reserved_irq_handler(excp_entry_t *entry, excp_vec_t vec, void *state);

int arch_irq_find_and_reserve_range(uint64_t numentries, int aligned, uint64_t *first)
{
  ulong_t h, s;
  int i,j;

  if (numentries>32) { 
    return -1;
  }

  DEBUG_PRINT("[irq] trying to reserve msi irq range of size: %u, aligned=%u\n",numentries, aligned);

  for (i=gic_base_msi_irq();i<(gic_base_msi_irq() + gic_num_msi_irq());) {
    DEBUG_PRINT("[irq] checking spi %u for reservation\n", i);
    if (irq_handler_desc_table[i].handler==unhandled_irq_handler) { 
      for (j=0; 
	   (i+j)<(gic_max_irq()+1) && 
	     j<numentries;
	   j++) {
        if(irq_handler_desc_table[i+j].handler == unhandled_irq_handler) {
          DEBUG_PRINT("[irq] spi %u is free\n", i+j);
        } else {
          DEBUG_PRINT("[irq] spi %u is not free, cancelling check of spi %u\n", i+j, i);
          break;
        }
      }

      if (j==numentries) { 
	// found it!
	for (j=0;j<numentries;j++) { 
	  irq_handler_desc_table[i+j].handler=reserved_irq_handler;
	}
	*first=i;
	return 0;
      } else {
	if (aligned) { 
	  i=i+numentries;
	} else {
	  i=i+j;
	}
      }
    } else {
      i++;
    }
  }

  return -1;
}


