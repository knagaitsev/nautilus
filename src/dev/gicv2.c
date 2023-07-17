
#include<dev/gicv2.h>
#include<dev/gic_common.h>

#include<nautilus/nautilus.h>
#include<nautilus/irqdev.h>
#include<nautilus/interrupt.h>
#include<nautilus/endian.h>
#include<nautilus/fdt/fdt.h>

#include<arch/arm64/sys_reg.h>

#ifndef NAUT_CONFIG_DEBUG_PRINTS
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

#define GIC_PRINT(fmt, args...) printk("[GICv2] " fmt, ##args)
#define GIC_DEBUG(fmt, args...) DEBUG_PRINT("[GICv2] " fmt, ##args)
#define GIC_ERROR(fmt, args...) ERROR_PRINT("[GICv2] " fmt, ##args)
#define GIC_WARN(fmt, args...) WARN_PRINT("[GICv2] " fmt, ##args)

#ifdef NAUT_CONFIG_ENABLE_ASSERTS
  #define INVALID_INT_NUM(gic, num) (num >= (gic).max_irq)
#else
  #define INVALID_INT_NUM(gic, num) 0
#endif

#define ASSERT_SIZE(type, size) _Static_assert(sizeof(type) == size, \
    "sizeof("#type") is not "#size" bytes!\n") 
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

#define MSI_TYPER_OFFSET 0x8

struct gicv2 {

  uint64_t dist_base;
  uint64_t cpu_base;

  struct gic_msi_frame *msi_frame;

  uint32_t max_irq;
  uint32_t cpu_num;

  uint8_t security_ext;

  char *compatible_string;

};

#define LOAD_GICD_REG(gic, offset) *(volatile uint32_t*)((gic).dist_base + offset)
#define STORE_GICD_REG(gic, offset, val) (*(volatile uint32_t*)((gic).dist_base + offset)) = val

#define GICD_BITMAP_SET(gic, num, offset) \
  *(((volatile uint32_t*)((gic).dist_base + offset))+(num >> 5)) |= (1<<(num&(0x1F)))
#define GICD_BITMAP_READ(gic, num, offset) \
  (!!(*(((volatile uint32_t*)((gic).dist_base + offset))+(num >> 5)) & (1<<(num&(0x1F)))))

#define GICD_DUALBITMAP_READ(gic, num, offset) \
  (*(((volatile uint8_t*)((gic).dist_base + offset))+(num >> 4)) & (0x3<<((num&0xF)<<4)))

#define GICD_BYTEMAP_READ(gic, num, offset) \
  (*(volatile uint8_t*)((gic).dist_base + offset + num))
#define GICD_BYTEMAP_WRITE(gic, num, offset, val) \
  *((volatile uint8_t*)((gic).dist_base + offset + num)) = (uint8_t)val

#define LOAD_GICC_REG(gic, offset) *(volatile uint32_t*)((gic).cpu_base + offset)
#define STORE_GICC_REG(gic, offset, val) (*(volatile uint32_t*)((gic).cpu_base + offset)) = val

#define LOAD_MSI_REG(frame, offset) *(volatile uint32_t*)((frame).mmio_base + offset)
#define STORE_MSI_REG(frame, offset, val) (*(volatile uint32_t*)((frame).mmio_base + offset)) = val

typedef union gicd_ctl_reg {
  uint32_t raw;
  struct {
    uint_t grp0_en : 1;
    // uint_t grp1_en : 1; // Secure Mode Only
    // Rest Reserved
  };
} gicd_ctl_reg_t;
ASSERT_SIZE(gicd_ctl_reg_t, 4);


typedef union gicd_type_reg {
  uint32_t raw;
  struct {
    uint_t it_lines_num : 5;
    uint_t cpu_num : 3;
    uint_t __res0_0 : 2;
    uint_t sec_ext_impl : 1;
    uint_t num_lspi : 5;
    // Rest Reserved
  };
} gicd_type_reg_t;
ASSERT_SIZE(gicd_type_reg_t, 4);

