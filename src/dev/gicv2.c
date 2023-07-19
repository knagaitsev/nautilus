
#include<dev/gic.h>
#include<dev/gicv2.h>

#include<nautilus/nautilus.h>
#include<nautilus/irqdev.h>
#include<nautilus/interrupt.h>
#include<nautilus/endian.h>
#include<nautilus/fdt/fdt.h>

#include<arch/arm64/sys_reg.h>

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
        GIC_DEBUG("Found compatible GICv2 controller in device tree: %s\n", gicv2_compat_list[i]);
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

  GIC_PRINT("inited globally\n");

  return 0;
}

static int gicv2_dev_ivec_desc_init(struct nk_irq_dev *dev, struct gicv2 *gic) 
{

  // Set up 0-15 for SGI
  nk_alloc_ivec_descs(0, 16, dev, NK_IVEC_DESC_TYPE_DEFAULT, NK_IVEC_DESC_FLAG_PERCPU | NK_IVEC_DESC_FLAG_IPI);
  GIC_DEBUG("Set Up SGI Vectors\n");
  
  // Set up 16-31 for PPI
  nk_alloc_ivec_descs(16, 16, dev, NK_IVEC_DESC_TYPE_DEFAULT, NK_IVEC_DESC_FLAG_PERCPU);
  GIC_DEBUG("Set Up PPI Vectors\n");

  // Set up the correct number of SPI
  GIC_DEBUG("gic->max_spi = %u\n", gic->max_spi);
  nk_alloc_ivec_descs(32, gic->max_spi - 32, dev, NK_IVEC_DESC_TYPE_DEFAULT, 0);
  GIC_DEBUG("Set Up SPI Vectors\n");

  // Add the special INTIDS
  GIC_DEBUG("Set Up Special Interrupt Vectors!\n");
  nk_alloc_ivec_descs(1020, 4, dev, NK_IVEC_DESC_TYPE_SPURRIOUS, 0);

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

static inline int gicv2_is_spurrious_int(uint32_t id) {
  return ((id & (~0b11)) == 0x3FC);
}

static int gicv2_dev_ack_irq(void *state, nk_irq_t *irq) {

  struct gicv2 *gic = (struct gicv2 *)state;

  gicc_int_info_reg_t info_reg;
  info_reg.raw = GICC_LOAD_REG(gic, GICC_IAR_OFFSET);

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
  GICC_STORE_REG(gic, GICC_EOIR_OFFSET, info_reg.raw);

  return IRQ_DEV_EOI_SUCCESS;

}

static uint8_t irq_target_round_robin;

static int gicv2_dev_enable_irq(void *state, nk_irq_t irq) {

  struct gicv2 *gic = (struct gicv2*)state;
  GIC_DEBUG("Enabled IRQ: %u\n", irq);

  GICD_BITMAP_SET(gic, GICD_ISENABLER_0_OFFSET, irq);
  if(irq >= 32) {
    uint8_t target = __sync_fetch_and_add(&irq_target_round_robin, 1) % gic->num_cpus;
    GICD_BYTEMAP_WRITE(gic, GICD_ITARGETSR_0_OFFSET, irq, 1<<target);
    GIC_DEBUG("Set IRQ %u Target to CPU %u\n", irq, target);
  }
  return 0;

}

static int gicv2_dev_disable_irq(void *state, nk_irq_t irq) {

  GICD_BITMAP_SET((struct gicv2*)state, GICD_ICENABLER_0_OFFSET, irq);

  return 0;
}

static int gicv2_dev_irq_status(void *state, nk_irq_t irq) {

  struct gicv2 *gic = (struct gicv2*)state;

  int status = 0;

  status |= GICD_BITMAP_READ(gic, GICD_ISENABLER_0_OFFSET, irq) ?
    IRQ_DEV_STATUS_ENABLED : 0;

  status |= GICD_BITMAP_READ(gic, GICD_ISPENDR_0_OFFSET, irq) ?
    IRQ_DEV_STATUS_PENDING : 0;

  status |= GICD_BITMAP_READ(gic, GICD_ISACTIVER_0_OFFSET, irq) ?
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

#ifdef NAUT_CONFIG_GIC_VERSION_2M
#include<dev/gicv2m.h>
#endif

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

  const char *dev_name = "gicv2";

#ifdef NAUT_CONFIG_GIC_VERSION_2M
  if(gicv2m_dtb_init(dtb, state)) {
    GIC_WARN("Failed to initialize GICv2m MSI Support!\n");
  }
  dev_name = "gicv2m";
#endif

  struct nk_irq_dev *dev = nk_irq_dev_register(dev_name, 0, &gicv2_dev_int, (void*)state);

  if(dev == NULL) {
    GIC_ERROR("Failed to register as an IRQ device!\n");
    return -1;
  }

  if(gicv2_dev_ivec_desc_init(dev, state)) {
    GIC_ERROR("Failed to set up interrupt vector descriptors!\n");
    return -1;
  }

#ifdef NAUT_CONFIG_GIC_VERSION_2M
  if(gicv2m_ivec_init(dtb, state)) {
    GIC_WARN("Failed to set up interrupt vector MSI info!\n");
  }
#endif

  if(nk_assign_all_cpus_irq_dev(dev)) {
    GIC_ERROR("Failed to assign the GIC to all CPU's in the system!\n");
    return -1;
  }

  return 0;
}

