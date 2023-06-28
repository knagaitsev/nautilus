#ifndef __ARM64_GIC_H__
#define __ARM64_GIC_H__

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

typedef struct gic_int_info {
  uint32_t int_id;
  uint32_t cpu_id;
  int group; // -1 -> spurrious interrupt
} gic_int_info_t;

int global_init_gic(uint64_t dtb);
int per_cpu_init_gic(void);

void gic_ack_int(gic_int_info_t *int_info);
void gic_end_of_int(gic_int_info_t *int_info);

uint16_t gic_max_irq(void);

uint32_t gic_num_msi_irq(void);
uint32_t gic_base_msi_irq(void);

int gic_enable_int(uint_t irq);
int gic_disable_int(uint_t irq);
int gic_int_enabled(uint_t irq);

int gic_clear_int_pending(uint_t irq);

void gic_set_target_all(uint_t irq);

void gic_dump_state(void);

#endif
