#ifndef __GIC_COMMON_H__
#define __GIC_COMMON_H__

#include<nautilus/naut_types.h>

// Distributor Register Offsets
#define GICD_CTLR_OFFSET 0x0
#define GICD_TYPER_OFFSET 0x4
#define GICD_IIDR_OFFSET 0x8
#define GICD_SGIR_OFFSET 0xF00
#define GICD_IGROUPR_0_OFFSET 0x80
#define GICD_ISENABLER_0_OFFSET 0x100
#define GICD_ICENABLER_0_OFFSET 0x180
#define GICD_ISPENDR_0_OFFSET 0x200
#define GICD_ICPENDR_0_OFFSET 0x280
#define GICD_ISACTIVER_0_OFFSET 0x300
#define GICD_ICACTIVER_0_OFFSET 0x380
#define GICD_IPRIORITYR_0_OFFSET 0x400
#define GICD_ITARGETSR_0_OFFSET 0x800
#define GICD_ICFGR_0_OFFSET 0xC00
#define GICD_NSACR_0_OFFSET 0xE00
#define GICD_CPENDSGIR_0_OFFSET 0xF10
#define GICD_SPENDSGIR_0_OFFSET 0xF20

#define LOAD_GICD_REG(gic, offset) *(volatile uint32_t*)((gic).dist_base + offset)
#define STORE_GICD_REG(gic, offset, val) (*(volatile uint32_t*)((gic).dist_base + offset)) = val
#define GICD_BITMAP_SET(gic, num, offset) \
  *(((volatile uint32_t*)((gic).dist_base + offset))+(num >> 5)) |= (1<<(num&(0x1F)))
#define GICD_BITMAP_READ(gic, num, offset) \
  (!!(*(((volatile uint32_t*)((gic).dist_base + offset))+(num >> 5)) & (1<<(num&(0x1F)))))

struct gic_msi_frame {

  uint64_t mmio_base;
  
  uint32_t base_irq;
  uint32_t num_irq;
  
  struct gic_msi_frame *next;

};


#endif
