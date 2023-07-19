
#include<dev/gic.h>
#include<dev/gicv2.h>
#include<dev/gicv2m.h>

#include<nautilus/endian.h>
#include<nautilus/interrupt.h>
#include<nautilus/fdt/fdt.h>

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

int gicv2m_dtb_init(uint64_t dtb, struct gicv2 *gic) {

  GIC_DEBUG("Looking for MSI(-X) Support\n");

  int msi_offset = fdt_node_offset_by_compatible((void*)dtb, -1, "arm,gic-v2m-frame");
  while(msi_offset >= 0) {
    GIC_DEBUG("Found compatible msi-controller frame in device tree!\n");

    struct gicv2m_msi_frame *frame = malloc(sizeof(struct gicv2m_msi_frame));
    memset(frame, 0, sizeof(struct gicv2m_msi_frame));

    fdt_reg_t msi_reg = { .address = 0, .size = 0 };
    fdt_getreg((void*)dtb, msi_offset, &msi_reg);
    frame->mmio_base = msi_reg.address;
    GIC_DEBUG("\tMSI_BASE = 0x%x\n", frame->mmio_base);

    int msi_info_found = 0;
    
    // We first try to get this info from the DTB
    struct fdt_property *prop = fdt_get_property_namelen((void*)dtb, msi_offset, "arm,msi-base-spi", 16, NULL); 
    if(prop) {

      GIC_DEBUG("Found MSI Base SPI in FDT\n");
      frame->base_irq = be32toh(*(uint32_t*)prop->data);
      prop = fdt_get_property_namelen((void*)dtb, msi_offset, "arm,msi-num-spis", 16, NULL); 
      GIC_DEBUG("\tBASE_SPI = %u\n", frame->base_irq);
      if(prop) {
        GIC_DEBUG("Found MSI SPI Count in FDT\n");
        frame->num_irq = be32toh(*(uint32_t*)prop->data);
        GIC_DEBUG("\tSPI_NUM = %u\n", frame->num_irq);
        msi_info_found = 1;
      } else {
        GIC_WARN("Failed to find MSI SPI Number in the device tree\n");
      }
    } else {
      GIC_WARN("Failed to find MSI Base SPI in the device tree\n");
    } 

    if(!msi_info_found) {
      
      // If the device tree doesn't say, look at the MSI_TYPER
      // (Finding documentation on GICv2m is awful, so we're trusting 
      //  the linux implementation has the right offsets)
      GIC_DEBUG("Could not find all MSI info in the device tree so checking MMIO MSI registers\n");

      msi_type_reg_t msi_type;
      msi_type.raw = LOAD_MSI_REG(*frame, MSI_TYPER_OFFSET);
      GIC_DEBUG("MSI_TYPER = 0x%08x\n", msi_type.raw);
      
      frame->base_irq = msi_type.first_int_id;
      frame->num_irq = msi_type.spi_num;
      GIC_DEBUG("MSI SPI Range : [%u - %u]\n", frame->base_irq, frame->base_irq + frame->num_irq - 1);

      if(!frame->base_irq || !frame->num_irq) {
        GIC_DEBUG("Invalid MSI_TYPER: spi base = 0x%x, spid num = 0x%x\n",
          msi_type.first_int_id, msi_type.spi_num);
        GIC_ERROR("Failed to get MSI Information!\n");  
      } else {
        msi_info_found = 1;
      }
    }

    if(!msi_info_found) {
      free(frame);
    } else {
      frame->next = gic->msi_frame;
      gic->msi_frame = frame;
    }

    msi_offset = fdt_node_offset_by_compatible((void*)dtb, msi_offset, "arm,gic-v2m-frame");
  }
}

int gicv2m_ivec_init(uint64_t dtb, struct gicv2 *gic) 
{
  // Go through and set the MSI flag as appropriate
  for(struct gicv2m_msi_frame *frame = gic->msi_frame; frame != NULL; frame = frame->next) 
  {
    GIC_DEBUG("Setting up MSI Frame: [%u - %u]\n", frame->base_irq, frame->base_irq + frame->num_irq - 1);
    for(uint32_t i = 0; i < frame->num_irq; i++) 
    {
      struct nk_ivec_desc *desc = nk_ivec_to_desc(i + frame->base_irq);

      if(desc == NULL) {
        GIC_ERROR("MSI frame contains values outside of the range of SPI interrupts! IRQ NO. = %u\n", i+frame->base_irq);
        continue;
      }

      desc->flags |= NK_IVEC_DESC_FLAG_MSI|NK_IVEC_DESC_FLAG_MSI_X;
    }
  }
  return 0;
}

