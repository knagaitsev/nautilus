#ifndef __GICV2M_H__
#define __GICV2M_H__

struct gicv2;

#define MSI_TYPER_OFFSET 0x8

#define LOAD_MSI_REG(frame, offset) *(volatile uint32_t*)((frame).mmio_base + offset)
#define STORE_MSI_REG(frame, offset, val) (*(volatile uint32_t*)((frame).mmio_base + offset)) = val

struct gicv2m_msi_frame {

  uint64_t mmio_base;
  
  uint32_t base_irq;
  uint32_t num_irq;
  
  struct gicv2m_msi_frame *next;

};

int gicv2m_dtb_init(uint64_t dtb, struct gicv2 *gic);
int gicv2m_ivec_init(uint64_t dtb, struct gicv2 *gic);

#endif