typedef union gicc_ctl_reg {
  uint32_t raw;
  struct {
    uint_t grp1_en : 1;
    // Rest reserved
  } sss;
  struct {
    uint_t grp1_en : 1;
    uint_t __res0_0 : 4;
    uint_t dis_bypass_fiq_grp1 : 1;
    uint_t dis_bypass_irq_grp1 : 1;
    uint_t __res0_1 : 2;
    uint_t multistep_eoi : 1;
    // Rest reserved
  } isc;
} gicc_ctl_reg_t;

typedef union gicc_priority_reg {
  uint32_t raw;
  struct {
    uint8_t priority;
    // Rest Reserved
  };
} gicc_priority_reg_t;

typedef union gicc_int_info_reg {
  uint32_t raw;
  struct {
    uint_t int_id : 13;
  };
} gicc_int_info_reg_t;

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

static const char *gicv2_compat_list[] = {
        "arm,arm1176jzf-devchip-gic",
	"arm,arm11mp-gic",
	"arm,cortex-a15-gic",
	"arm,cortex-a7-gic",
	"arm,cortex-a9-gic",
	"arm,eb11mp-gic",
	"arm,gic-400",
	"arm,pl390",
	"arm,tc11mp-gic",
	"brcm,brahma-b15-gic",
	"nvidia,tegra210-agic",
	"qcom,msm-8660-qgic",
	"qcom,msm-qgic2"
};

static int gicv2_dev_init(uint64_t dtb, struct gicv2 *gic) {

  // Find node with "interrupt-controller" property
  int offset = fdt_node_offset_by_prop_value((void*)dtb, -1, "interrupt-controller", NULL, NULL);
  if(offset < 0) {
    GIC_ERROR("FDT_ERROR: %s\n", fdt_strerror(offset));
  }
  while(offset >= 0) { 
    int compat_result = 1;
    for(uint_t i = 0; i < sizeof(gicv2_compat_list)/sizeof(const char*); i++) {
      compat_result = fdt_node_check_compatible((void*)dtb, offset, gicv2_compat_list[i]);
     
      if(compat_result & ~1) {
        GIC_ERROR("FDT_ERROR: %s\n", fdt_strerror(compat_result));
        return -1;
      }

      if(!compat_result) {
        gic->compatible_string = gicv2_compat_list[i];
        GIC_DEBUG("Found compatible GICv2 controller in device tree: %s\n", gic->compatible_string);
        break;
      }
    }
    if(compat_result) {
      GIC_DEBUG("Found interrupt controller in the device tree which is not compatible with GICv2\n");
    } else {
      break;
    }
    offset = fdt_node_offset_by_prop_value((void*)dtb, offset, "interrupt-controller", NULL, NULL);
  }

  if(offset == -FDT_ERR_NOTFOUND) {
    GIC_ERROR("Could not find a compatible interrupt controller in the device tree!\n");
    return -1;
  }

  int num_read = 2;
  fdt_reg_t reg[num_read];
  int getreg_result = fdt_getreg_array((void*)dtb, offset, reg, &num_read);

  if(!getreg_result && num_read == 2) {
     gic->dist_base = reg[0].address;
     gic->cpu_base = reg[1].address;
     GIC_DEBUG("GICD_BASE = 0x%x GICC_BASE = 0x%x\n", gic->dist_base, gic->cpu_base);
  } else {
    GIC_ERROR("Invalid reg field in the device tree!\n");
    return -1;
  }

  gicd_type_reg_t d_type_reg;
  d_type_reg.raw = LOAD_GICD_REG(*gic, GICD_TYPER_OFFSET);
  gic->max_irq = 32*(d_type_reg.it_lines_num + 1);
  gic->cpu_num = d_type_reg.cpu_num;
  gic->security_ext = d_type_reg.sec_ext_impl;

  GIC_DEBUG("max irq = %u, cpu num = %u, security extensions = %s\n", gic->max_irq, gic->cpu_num, 
      gic->security_ext ? "ENABLED" : "DISABLED");

  gicd_ctl_reg_t d_ctl_reg;
  d_ctl_reg.raw = LOAD_GICD_REG(*gic, GICD_CTLR_OFFSET);
  d_ctl_reg.grp0_en = 1;
  STORE_GICD_REG(*gic, GICD_CTLR_OFFSET, d_ctl_reg.raw);

  GIC_PRINT("inited globally\n");

  return 0;
}

