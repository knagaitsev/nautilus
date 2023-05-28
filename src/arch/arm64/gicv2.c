
#include<arch/arm64/gic.h>

#include<nautilus/nautilus.h>
#include<nautilus/fdt.h>

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

// Distributor Register Offsets
#define GICD_CTLR_OFFSET 0x0
#define GICD_TYPER_OFFSET 0x4
#define GICD_IIDR_OFFSET 0x8
#define GICD_SGIR_OFFSET 0xF00
#define GICD_IGROUPR_0_OFFSET 0x80
#define GICD_ISENABLER_0_OFFSET 0x100
#define GICD_ICENABLER_0_OFFSET 0x180
#define GICD_ISPENDR_0_OFFSET 0x200
#define GICD_ICPENDR_0_OFFSET 0x280
#define GICD_ISACTIVER_0_OFFSET 0x300
#define GICD_ICACTIVER_0_OFFSET 0x380
#define GICD_IPRIORITYR_0_OFFSET 0x400
#define GICD_ITARGETSR_0_OFFSET 0x800
#define GICD_ICFGR_0_OFFSET 0xC00
#define GICD_NSACR_0_OFFSET 0xE00
#define GICD_CPENDSGIR_0_OFFSET 0xF10
#define GICD_SPENDSGIR_0_OFFSET 0xF20

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

typedef struct gic_msi_frame {
  uint64_t mmio_base;

  uint64_t spi_base;
  uint64_t spi_num;
} gic_msi_frame_t;

typedef struct gic {

  uint64_t dist_base;
  uint64_t cpu_base;

  // Technically there could be multiple of these
  // but for now we're just going to support a single
  // msi frame
  gic_msi_frame_t msi_frame;

  uint32_t max_irq;
  uint32_t cpu_num;

  uint8_t security_ext;

  char *compatible_string;

} gic_t;

static gic_t __gic;

#define LOAD_GICD_REG(gic, offset) *(volatile uint32_t*)((gic).dist_base + offset)
#define STORE_GICD_REG(gic, offset, val) (*(volatile uint32_t*)((gic).dist_base + offset)) = val
#define GICD_BITMAP_SET(gic, num, offset) \
  *(((volatile uint32_t*)((gic).dist_base + offset))+(num >> 5)) |= (1<<(num&(0x1F)))
#define GICD_BITMAP_READ(gic, num, offset) \
  (!!(*(((volatile uint32_t*)((gic).dist_base + offset))+(num >> 5)) & (1<<(num&(0x1F)))))

#define GICD_DUALBITMAP_READ(gic, num, offset) \
  (*(((volatile uint8_t*)((gic).dist_base + offset))+(num >> 4)) & (0x3<<((num&0xF)<<4)))

#define GICD_BYTEMAP_READ(gic, num, offset) \
  (*(((volatile uint8_t*)((gic).dist_base + offset))+(num >> 2)) & (0xFF<<((num&0x3)<<3)))

#define LOAD_GICC_REG(gic, offset) *(volatile uint32_t*)((gic).cpu_base + offset)
#define STORE_GICC_REG(gic, offset, val) (*(volatile uint32_t*)((gic).cpu_base + offset)) = val

#define LOAD_MSI_REG(gic, offset) *(volatile uint32_t*)((gic).msi_frame.mmio_base + offset)
#define STORE_MSI_REG(gic, offset, val) (*(volatile uint32_t*)((gic).msi_frame.mmio_base + offset)) = val

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
    uint_t int_id : 10;
    uint_t cpu_id : 3;
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

