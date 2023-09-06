#ifndef __GICV2_H__
#define __GICV2_H__

#include<nautilus/naut_types.h>

#ifndef NAUT_CONFIG_DEBUG_GIC
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

#define GIC_PRINT(fmt, args...) printk("[GICv2] " fmt, ##args)
#define GIC_DEBUG(fmt, args...) DEBUG_PRINT("[GICv2] " fmt, ##args)
#define GIC_ERROR(fmt, args...) ERROR_PRINT("[GICv2] " fmt, ##args)
#define GIC_WARN(fmt, args...) WARN_PRINT("[GICv2] " fmt, ##args)

struct gicv2 {

  uint64_t dist_base;
  uint64_t cpu_base;

  nk_irq_t irq_base;
  struct nk_irq_desc *sgi_descs;
  struct nk_irq_desc *ppi_descs;
  struct nk_irq_desc *spi_descs;

  uint32_t max_spi;
  uint32_t num_cpus;

  uint8_t security_ext;

#ifdef NAUT_CONFIG_GIC_VERSION_2M
  struct gicv2m_msi_frame *msi_frame;
#endif
};

int gicv2_init(void);

#endif
