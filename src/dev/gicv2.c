
#include<dev/gic.h>
#include<dev/gicv2.h>

#include<nautilus/nautilus.h>
#include<nautilus/irqdev.h>
#include<nautilus/interrupt.h>
#include<nautilus/endian.h>
#include<nautilus/of/fdt.h>
#include<nautilus/of/dt.h>
#include<nautilus/atomic.h>
#include<nautilus/iomap.h>

#include<arch/arm64/sys_reg.h>
#include<arch/arm64/excp.h>

#ifdef NAUT_CONFIG_ENABLE_ASSERTS
  #define INVALID_INT_NUM(gic, num) (num >= (gic).max_spi)
#else
  #define INVALID_INT_NUM(gic, num) 0
#endif

#define ASSERT_SIZE(type, size) _Static_assert(sizeof(type) == size, \
    "sizeof("#type") is not "#size" bytes!\n") 

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
    uint_t num_cpus : 3;
    uint_t __res0_0 : 2;
    uint_t sec_ext_impl : 1;
    uint_t num_lpi : 5;
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

static struct gicv2 *global_gicv2_ptr = NULL;

static int gicv2_dev_get_characteristics(void *state, struct nk_irq_dev_characteristics *c)
{
  memset(c, 0, sizeof(c));
  return 0;
}

int gicv2_percpu_init(void) {

  struct gicv2 *gic = global_gicv2_ptr;

  if(gic == NULL) {
    GIC_ERROR("Called gicv2_percpu_init with NULL GIC!\n");
    return -1;
  }

  // Enable insecure interrupts
  gicc_ctl_reg_t ctl;
  ctl.raw = GICC_LOAD_REG(gic, GICC_CTLR_OFFSET);

  if(gic->security_ext) {
    ctl.isc.grp1_en = 1;
    ctl.isc.multistep_eoi = 0;
  } else {
    ctl.sss.grp1_en = 1;
  }

  GICC_STORE_REG(gic, GICC_CTLR_OFFSET, ctl.raw);

  // Set priority mask to allow all
  gicc_priority_reg_t prior_reg;
  prior_reg.raw = GICC_LOAD_REG(gic, GICC_PMR_OFFSET);
  prior_reg.priority = 0xFF;
  GICC_STORE_REG(gic, GICC_PMR_OFFSET, prior_reg.raw);

  GIC_DEBUG("per_cpu GICv2 initialized\n");

  return 0;
}

static int gicv2_dev_revmap(void *state, nk_hwirq_t hwirq, nk_irq_t *irq) 
{
  struct gicv2 *gic = (struct gicv2*)state;
  if(hwirq < 0) {
    return -1;
  }
  else if(hwirq <= gic->max_spi) {
    *irq = gic->irq_base + hwirq;
    return 0;
  } else if(hwirq >= 1020 && hwirq < 1024) {
    *irq = gic->special_irq_base + (hwirq - 1020);
    return 0;
  }

  return -1;
}

static int gicv2_dev_translate(void *state, nk_dev_info_type_t type, void *raw_irq, nk_hwirq_t *out_hwirq) 
{
  if(type != NK_DEV_INFO_OF) {
    GIC_ERROR("Currently only support Device Tree interrupt format, but another type of device info was provided! dev_info_type = %u\n", type);
    return -1;
  }

  uint32_t *cells = (uint32_t*)raw_irq;
  uint32_t irq_type = be32toh(cells[0]);
  uint32_t num = be32toh(cells[1]);
  uint32_t flags = be32toh(cells[2]); // We're going to ignore this field for now
                                      // (We could use it to add some info to the nk_irq_desc for this hwirq)
  switch(irq_type) {
    case 0:
      // It's an SPI
      if(num > 987) {
        GIC_DEBUG("Failed to translate FDT interrupt: SPI interrupt had range >987 (%u)\n",num);
        return -1;
      } else {
        num += 32;
      }
      break;
    case 1:
      // It's a PPI
      if(num > 15) {
        GIC_DEBUG("Failed to translate FDT interrrupt: PPI interrupt had range >15 (%u)\n",num);
        return -1;
      } else {
        num += 16;
      }
      break;
    default:
      // Something is horribly wrong :(
      GIC_DEBUG("Failed to translate FDT interrupt: neither SPI nor PPI was specified!\n");
      return -1;
  }

  *out_hwirq = (nk_hwirq_t)num;
  return 0;
}

