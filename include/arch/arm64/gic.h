#ifndef __ARM64_GIC_H__
#define __ARM64_GIC_H__

#include<nautilus/naut_types.h>

typedef struct gic_int_info {
  uint32_t int_id;
  uint32_t cpu_id;
  uint8_t group; // -1 -> spurrious interrupt
} gic_int_info_t;

int global_init_gic(uint64_t dtb);
int per_cpu_init_gic(void);

int gic_ack_int(gic_int_info_t *int_info);
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
