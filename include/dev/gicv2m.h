#ifndef __GICV2M_H__
#define __GICV2M_H__

struct gicv2;
struct nk_dev_info;

#define MSI_TYPER_OFFSET 0x8

#define LOAD_MSI_REG(base, offset) *(volatile uint32_t*)(base + offset)
#define STORE_MSI_REG(base, offset, val) (*(volatile uint32_t*)(base + offset)) = val

int gicv2m_init(struct nk_dev_info *gic_info);

#endif