static inline int gicv2_is_spurrious_int(uint32_t id) {
  return ((id & (~0b11)) == 0x3FC);
}

static int gicv2_dev_ack_irq(void *state, nk_hwirq_t *irq) {

  struct gicv2 *gic = (struct gicv2 *)state;

  gicc_int_info_reg_t info_reg;
  info_reg.raw = GICC_LOAD_REG(gic, GICC_IAR_OFFSET);

  if(gicv2_is_spurrious_int(info_reg.int_id)) {
    return IRQ_DEV_ACK_ERR_RET;
  }

  *irq = info_reg.int_id;
  return IRQ_DEV_ACK_SUCCESS;

}

static int gicv2_dev_eoi_irq(void *state, nk_hwirq_t irq) {

  struct gicv2 *gic = (struct gicv2 *)state;

  gicc_int_info_reg_t info_reg;
  info_reg.int_id = irq;
  GICC_STORE_REG(gic, GICC_EOIR_OFFSET, info_reg.raw);

  return IRQ_DEV_EOI_SUCCESS;

}

static uint8_t irq_target_round_robin;

static int gicv2_dev_enable_irq(void *state, nk_hwirq_t irq) {

  struct gicv2 *gic = (struct gicv2*)state;
  GIC_DEBUG("Enabled IRQ: %u\n", irq);

  GICD_BITMAP_SET(gic, GICD_ISENABLER_0_OFFSET, irq);
  if(irq >= 32) {
    uint8_t target = atomic_add(irq_target_round_robin, 1) % gic->num_cpus;
    GICD_BYTEMAP_WRITE(gic, GICD_ITARGETSR_0_OFFSET, irq, 1<<target);
    GIC_DEBUG("Set IRQ %u Target to CPU %u\n", irq, target);
  }
  return 0;

}

static int gicv2_dev_disable_irq(void *state, nk_hwirq_t irq) {

  GICD_BITMAP_SET((struct gicv2*)state, GICD_ICENABLER_0_OFFSET, irq);

  return 0;
}

static int gicv2_dev_irq_status(void *state, nk_hwirq_t irq) {

  struct gicv2 *gic = (struct gicv2*)state;

  int status = 0;

  status |= GICD_BITMAP_READ(gic, GICD_ISENABLER_0_OFFSET, irq) ?
    IRQ_STATUS_ENABLED : 0;

  status |= GICD_BITMAP_READ(gic, GICD_ISPENDR_0_OFFSET, irq) ?
    IRQ_STATUS_PENDING : 0;

  status |= GICD_BITMAP_READ(gic, GICD_ISACTIVER_0_OFFSET, irq) ?
    IRQ_STATUS_ACTIVE : 0;

  return status;
}

static struct nk_irq_dev_int gicv2_dev_int = {
  .get_characteristics = gicv2_dev_get_characteristics,
  .ack_irq = gicv2_dev_ack_irq,
  .eoi_irq = gicv2_dev_eoi_irq,
  .enable_irq = gicv2_dev_enable_irq,
  .disable_irq = gicv2_dev_disable_irq,
  .irq_status = gicv2_dev_irq_status,
  .revmap = gicv2_dev_revmap,
  .translate = gicv2_dev_translate
};

#ifdef NAUT_CONFIG_GIC_VERSION_2M
#include<dev/gicv2m.h>
#endif