static char *gicv2_fdt_node_name_list[] = {
        "intc",
        "interrupt-controller"
};
static char *gicv2_compat_list[] = {
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

int global_init_gic(uint64_t dtb) {

  memset(&__gic, 0, sizeof(gic_t));

  // Find node with "interrupt-controller" property
  int offset = fdt_node_offset_by_prop_value((void*)dtb, -1, "interrupt-controller", NULL, NULL);

  while(offset >= 0) { 
    int compat_result = 1;
    for(uint_t i = 0; i < sizeof(gicv2_compat_list)/sizeof(const char*); i++) {
      compat_result &= fdt_node_check_compatible((void*)dtb, offset, gicv2_compat_list[i]);
      if(!compat_result) {
        __gic.compatible_string = gicv2_compat_list[i];
        GIC_DEBUG("Found compatible GICv2 controller in device tree: %s\n", __gic.compatible_string);
        break;
      }
    }
    if(compat_result) {
      GIC_DEBUG("Found interrupt controller found in the device tree which is not compatible with GICv2!\n");
    } else {
      break;
    }
    offset = fdt_node_offset_by_prop_value((void*)dtb, offset, "interrupt-controller", NULL, NULL);
  }

  if(offset < 0) {
    GIC_ERROR("Could not find a compatible interrupt controller in the device tree!\n");
    return -1;
  }

  fdt_reg_t reg[2];
  int getreg_result = fdt_getreg_array((void*)dtb, offset, reg, 2);

  if(!getreg_result) {
     __gic.dist_base = reg[0].address;
     __gic.cpu_base = reg[1].address;
    GIC_DEBUG("GICD_BASE = 0x%x GICC_BASE = 0x%x\n", __gic.dist_base, __gic.cpu_base);
  } else {
    GIC_ERROR("Invalid reg field in the device tree!\n");
    return -1;
  }

  gicd_type_reg_t d_type_reg;
  d_type_reg.raw = LOAD_GICD_REG(__gic, GICD_TYPER_OFFSET);
  __gic.max_irq = 32*(d_type_reg.it_lines_num + 1);
  __gic.cpu_num = d_type_reg.cpu_num;
  __gic.security_ext = d_type_reg.sec_ext_impl;

  GIC_DEBUG("max irq = %u, cpu num = %u, security extensions = %s\n", __gic.max_irq, __gic.cpu_num, 
      __gic.security_ext ? "ENABLED" : "DISABLED");

  gicd_ctl_reg_t d_ctl_reg;
  d_ctl_reg.raw = LOAD_GICD_REG(__gic, GICD_CTLR_OFFSET);
  d_ctl_reg.grp0_en = 1;
  STORE_GICD_REG(__gic, GICD_CTLR_OFFSET, d_ctl_reg.raw);

  GIC_DEBUG("Looking for MSI(-X) Support\n");
  int msi_offset = fdt_node_offset_by_compatible((void*)dtb, -1, "arm,gic-v2m-frame");
  while(msi_offset >= 0) {

    if(__gic.msi_frame.mmio_base) {
      GIC_WARN("Another MSI Frame was found in the device tree, which is not supported yet, ignoring...\n");
      msi_offset = fdt_node_offset_by_compatible((void*)dtb, msi_offset, "arm,gic-v2m-frame");
      continue;
    }
    GIC_DEBUG("Found compatible msi-controller frame in device tree!\n");

    fdt_reg_t msi_reg = { .address = 0, .size = 0 };
    fdt_getreg((void*)dtb, msi_offset, &msi_reg);
    __gic.msi_frame.mmio_base = msi_reg.address;
    GIC_DEBUG("MSI_BASE = 0x%x\n", __gic.msi_frame.mmio_base);

    int msi_info_not_found = 0;
    
    // We first try to get this info from the DTB
    struct fdt_property *prop = fdt_get_property_namelen((void*)dtb, msi_offset, "arm,msi-base-spi", 16, NULL); 
    if(prop) {
      GIC_DEBUG("Found MSI Base SPI in FDT\n");
      __gic.msi_frame.spi_base = bswap_32(*(uint32_t*)prop->data);
    } else {
      GIC_WARN("Failed to find MSI Base SPI in the device tree\n");
      msi_info_not_found = 1;
    }
    prop = fdt_get_property_namelen((void*)dtb, msi_offset, "arm,msi-num-spis", 16, NULL); 
    if(prop) {
      GIC_DEBUG("Found MSI SPI Number in FDT\n");
      __gic.msi_frame.spi_num = bswap_32(*(uint32_t*)prop->data);
    } else {
      GIC_WARN("Failed to find MSI SPI Number in the device tree\n");
      msi_info_not_found = 1;
    }
    
    if(msi_info_not_found) {
      
      // If the device tree doesn't say, look at the MSI_TYPER
      // (Finding documentation on GICv2m is awful, so we're trusting 
      //  the linux implementation)
      GIC_DEBUG("Could not find all MSI info in the device tree so checking MMIO MSI registers\n");

      msi_type_reg_t msi_type;
      msi_type.raw = LOAD_MSI_REG(__gic, MSI_TYPER_OFFSET);
      GIC_DEBUG("MSI_TYPER = 0x%08x\n", msi_type.raw);
      
      __gic.msi_frame.spi_base = msi_type.first_int_id;
      __gic.msi_frame.spi_num = msi_type.spi_num;
      GIC_DEBUG("MSI SPI Range : [%u - %u]\n", __gic.msi_frame.spi_base, __gic.msi_frame.spi_base+__gic.msi_frame.spi_num-1);
      if(!__gic.msi_frame.spi_base || !__gic.msi_frame.spi_num) {
        GIC_DEBUG("Invalid MSI_TYPER: spi base = 0x%x, spid num = 0x%x\n",
          msi_type.first_int_id, msi_type.spi_num);
        GIC_ERROR("Failed to get MSI Information!\n");
        return -1;
      }
    }

    msi_offset = fdt_node_offset_by_compatible((void*)dtb, msi_offset, "arm,gic-v2m-frame");
  }

  GIC_PRINT("inited globally\n");

  return 0;
}

int per_cpu_init_gic(void) {

  gicc_ctl_reg_t c_ctl_reg;
  c_ctl_reg.raw = LOAD_GICC_REG(__gic, GICC_CTLR_OFFSET);

  if(__gic.security_ext) {
    c_ctl_reg.isc.grp1_en = 1;
    c_ctl_reg.isc.multistep_eoi = 0;
  } else {
    c_ctl_reg.sss.grp1_en = 1;
  }
  STORE_GICC_REG(__gic, GICC_CTLR_OFFSET, c_ctl_reg.raw);

  gicc_priority_reg_t prior_reg;
  prior_reg.raw = LOAD_GICC_REG(__gic, GICC_PMR_OFFSET);
  prior_reg.priority = 0xFF;
  STORE_GICC_REG(__gic, GICC_PMR_OFFSET, prior_reg.raw);

  GIC_DEBUG("per_cpu GICv2 initialized\n");

  return 0;
}

static inline int is_spurrious_int(uint32_t id) {
  return ((id & (~0b11)) ^ (0x3FC)) == 0;
}

uint16_t gic_max_irq(void) {
  return __gic.max_irq;
}

void gic_ack_int(gic_int_info_t *info) {
  gicc_int_info_reg_t info_reg;
  info_reg.raw = LOAD_GICC_REG(__gic, GICC_IAR_OFFSET);

  if(is_spurrious_int(info_reg.int_id)) {
    info->group = -1;
    GIC_WARN("Spurrious interrupt occurred!\n");
  }
  else {
    info->group = 1;
  }
  info->int_id = info_reg.int_id;
}

void gic_end_of_int(gic_int_info_t *info) {
  gicc_int_info_reg_t info_reg;
  info_reg.int_id = info->int_id;
  info_reg.cpu_id = info->cpu_id;
  STORE_GICC_REG(__gic, GICC_EOIR_OFFSET, info_reg.raw);
}

int gic_enable_int(uint_t irq) {
  GICD_BITMAP_SET(__gic, irq, GICD_ISENABLER_0_OFFSET);
  return 0;
}

int gic_disable_int(uint_t irq) {
  GICD_BITMAP_SET(__gic, irq, GICD_ICENABLER_0_OFFSET);
  return 0;
}
int gic_int_enabled(uint_t irq) {
  return GICD_BITMAP_READ(__gic, irq, GICD_ISENABLER_0_OFFSET);
}

int gic_clear_int_pending(uint_t irq) {
  GICD_BITMAP_SET(__gic, irq, GICD_ICPENDR_0_OFFSET);
  return 0;
}
int gic_int_pending(uint_t irq) {
  return GICD_BITMAP_READ(__gic, irq, GICD_ISPENDR_0_OFFSET);
}

int gic_int_active(uint_t irq) {
  return GICD_BITMAP_READ(__gic, irq, GICD_ISACTIVER_0_OFFSET);
}

static int gic_int_group(uint_t irq) {
  return GICD_BITMAP_READ(__gic, irq, GICD_IGROUPR_0_OFFSET);
}
static uint8_t gic_int_target(uint_t irq) {
  return GICD_BYTEMAP_READ(__gic, irq, GICD_ITARGETSR_0_OFFSET);
}
static int gic_int_is_edge_triggered(uint_t irq) {
  return 0b10 & GICD_DUALBITMAP_READ(__gic, irq, GICD_ICFGR_0_OFFSET);
}

uint32_t gic_num_msi_irq(void) {
  return __gic.msi_frame.spi_num;
}

uint32_t gic_base_msi_irq(void) {
  return __gic.msi_frame.spi_base;
}

void gic_dump_state(void) {

  gicd_ctl_reg_t ctl_reg;
  ctl_reg.raw = LOAD_GICD_REG(__gic, GICD_CTLR_OFFSET);
  GIC_PRINT("Distributor Control Register\n");
  GIC_PRINT("\traw = 0x%08x\n", ctl_reg.raw);

  for(uint_t i = 0; i < __gic.max_irq; i++) {
    GIC_PRINT("Int (%u) : %s%s%s(group %u) (target 0x%02x) (%s)\n", i,
      gic_int_enabled(i) ? "enabled " : "",
      gic_int_pending(i) ? "pending " : "",
      gic_int_active(i) ? "active " : "",
      gic_int_group(i),
      gic_int_target(i),
      gic_int_is_edge_triggered(i) ? "edge triggered" : "level sensitive"
      );
  }
}
