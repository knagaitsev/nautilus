
#include<dev/gic.h>
#include<dev/gicv2.h>
#include<dev/gicv2m.h>

#include<nautilus/endian.h>
#include<nautilus/interrupt.h>
#include<nautilus/of/dt.h>
#include<nautilus/iomap.h>

typedef union msi_type_reg {
  uint32_t raw;
  struct {
    uint_t spi_num : 11;
    uint_t __res0_0 : 5;
    uint_t first_int_id : 13;
    // Linux claims these don't exist
    //uint_t secure_aliases : 1;
    //uint_t clr_reg_supported : 1;
    //uint_t valid : 1;
  };
} msi_type_reg_t;

int gicv2m_of_init_frame(struct nk_dev_info *msi_info) 
{
  if(!nk_dev_info_has_property(msi_info, "msi-controller")) {
    GIC_WARN("GICv2m Device Tree MSI Frame does not have the \"msi-controller\" property!\n");
    goto err_exit;
  }

  // Get the gic
  struct nk_dev_info * parent_info = nk_dev_info_get_parent(msi_info);
  if(parent_info == NULL) {
    GIC_WARN("Could not get parent node of gicv2m MSI frame!\n");
    goto err_exit;
  }

  struct nk_dev * gic_dev = nk_dev_info_get_device(parent_info);
  if(gic_dev == NULL || gic_dev->type != NK_DEV_IRQ) {
    GIC_WARN("No IRQ device associated with msi frame parent!\n");
    goto err_exit;
  }

  struct nk_irq_dev *gic_irq_dev = (struct nk_irq_dev*)gic_dev;

  nk_hwirq_t base_hwirq = NK_NULL_IRQ;
  int num_hwirq = 0;

  void *mmio_base = NULL;
  int mmio_size = 0;

  nk_dev_info_read_register_block(msi_info, &mmio_base, &mmio_size);
  mmio_base = nk_io_map(mmio_base, mmio_size, 0);
  if(mmio_base == NULL) {
    GIC_ERROR("Failed to map MSI_BASE registers!\n");
    goto err_exit;
  }
  GIC_DEBUG("\tMSI_BASE = 0x%x\n", mmio_base);

  // First get frame from MSI_TYPER register
  msi_type_reg_t msi_type;
  msi_type.raw = LOAD_MSI_REG(mmio_base, MSI_TYPER_OFFSET);
  GIC_DEBUG("MSI_TYPER = 0x%08x\n", msi_type.raw);
  
  base_hwirq = msi_type.first_int_id;
  num_hwirq = msi_type.spi_num;
 
  // See if the Device Tree overrides the MSI_TYPER values
  uint32_t info_base_spi = 0;
  if(!nk_dev_info_read_u32(msi_info, "arm,msi-base-spi", &info_base_spi)) {
    GIC_DEBUG("Device tree override msi-base-spi from MSI_TYPER\n");
    base_hwirq = info_base_spi;
  } 
  uint32_t info_num_spi = 0;
  if(!nk_dev_info_read_u32(msi_info, "arm,msi-num-spis", &info_num_spi)) {
    GIC_DEBUG("Device tree override msi-num-spis from MSI_TYPER\n");
    num_hwirq = info_num_spi;
  }

  GIC_DEBUG("MSI SPI Range : [%u - %u]\n", base_hwirq, base_hwirq + num_hwirq - 1);
   
  // Go through and set the MSI flag as appropriate
  for(uint32_t i = 0; i < num_hwirq; i++) 
  {
    nk_irq_t irq;

    if(nk_irq_dev_revmap(gic_irq_dev, i + base_hwirq, &irq)) {
      GIC_WARN("MSI frame contains SPI number without reverse mapping! SPI = %u\n", i + base_hwirq);
      continue;
    }

    struct nk_irq_desc *desc = nk_irq_to_desc(irq);

    if(desc == NULL) {
      GIC_WARN("Could not get interrupt descriptor for IRQ %u, (HWIRQ %u)!\n", irq, i + base_hwirq);
      continue;
    }

    desc->flags |= NK_IRQ_DESC_FLAG_MSI|NK_IRQ_DESC_FLAG_MSI_X;
  }

  return 0;

err_exit:
  return -1;
}

static struct of_dev_id gicv2m_of_dev_ids[] = {
  { .compatible = "arm,gic-v2m-frame" }
};
declare_of_dev_match_id_array(gicv2m_of_match, gicv2m_of_dev_ids);

int gicv2m_init(struct nk_dev_info *gic_info) {
  if(gic_info->type == NK_DEV_INFO_OF) {
    of_for_each_subnode_match(&gicv2m_of_match, gicv2m_of_init_frame, gic_info);
    return 0;
  } else {
    GIC_WARN("GICv2m currently only supports Device Tree initialization!\n");
    return -1;
  }
}