static int gicv2_init_dev_info(struct nk_dev_info *info) {

  GIC_DEBUG("initializing with nk_dev_info = \"%s\"\n", nk_dev_info_get_name(info));

  int did_alloc = 0;
  int did_register = 0;

  if(info->type != NK_DEV_INFO_OF) {
    GIC_ERROR("Currently only support device tree initialization!\n");
  } else {
    GIC_DEBUG("Using device tree info\n");
  }

  struct gicv2 *gic = malloc(sizeof(struct gicv2));

  if(gic == NULL) {
    GIC_ERROR("Failed to allocate gicv2 struct!\n");
    goto err_exit;
  } else {
    GIC_DEBUG("Allocated structure\n");
    did_alloc = 1;
  }

  memset(gic, 0, sizeof(struct gicv2));

  global_gicv2_ptr = gic;

  void * bases[2];
  int sizes[2];

  if(nk_dev_info_read_register_blocks_exact(info, bases, sizes, 2)) {
    GIC_ERROR("Failed to read register mmio bases!\n");
    goto err_exit;
  }

  gic->dist_base = (uint64_t)nk_io_map(bases[0], sizes[0], 0);
  if(gic->dist_base == NULL) {
    GIC_ERROR("Failed to map GICD register region!\n");
    goto err_exit;
  }
  gic->cpu_base = (uint64_t)nk_io_map(bases[1], sizes[0], 0);
  if(gic->cpu_base == NULL) {
    GIC_ERROR("Failed to map GICC register region!\n");
    goto err_exit;
  }
  GIC_DEBUG("GICD_BASE = 0x%x GICC_BASE = 0x%x\n", gic->dist_base, gic->cpu_base);

  gicd_type_reg_t d_type_reg;
  d_type_reg.raw = GICD_LOAD_REG(gic, GICD_TYPER_OFFSET);
  gic->max_spi = 32*(d_type_reg.it_lines_num + 1);
  gic->num_cpus = d_type_reg.num_cpus + 1;
  gic->security_ext = d_type_reg.sec_ext_impl;

  GIC_DEBUG("max irq = %u, cpu num = %u, security extensions = %s\n", gic->max_spi, gic->num_cpus, 
      gic->security_ext ? "ENABLED" : "DISABLED");

  gicd_ctl_reg_t d_ctl_reg;
  d_ctl_reg.raw = GICD_LOAD_REG(gic, GICD_CTLR_OFFSET);
  d_ctl_reg.grp0_en = 1;
  GICD_STORE_REG(gic, GICD_CTLR_OFFSET, d_ctl_reg.raw);

  struct nk_irq_dev *dev = nk_irq_dev_register("gicv2", 0, &gicv2_dev_int, (void*)gic);

  if(dev == NULL) {
    GIC_ERROR("Failed to register as an IRQ device!\n");
    goto err_exit;
  } else {
    did_register = 1;
  }

  nk_dev_info_set_device(info, (struct nk_dev*)dev);

#define SGI_HWIRQ      0
#define SGI_NUM        16
#define PPI_HWIRQ      16
#define PPI_NUM        16
#define SPI_HWIRQ      32
#define SPECIAL_HWIRQ  1020
#define SPECIAL_NUM    4
  
  if(nk_request_irq_range((gic->max_spi + 1) + SPECIAL_NUM, &gic->irq_base)) {
    GIC_ERROR("Failed to get IRQ numbers for interrupts!\n");
    goto err_exit;
  } 

  // Set up 0-15 for SGI
  gic->sgi_descs = nk_alloc_irq_descs(SGI_NUM, SGI_HWIRQ, NK_IRQ_DESC_FLAG_PERCPU | NK_IRQ_DESC_FLAG_IPI, dev);
  if(gic->sgi_descs == NULL) {
    GIC_ERROR("Failed to allocate IRQ descriptors for SGI interrupts!\n");
    goto err_exit;
  }
  if(nk_assign_irq_descs(SGI_NUM, gic->irq_base + SGI_HWIRQ, gic->sgi_descs)) {
    GIC_ERROR("Failed to assign IRQ descriptors for SGI interrupts!\n");
    goto err_exit;
  } 
  GIC_DEBUG("Set Up SGI's\n");
  
  // Set up 16-31 for PPI
  gic->ppi_descs = nk_alloc_irq_descs(PPI_NUM, PPI_HWIRQ, NK_IRQ_DESC_FLAG_PERCPU, dev);
  if(gic->ppi_descs == NULL) {
    GIC_ERROR("Failed to allocate IRQ descriptors for PPI interrupts!\n");
    goto err_exit;
  }
  if(nk_assign_irq_descs(PPI_NUM, gic->irq_base + PPI_HWIRQ, gic->ppi_descs)) {
    GIC_ERROR("Failed to assign IRQ descriptors for PPI interrupts!\n");
    goto err_exit;
  }
  GIC_DEBUG("Set Up PPI Vectors\n");

  // Set up the correct number of SPI
  GIC_DEBUG("gic->max_spi = %u\n", gic->max_spi);
  int num_spi = gic->max_spi - 31;
  gic->spi_descs = nk_alloc_irq_descs(num_spi, SPI_HWIRQ, 0, dev);
  if(gic->spi_descs == NULL) {
    GIC_ERROR("Failed to allocate IRQ descriptors for SPI interrupts!\n");
    goto err_exit;
  }
  if(nk_assign_irq_descs(num_spi, gic->irq_base + SPI_HWIRQ, gic->spi_descs)) {
    GIC_ERROR("Failed to assign IRQ descriptors for SPI interrupts!\n");
    goto err_exit;
  }
  GIC_DEBUG("Set Up SPI Vectors\n");

  // Set up spurrious interrupt descriptors
  gic->special_descs = nk_alloc_irq_descs(SPECIAL_NUM, SPECIAL_HWIRQ, 0, dev);
  if(gic->special_descs == NULL) {
    GIC_ERROR("Failed to allocate IRQ descriptors for Special INTID's!\n");
    goto err_exit;
  }
  if(nk_assign_irq_descs(SPECIAL_NUM, gic->irq_base + SPI_HWIRQ + num_spi, gic->special_descs)) {
    GIC_ERROR("Failed to assign IRQ descriptors for Special INTID's!\n");
    goto err_exit;
  }
  gic->special_irq_base = gic->irq_base + SPI_HWIRQ + num_spi;
  GIC_DEBUG("Added Special INTID's\n");

  if(arm64_set_root_irq_dev(dev)) {
    GIC_ERROR("Failed to make GICv2 the root IRQ device!\n");
    goto err_exit;
  }

#ifdef NAUT_CONFIG_GIC_VERSION_2M
  if(gicv2m_init(info)) {
    GIC_WARN("Failed to initialize GICv2m MSI Support!\n");
  }
#endif

  GIC_DEBUG("initialized globally\n");

  return 0;

err_exit:
  if(did_alloc) {
    free(gic);
  }
  if(did_register) {
    nk_irq_dev_unregister(dev);
  }
  return -1;
}

