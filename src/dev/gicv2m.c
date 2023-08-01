
#include<dev/gic.h>
#include<dev/gicv2.h>
#include<dev/gicv2m.h>

#include<nautilus/endian.h>
#include<nautilus/interrupt.h>
#include<nautilus/of/dt.h>

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

  struct gicv2m_msi_frame frame;
  memset(&frame, 0, sizeof(struct gicv2m_msi_frame));

  int mmio_size = 0;
  nk_dev_info_read_register_block(msi_info, &frame.mmio_base, &mmio_size);
  GIC_DEBUG("\tMSI_BASE = 0x%x\n", frame.mmio_base);

  // First get frame from MSI_TYPER register
  msi_type_reg_t msi_type;
  msi_type.raw = LOAD_MSI_REG(frame, MSI_TYPER_OFFSET);
  GIC_DEBUG("MSI_TYPER = 0x%08x\n", msi_type.raw);
  
  frame.base_irq = msi_type.first_int_id;
  frame.num_irq = msi_type.spi_num;
 
  // See if the Device Tree overrides the MSI_TYPER values
  uint32_t info_base_spi = 0;
  if(nk_dev_info_read_u32(msi_info, "arm,msi-base-spi", &info_base_spi)) {
    frame.base_irq = info_base_spi;
  } 
  uint32_t info_num_spi = 0;
  if(nk_dev_info_read_u32(msi_info, "arm,msi-num-spis", &info_num_spi)) {
    frame.num_irq = info_num_spi;
  }

  GIC_DEBUG("MSI SPI Range : [%u - %u]\n", frame.base_irq, frame.base_irq + frame.num_irq - 1);
   
  // Go through and set the MSI flag as appropriate
  GIC_DEBUG("Setting up MSI Frame: [%u - %u]\n", frame.base_irq, frame.base_irq + frame.num_irq - 1);
  for(uint32_t i = 0; i < frame.num_irq; i++) 
  {
    struct nk_ivec_desc *desc = nk_ivec_to_desc(i + frame.base_irq);

    if(desc == NULL) {
      GIC_WARN("MSI frame contains value outside of the range of SPI interrupts! IRQ NO. = %u\n", i+frame.base_irq);
      continue;
    }

    desc->flags |= NK_IVEC_DESC_FLAG_MSI|NK_IVEC_DESC_FLAG_MSI_X;
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