static int gicv2_dev_msi_init(uint64_t dtb, struct gicv2 *gic) {

  GIC_DEBUG("Looking for MSI(-X) Support\n");

  int msi_offset = fdt_node_offset_by_compatible((void*)dtb, -1, "arm,gic-v2m-frame");
  while(msi_offset >= 0) {
    GIC_DEBUG("Found compatible msi-controller frame in device tree!\n");

    struct gic_msi_frame *frame = malloc(sizeof(struct gic_msi_frame));
    memset(frame, 0, sizeof(struct gic_msi_frame));

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

static int gicv2_dev_int_desc_init(struct nk_irq_dev *dev, struct gicv2 *gic) 
{

  // Set up 0-15 for SGI
  nk_alloc_ivec_descs(0, 16, dev, NK_IVEC_DESC_TYPE_DEFAULT, NK_IVEC_DESC_FLAG_PERCPU | NK_IVEC_DESC_FLAG_IPI);
  GIC_DEBUG("Set Up SGI Vectors\n");
  
  // Set up 16-31 for PPI
  nk_alloc_ivec_descs(16, 16, dev, NK_IVEC_DESC_TYPE_DEFAULT, NK_IVEC_DESC_FLAG_PERCPU);
  GIC_DEBUG("Set Up PPI Vectors\n");

  // Set up the correct number of SPI
  GIC_DEBUG("gic->max_irq = %u\n", gic->max_irq);
  nk_alloc_ivec_descs(32, gic->max_irq - 32, dev, NK_IVEC_DESC_TYPE_DEFAULT, 0);
  GIC_DEBUG("Set Up SPI Vectors\n");

  // Add the special INTIDS
  GIC_DEBUG("Set Up Special Interrupt Vectors!\n");
  nk_alloc_ivec_descs(1020, 4, dev, NK_IVEC_DESC_TYPE_SPURRIOUS, 0);


  // Now go through and set the MSI flag as appropriate
  for(struct gic_msi_frame *frame = gic->msi_frame; frame != NULL; frame = frame->next) 
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

static int gicv2_dev_get_characteristics(void *state, struct nk_irq_dev_characteristics *c)
{
  memset(c, 0, sizeof(c));
  return 0;
}

static int gicv2_dev_initialize_cpu(void *state) {

  struct gicv2 *gic = (struct gicv2*)state;

  // Enable insecure interrupts
  gicc_ctl_reg_t ctl;
  ctl.raw = LOAD_GICC_REG(*gic, GICC_CTLR_OFFSET);

  if(gic->security_ext) {
    ctl.isc.grp1_en = 1;
    ctl.isc.multistep_eoi = 0;
  } else {
    ctl.sss.grp1_en = 1;
  }

  STORE_GICC_REG(*gic, GICC_CTLR_OFFSET, ctl.raw);

  // Set priority mask to allow all
  gicc_priority_reg_t prior_reg;
  prior_reg.raw = LOAD_GICC_REG(*gic, GICC_PMR_OFFSET);
  prior_reg.priority = 0xFF;
  STORE_GICC_REG(*gic, GICC_PMR_OFFSET, prior_reg.raw);

  GIC_DEBUG("per_cpu GICv2 initialized\n");

  return 0;
}

static inline int gicv2_is_spurrious_int(uint32_t id) {
  return ((id & (~0b11)) == 0x3FC);
}

static int gicv2_dev_ack_irq(void *state, nk_irq_t *irq) {

  struct gicv2 *gic = (struct gicv2 *)state;

  gicc_int_info_reg_t info_reg;
  info_reg.raw = LOAD_GICC_REG(*gic, GICC_IAR_OFFSET);

  if(gicv2_is_spurrious_int(info_reg.int_id)) {
    return IRQ_DEV_ACK_ERR_RET;
  }

  *irq = info_reg.int_id;
  return IRQ_DEV_ACK_SUCCESS;

}

static int gicv2_dev_eoi_irq(void *state, nk_irq_t irq) {

  struct gicv2 *gic = (struct gicv2 *)state;

  gicc_int_info_reg_t info_reg;
  info_reg.int_id = irq;
  STORE_GICC_REG(*gic, GICC_EOIR_OFFSET, info_reg.raw);

  return IRQ_DEV_EOI_SUCCESS;

}

static int gicv2_dev_enable_irq(void *state, nk_irq_t irq) {

  GICD_BITMAP_SET(*(struct gicv2*)state, irq, GICD_ISENABLER_0_OFFSET);
  if(irq >= 32) {
    GICD_BYTEMAP_WRITE(*(struct gicv2*)state, irq, GICD_ITARGETSR_0_OFFSET, ((1<<((struct gicv2*)state)->cpu_num)-1));
  }
  return 0;

}

static int gicv2_dev_disable_irq(void *state, nk_irq_t irq) {

  GICD_BITMAP_SET(*(struct gicv2*)state, irq, GICD_ICENABLER_0_OFFSET);
  return 0;

}

static int gicv2_dev_irq_status(void *state, nk_irq_t irq) {

  struct gicv2 *gic = (struct gicv2*)state;

  int status = 0;

  status |= GICD_BITMAP_READ(*gic, irq, GICD_ISENABLER_0_OFFSET) ?
    IRQ_DEV_STATUS_ENABLED : 0;

  status |= GICD_BITMAP_READ(*gic, irq, GICD_ISPENDR_0_OFFSET) ?
    IRQ_DEV_STATUS_PENDING : 0;

  status |= GICD_BITMAP_READ(*gic, irq, GICD_ISACTIVER_0_OFFSET) ?
    IRQ_DEV_STATUS_ACTIVE : 0;

  return status;

}

static struct nk_irq_dev_int gicv2_dev_int = {
  .get_characteristics = gicv2_dev_get_characteristics,
  .initialize_cpu = gicv2_dev_initialize_cpu,
  .ack_irq = gicv2_dev_ack_irq,
  .eoi_irq = gicv2_dev_eoi_irq,
  .enable_irq = gicv2_dev_enable_irq,
  .disable_irq = gicv2_dev_disable_irq,
  .irq_status = gicv2_dev_irq_status
};

int gicv2_init(uint64_t dtb) 
{
  struct gicv2 *state = malloc(sizeof(struct gicv2));
  memset(state, 0, sizeof(struct gicv2));

  if(state == NULL) {
    GIC_ERROR("Failed to allocate gicv2 struct!\n");
    return -1;
  }

  if(gicv2_dev_init(dtb, state)) {
    GIC_ERROR("Failed to initialize GIC device!\n");
    return -1;
  }

  if(gicv2_dev_msi_init(dtb, state)) {
    GIC_WARN("Failed to initialize GICv2m MSI Support!\n");
  }

  struct nk_irq_dev *dev = nk_irq_dev_register("gicv2", 0, &gicv2_dev_int, (void*)state);

  if(dev == NULL) {
    GIC_ERROR("Failed to register as an IRQ device!\n");
    return -1;
  }

  if(gicv2_dev_int_desc_init(dev, state)) {
    GIC_ERROR("Failed to set up interrupt vector descriptors!\n");
    return -1;
  }

  if(nk_assign_all_cpus_irq_dev(dev)) {
    GIC_ERROR("Failed to assign the GIC to all CPU's in the system!\n");
    return -1;
  }

  return 0;
}