static const char * gicv2_properties_names[] = {
  "interrupt-controller"
};

static const struct of_dev_properties gicv2_of_properties = {
  .names = gicv2_properties_names,
  .count = 1
};

static const struct of_dev_id gicv2_of_dev_ids[] = {
  { .properties = &gicv2_of_properties, .compatible = "arm,arm1176jzf-devchip-gic" },
  { .properties = &gicv2_of_properties, .compatible = "arm,arm11mp-gic" },
  { .properties = &gicv2_of_properties, .compatible = "arm,cortex-a15-gic" },
  { .properties = &gicv2_of_properties, .compatible = "arm,cortex-a7-gic" },
  { .properties = &gicv2_of_properties, .compatible = "arm,cortex-a9-gic" },
  { .properties = &gicv2_of_properties, .compatible = "arm,eb11mp-gic" },
  { .properties = &gicv2_of_properties, .compatible = "arm,gic-400" },
  { .properties = &gicv2_of_properties, .compatible = "arm,pl390" },
  { .properties = &gicv2_of_properties, .compatible = "arm,tc11mp-gic" },
  { .properties = &gicv2_of_properties, .compatible = "brcm,brahma-b15-gic" },
  { .properties = &gicv2_of_properties, .compatible = "nvidia,tegra210-agic" },
  { .properties = &gicv2_of_properties, .compatible = "qcom,msm-8660-qgic" },
  { .properties = &gicv2_of_properties, .compatible = "qcom,msm-qgic2" }
};

static const struct of_dev_match gicv2_of_dev_match = {
  .ids = gicv2_of_dev_ids,
  .num_ids = sizeof(gicv2_of_dev_ids)/sizeof(struct of_dev_id),
  .max_num_matches = 1
};

int gicv2_init(void) 
{ 
  GIC_DEBUG("init\n");
  return of_for_each_match(&gicv2_of_dev_match, gicv2_init_dev_info);
  GIC_DEBUG("initialized\n");
}

