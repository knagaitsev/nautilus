#ifndef __GIC_COMMON_H__
#define __GIC_COMMON_H__

#include<nautilus/naut_types.h>

/*
 * GICD Registers
 */

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

#define GICD_LOAD_REG(gicd, offset) *(volatile uint32_t*)((gicd)->dist_base + offset)
#define GICD_STORE_REG(gicd, offset, val) (*(volatile uint32_t*)((gicd)->dist_base + offset)) = val

#define GICD_BITMAP_SET(gicd, offset, num) \
  *(((volatile uint32_t*)((gicd)->dist_base + offset))+(num >> 5)) |= (1<<(num&(0x1F)))
#define GICD_BITMAP_READ(gicd, offset, num) \
  (!!(*(((volatile uint32_t*)((gicd)->dist_base + offset))+(num >> 5)) & (1<<(num&(0x1F)))))

#define GICD_DUALBITMAP_READ(gic, offset, num) \
  (*(((volatile uint8_t*)((gic)->dist_base + offset))+(num >> 4)) & (0x3<<((num&0xF)<<4)))

#define GICD_BYTEMAP_READ(gic, offset, num) \
  (*(volatile uint8_t*)((gic)->dist_base + offset + num))
#define GICD_BYTEMAP_WRITE(gic, offset, num, val) \
  *((volatile uint8_t*)((gic)->dist_base + offset + num)) = (uint8_t)val


/*
 * GICC Registers
 */

#define GICC_CTLR_OFFSET 0x0
#define GICC_PMR_OFFSET 0x4
#define GICC_BPR_OFFSET 0x8
#define GICC_IAR_OFFSET 0xC
#define GICC_EOIR_OFFSET 0x10
#define GICC_RPR_OFFSET 0x14
#define GICC_HPPIR_OFFSET 0x18
#define GICC_ABPR_OFFSET 0x1C
#define GICC_AIAR_OFFSET 0x20
#define GICC_AEOIR_OFFSET 0x24
#define GICC_AHPPIR_OFFSET 0x28
#define GICC_IIDR_OFFSET 0xFC
#define GICC_DIR_OFFSET 0x1000
#define GICC_APR_0_OFFSET 0xD0
#define GICC_NSAPR_0_OFFSET 0xE0

#define GICC_LOAD_REG(gic, offset) *(volatile uint32_t*)((gic)->cpu_base + offset)
#define GICC_STORE_REG(gic, offset, val) (*(volatile uint32_t*)((gic)->cpu_base + offset)) = val
#define GICC_BITMAP_SET(gic, num, offset) \
  *(((volatile uint32_t*)((gic)->cpu_base + offset))+(num >> 5)) |= (1<<(num&(0x1F)))
#define GICC_BITMAP_READ(gic, num, offset) \
  (!!(*(((volatile uint32_t*)((gic)->cpu_base + offset))+(num >> 5)) & (1<<(num&(0x1F)))))

/*
 * GICR Registers
 */

#define GICR_DEFAULT_STRIDE 0x20000
#define GICR_SGI_BASE 0x10000

#define GICR_CTLR_OFFSET 0x0
#define GICR_TYPER_OFFSET 0x8
#define GICR_PROPBASE_OFFSET 0x70
#define GICR_ISENABLER_0_OFFSET (GICR_SGI_BASE+0x100)
#define GICR_ICENABLER_0_OFFSET (GICR_SGI_BASE+0x180)
#define GICR_ISPENDR_0_OFFSET (GICR_SGI_BASE+0x200)
#define GICR_ICPENDR_0_OFFSET (GICR_SGI_BASE+0x280)
#define GICR_ISACTIVER_0_OFFSET (GICR_SGI_BASE+0x300)
#define GICR_ICACTIVER_0_OFFSET (GICR_SGI_BASE+0x380)

#define GICR_LOAD_REG32(gicr, offset) \
   (*(volatile uint32_t*)((gicr)->base + offset))
#define GICR_LOAD_REG64(gicr, offset) \
   (*(volatile uint64_t*)((gicr)->base + offset))
#define GICR_STORE_REG32(gicr, offset, val) \
   (*(volatile uint32_t*)((gicr)->base + offset) = (val))
#define GICR_STORE_REG64(gicr, offset, val) \
   (*(volatile uint64_t*)((gicr)->base + offset) = (val))

#define GICR_BITMAP_SET(gicr, offset, nr) \
  set_bit(nr, ((gicr)->base)+offset)
#define GICR_BITMAP_CLEAR(gicr, offset, nr) \
  clear_bit(nr, ((gicr)->base)+offset)
#define GICR_BITMAP_READ(gicr, offset, nr) \
  test_bit(nr, ((gicr)->base)+offset)

#endif
