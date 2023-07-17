
#include<dev/gicv3.h>
#include<dev/gic_common.h>

#include<nautilus/nautilus.h>
#include<nautilus/fdt/fdt.h>
#include<nautilus/endian.h>

#include<arch/arm64/sys_reg.h>

#include<lib/bitops.h>

#ifndef NAUT_CONFIG_DEBUG_PRINTS
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

#define GIC_PRINT(fmt, args...) INFO_PRINT("[GICv3] " fmt, ##args)
#define GIC_DEBUG(fmt, args...) DEBUG_PRINT("[GICv3] " fmt, ##args)
#define GIC_ERROR(fmt, args...) ERROR_PRINT("[GICv3] " fmt, ##args)
#define GIC_WARN(fmt, args...) WARN_PRINT("[GICv3] " fmt, ##args)

#ifdef NAUT_CONFIG_ENABLE_ASSERTS
  #define INVALID_INT_NUM(gic, num) (num >= (gic).max_int_num)
#else
  #define INVALID_INT_NUM(gic, num) 0
#endif

#define ASSERT_SIZE(type, size) _Static_assert(sizeof(type) == size, \
    "sizeof("#type") is not "#size" bytes!\n") 

#define GICR_DEFAULT_STRIDE 0x20000

#define GICR_SGI_BASE 0x10000

#define GICR_CTLR_OFFSET 0x0
#define GICR_PROPBASE_OFFSET 0x70
#define GICR_ISENABLER_0_OFFSET (GICR_SGI_BASE+0x100)
#define GICR_ICENABLER_0_OFFSET (GICR_SGI_BASE+0x180)
#define GICR_ISPENDR_0_OFFSET (GICR_SGI_BASE+0x200)
#define GICR_ICPENDR_0_OFFSET (GICR_SGI_BASE+0x280)
#define GICR_ISACTIVER_0_OFFSET (GICR_SGI_BASE+0x300)

typedef struct gicv3 {

  // Required
  uint64_t dist_base;
  uint64_t dist_size;

  uint_t num_redist_regions;
  uint64_t redist_stride;
  uint64_t *redist_bases;
  uint64_t *redist_sizes;

  uint32_t num_redists;

  // Optional
  uint64_t cpu_base;
  uint64_t cpu_size;
  uint64_t hyper_base;
  uint64_t hyper_size;
  uint64_t virt_base;
  uint64_t virt_size;
  
  const char *compatible_string;

  uint64_t max_int_num;
  uint_t num_security_states;

} gicv3_t;

static gicv3_t __gic;

static inline uint32_t gicr_read32(gicv3_t *gic, uint64_t offset, uint32_t cpu) {
  uint32_t region = (cpu >> 5); // 32 CPU's per region
  if(cpu >= gic->num_redists || region >= gic->num_redist_regions) {
    GIC_ERROR("Trying to access CPU redistributor which does not exist: cpu = %u, region = %u\n", cpu, region);
    return -1;
  } else {
    volatile uint32_t *ptr = (volatile uint32_t *)(gic->redist_bases[region] + (cpu * gic->redist_stride) + offset);
#ifdef NAUT_CONFIG_ENABLE_ASSERTS
    if(((void*)ptr) < (void*)(gic->redist_bases[region]) || ((void*)ptr) >= (void*)(gic->redist_bases[region] + gic->redist_sizes[region])) {
      GIC_ERROR("Assertion Failed: Trying to access GIC redistributor outside of the MMIO region bounds!\n");
      return -1;
    }
#endif
    return *ptr;
  }
}

static inline int gicr_write32(uint32_t val, gicv3_t *gic, uint64_t offset, uint32_t cpu) {
  uint32_t region = (cpu >> 5);
  if(cpu >= gic->num_redists || region >= gic->num_redist_regions) {
    GIC_ERROR("Trying to write to CPU redistributor which does not exist: cpu = %u, region = %u\n", cpu, region);
    return -1;
  } else {
    volatile uint32_t *ptr = (volatile uint32_t *)(gic->redist_bases[region] + (cpu * gic->redist_stride) + offset);
#ifdef NAUT_CONFIG_ENABLE_ASSERTS
    if(((void*)ptr) < (void*)(gic->redist_bases[region]) || ((void*)ptr) >= (void*)(gic->redist_bases[region] + gic->redist_sizes[region])) {
      GIC_ERROR("Assertion Failed: Trying to write to GIC redistributor outside of the MMIO region bounds!\n");
      return -1;
    }
#endif
    *ptr = val;
    return 0;
  }
}

static inline uint64_t gicr_read64(gicv3_t *gic, uint64_t offset, uint32_t cpu) {
  uint32_t region = (cpu >> 5); // 32 CPU's per region
  if(cpu >= gic->num_redists || region >= gic->num_redist_regions) {
    GIC_ERROR("Trying to access CPU redistributor which does not exist: cpu = %u, region = %u\n", cpu, region);
    return -1;
  } else {
    volatile uint64_t *ptr = (volatile uint64_t *)(gic->redist_bases[region] + (cpu * gic->redist_stride) + offset);
#ifdef NAUT_CONFIG_ENABLE_ASSERTS
    if(((void*)ptr) < (void*)(gic->redist_bases[region]) || ((void*)ptr) >= (void*)(gic->redist_bases[region] + gic->redist_sizes[region])) {
      GIC_ERROR("Assertion Failed: Trying to access GIC redistributor outside of the MMIO region bounds!\n");
      return -1;
    }
#endif
    return *ptr;
  }
}

static inline int gicr_write64(uint64_t val, gicv3_t *gic, uint64_t offset, uint32_t cpu) {
  uint32_t region = (cpu >> 5);
  if(cpu >= gic->num_redists || region >= gic->num_redist_regions) {
    GIC_ERROR("Trying to write to CPU redistributor which does not exist: cpu = %u, region = %u\n", cpu, region);
    return -1;
  } else {
    volatile uint64_t *ptr = (volatile uint64_t *)(gic->redist_bases[region] + (cpu * gic->redist_stride) + offset);
#ifdef NAUT_CONFIG_ENABLE_ASSERTS
    if(((void*)ptr) < (void*)(gic->redist_bases[region]) || ((void*)ptr) >= (void*)(gic->redist_bases[region] + gic->redist_sizes[region])) {
      GIC_ERROR("Assertion Failed: Trying to write to GIC redistributor outside of the MMIO region bounds!\n");
      return -1;
    }
#endif
    *ptr = val;
    return 0;
  }
}

static inline int gicr_bitmap_set(uint32_t nr, gicv3_t *gic, uint64_t reg, uint32_t cpu) {
  uint32_t region = (cpu >> 5);
  if(cpu >= gic->num_redists || region >= gic->num_redist_regions) {
    GIC_ERROR("Trying to write to CPU redistributor which does not exist: cpu = %u, region = %u\n", cpu, region);
    return -1;
  } else {
    volatile uint64_t *ptr = (volatile uint64_t *)(gic->redist_bases[region] + (cpu * gic->redist_stride) + reg);
#ifdef NAUT_CONFIG_ENABLE_ASSERTS
    if(((void*)ptr) < (void*)(gic->redist_bases[region]) || ((void*)ptr) >= (void*)(gic->redist_bases[region] + gic->redist_sizes[region])) {
      GIC_ERROR("Assertion Failed: Trying to write to GIC redistributor outside of the MMIO region bounds!\n");
      return -1;
    }
#endif
    set_bit(nr, ptr);
    return 0;
  }
}

static inline int gicr_bitmap_clear(uint32_t nr, gicv3_t *gic, uint64_t reg, uint32_t cpu) {
  uint32_t region = (cpu >> 5);
  if(cpu >= gic->num_redists || region >= gic->num_redist_regions) {
    GIC_ERROR("Trying to write to CPU redistributor which does not exist: cpu = %u, region = %u\n", cpu, region);
    return -1;
  } else {
    volatile uint64_t *ptr = (volatile uint64_t *)(gic->redist_bases[region] + (cpu * gic->redist_stride) + reg);
#ifdef NAUT_CONFIG_ENABLE_ASSERTS
    if(((void*)ptr) < (void*)(gic->redist_bases[region]) || ((void*)ptr) >= (void*)(gic->redist_bases[region] + gic->redist_sizes[region])) {
      GIC_ERROR("Assertion Failed: Trying to write to GIC redistributor outside of the MMIO region bounds!\n");
      return -1;
    }
#endif
    clear_bit(nr, ptr);
    return 0;
  }
}

static inline int gicr_bitmap_read(uint32_t nr, gicv3_t *gic, uint64_t reg, uint32_t cpu) {
  uint32_t region = (cpu >> 5);
  if(cpu >= gic->num_redists || region >= gic->num_redist_regions) {
    GIC_ERROR("Trying to write to CPU redistributor which does not exist: cpu = %u, region = %u\n", cpu, region);
    return -1;
  } else {
    volatile uint64_t *ptr = (volatile uint64_t *)(gic->redist_bases[region] + (cpu * gic->redist_stride) + reg);
#ifdef NAUT_CONFIG_ENABLE_ASSERTS
    if(((void*)ptr) < (void*)(gic->redist_bases[region]) || ((void*)ptr) >= (void*)(gic->redist_bases[region] + gic->redist_sizes[region])) {
      GIC_ERROR("Assertion Failed: Trying to write to GIC redistributor outside of the MMIO region bounds!\n");
      return -1;
    }
#endif
    return test_bit(nr, ptr);
  }
}
typedef union gicd_ctl_reg {
  uint32_t raw;
  struct {
    uint_t grp1_en : 1;
    uint_t grp1_aff_en : 1;
    uint_t __res0_0 : 2;
    uint_t aff_routing_en : 1;
    uint_t __res0_1 : 25;
    uint_t write_pending : 1;
  } isc;
  struct {
    uint_t grp0_en : 1;
    uint_t grp1_en : 1;
    uint_t __res0_0 : 1;
    uint_t aff_routing_en : 1;
    uint_t __raowi : 1;
    uint_t one_of_n_wakeup_en : 1;
    uint_t sgis_active_state_en : 1;
    uint_t __res0_1 : 22;
    uint_t write_pending : 1;
  } sss;
  struct {
    uint_t __unknown : 31;
    uint_t write_pending : 1;
  } common;
} gicd_ctl_reg_t;
ASSERT_SIZE(gicd_ctl_reg_t, 4);


typedef union gicd_type_reg {
  uint32_t raw;
  struct {
    uint_t it_lines_num : 5;
    uint_t cpus_without_aff_routing : 3;
    uint_t espi_impl : 1;
    uint_t nmi_impl : 1;
    uint_t security_ext_impl : 1;
    uint_t num_lpis : 5;
    uint_t msi_by_gicd : 1;
    uint_t lpi_impl : 1;
    uint_t direct_virt_lpi_inj_impl : 1;
    uint_t int_id_bits : 5;
    uint_t aff3_valid : 1;
    uint_t one_of_n_spi_impl : 1;
    uint_t rss_impl : 1;
    uint_t espi_range : 5;
  };
} gicd_type_reg_t;
ASSERT_SIZE(gicd_type_reg_t, 4);

typedef union icc_ctl_reg {
  uint64_t raw;
  struct {
    uint_t common_bin_point_reg : 1;
    uint_t multistep_eoi : 1;
    uint_t __res0_0 : 4;
    uint_t use_priority_mask_hint : 1;
    uint_t __res0_1 : 1;
    uint_t priority_bits_ro : 3;
    uint_t id_bits_ro : 3;
    uint_t sei_support_ro : 1;
    uint_t aff3_valid_ro : 1;
    uint_t __res0_2 : 2;
    uint_t range_sel_support_ro : 1;
    uint_t ext_int_id_range_ro : 1;
    // Rest reserved
  };
} icc_ctl_reg_t;
ASSERT_SIZE(icc_ctl_reg_t, 8);

typedef union gicr_ctl_reg {
  uint64_t raw;
  struct {
    uint_t lpis_en : 1;
    uint_t clr_en_supported : 1;
    uint_t lpi_invalidate_reg_supported : 1;
    uint_t write_pending : 1;
    uint_t __res0_0 : 20;
    uint_t disable_grp0_proc_sel : 1;
    uint_t disable_grp1_isc_proc_sel : 1;
    uint_t disable_grp1_sec_proc_sel : 1;
    uint_t __res0_1 : 4;
    uint_t upstream_write_pending : 1;
  };
} gicr_ctl_reg_t;
ASSERT_SIZE(gicr_ctl_reg_t, 8);

typedef union gicr_type_reg {
  uint64_t raw;
  struct {
  };
} gicr_type_reg_t;
ASSERT_SIZE(gicr_type_reg_t, 8);

typedef union gicr_base_reg {
  uint64_t raw;
  struct {
    uint_t id_bits : 5;
    uint_t __res0_0 : 2;
    uint_t inner_cache : 3;
    uint_t shareability : 2;
    uint64_t paddr : 40;
    uint_t __res0_1 : 4;
    uint_t outer_cache : 3;
    uint_t __res0_2 : 5;
  };
} gicr_base_reg_t;
ASSERT_SIZE(gicr_base_reg_t, 8);

static inline void gicd_wait_for_write(void) {
  gicd_ctl_reg_t gicd_ctl_reg;
  do {
    gicd_ctl_reg.raw = LOAD_GICD_REG(__gic, GICD_CTLR_OFFSET);
  } while(gicd_ctl_reg.common.write_pending);
}
static inline void curr_cpu_gicr_wait_for_write(void) {
  gicr_ctl_reg_t gicr_ctl_reg;
  do {
    gicr_ctl_reg.raw = gicr_read64(&__gic, GICR_CTLR_OFFSET, my_cpu_id());
  } while(gicr_ctl_reg.upstream_write_pending || gicr_ctl_reg.write_pending);
}

static const char *gicv3_compat_list[] = {
  "arm,gic-v3",
  "qcom,msm8996-gic-v3"
};

static int dtb_find_gicv3(gicv3_t *gic, uint64_t dtb) {

  int offset = fdt_node_offset_by_prop_value((void*)dtb, -1, "interrupt-controller", NULL, NULL);

  while(offset >= 0) { 
    int compat_result = 1;
    for(uint_t i = 0; i < sizeof(gicv3_compat_list)/sizeof(const char*); i++) {
      compat_result &= fdt_node_check_compatible((void*)dtb, offset, gicv3_compat_list[i]);
      if(!compat_result) {
        gic->compatible_string = gicv3_compat_list[i];
        GIC_DEBUG("Found compatible GICv3 controller in device tree: %s\n", gic->compatible_string);
        break;
      }
    }
    if(compat_result) {
      GIC_DEBUG("Found interrupt controller found in the device tree which is not compatible with GICv3!\n");
    } else {
      break;
    }
    offset = fdt_node_offset_by_prop_value((void*)dtb, offset, "interrupt-controller", NULL, NULL);
  }

  if(offset < 0) {
    GIC_ERROR("Could not find a compatible interrupt controller in the device tree!\n");
    return -1;
  }

  int lenp = 0;
  void *gicr_regions_p = fdt_getprop(dtb, offset, "#redistributor-regions", &lenp);
  if(gicr_regions_p != NULL) {
    if(lenp == 4) {
        gic->num_redist_regions = be32toh(*(uint32_t *)gicr_regions_p);
    } else if(lenp == 8) {
        gic->num_redist_regions = be64toh(*(uint32_t *)gicr_regions_p);
    }
  } else {
    gic->num_redist_regions = 1;
  }

  gic->redist_bases = malloc(sizeof(typeof(*gic->redist_bases)) * gic->num_redist_regions);
  gic->redist_sizes = malloc(sizeof(typeof(*gic->redist_sizes)) * gic->num_redist_regions);

  void *gicr_stride_p = fdt_getprop(dtb, offset, "#redistributor-stride", &lenp);
  if(gicr_regions_p != NULL) {
    if(lenp == 4) {
        gic->redist_stride = be32toh(*(uint32_t *)gicr_stride_p);
    } else if(lenp == 8) {
        gic->redist_stride = be64toh(*(uint64_t *)gicr_stride_p);
    }
  } else {
    gic->redist_stride = GICR_DEFAULT_STRIDE;
  }

  int num_read = 4 + gic->num_redist_regions;
  fdt_reg_t reg[num_read];

  int getreg_result = fdt_getreg_array((void*)dtb, offset, reg, &num_read);

  if(!getreg_result && num_read >= 1+gic->num_redist_regions) {

      // Required MMIO Regions
      for(uint_t i = 0; i < gic->num_redist_regions; i++) {
        gic->redist_bases[i] = reg[1+i].address;
        gic->redist_sizes[i] = reg[1+i].size;
        GIC_PRINT("GICR_BASE_%u = %p, GICR_SIZE_%u = 0x%x\n", i, gic->redist_bases[i], i, gic->redist_sizes[i]);
      }
      gic->dist_base = reg[0].address;
      gic->dist_size = reg[0].size;
      GIC_PRINT("GICD_BASE = %p, GICD_SIZE = 0x%x\n", gic->dist_base, gic->dist_size);

      // Optional extra MMIO regions
      if(num_read >= 2+gic->num_redist_regions) {
        gic->cpu_base = reg[num_read-3].address;
        gic->cpu_size = reg[num_read-3].size;
        GIC_PRINT("GICC_BASE = %p, GICC_SIZE = 0x%x\n", gic->cpu_base, gic->cpu_size);
      }
      if(num_read >= 3+gic->num_redist_regions) {
        gic->hyper_base = reg[num_read-2].address;
        gic->hyper_size = reg[num_read-2].size;
        GIC_PRINT("GICH_BASE = %p, GICH_SIZE = 0x%x\n", gic->hyper_base, gic->hyper_size);
      } 
      if(num_read >= 4+gic->num_redist_regions) {
        gic->virt_base = reg[num_read-1].address;
        gic->virt_size = reg[num_read-1].size;
        GIC_PRINT("GICV_BASE = %p, GICV_SIZE = 0x%x\n", gic->virt_base, gic->virt_size);
      }
      if(num_read > 4+gic->num_redist_regions) {
        GIC_WARN("Read more REG entries in device tree than expected! num_read = %d\n", num_read);
      }
  } else {
    GIC_ERROR("GICv3 found in device tree has invalid or missing REG property! num_read = %d, gic->num_redist_regions = %u\n", num_read, gic->num_redist_regions);
    return -1;
  }

  return 0;
}

int gicv3_init(uint64_t dtb) {

  memset(&__gic, 0, sizeof(gicv3_t));

  // Offset should now point to the GICv3 entry in the device tree
  if(dtb_find_gicv3(&__gic, dtb)) {
    GIC_ERROR("Could not find GICv3 in device tree: global initialization failed!\n");
    return -1;
  }

  // There are better ways to do this but if there isn't exactly 1 redistributor
  // per CPU it's not going to work anyways
  extern struct naut_info nautilus_info;
  __gic.num_redists = nautilus_info.sys.num_cpus;

  gicd_type_reg_t gicd_type_reg;
  gicd_type_reg.raw = LOAD_GICD_REG(__gic, GICD_TYPER_OFFSET);
  __gic.max_int_num = (gicd_type_reg.it_lines_num + 1) << 6;
  __gic.num_security_states = gicd_type_reg.security_ext_impl ? 2 : 1;
  GIC_PRINT("max_int_num = %u\n", __gic.max_int_num);
  GIC_PRINT("num_security_states = %u\n", __gic.num_security_states);

  /*
  uint64_t gicr_prop_mem_bytes = (1<<(gicd_type_reg.int_id_bits)) - 8192;
  void *allocated_propbase = GIC_MALLOC_MEM(gicr_prop_mem_bytes);
  if(!allocated_propbase) {
    GIC_ERROR("Failed to allocate the GICR properties memory region!\n");
    return -1;
  }

  memset(allocated_propbase, 0, gicr_prop_mem_bytes);

  gicr_base_reg_t propbase;
  propbase.raw = LOAD_GICR_REG(__gic, GICR_PROPBASE_OFFSET);
  propbase.paddr = allocated_propbase;
  STORE_GICR_REG(__gic, GICR_PROPBASE_OFFSET, propbase.raw);

  */
  gicd_ctl_reg_t ctl;
  ctl.raw = LOAD_GICD_REG(__gic, GICD_CTLR_OFFSET);
  
  switch(__gic.num_security_states) {
    case 1:

      GIC_DEBUG("Configuring Distributor for Single Security State System\n");
      // Disable all groups (required)
      ctl.sss.grp0_en = 0;
      ctl.sss.grp1_en = 0;
      STORE_GICD_REG(__gic, GICD_CTLR_OFFSET, ctl.raw);
      gicd_wait_for_write();

      // Enable affinity routing
      ctl.raw = LOAD_GICD_REG(__gic, GICD_CTLR_OFFSET);
      ctl.sss.aff_routing_en = 1;
      STORE_GICD_REG(__gic, GICD_CTLR_OFFSET, ctl.raw);
      gicd_wait_for_write();

      // Re-enable groups (and other config)
      ctl.raw = LOAD_GICD_REG(__gic, GICD_CTLR_OFFSET);
      ctl.sss.grp0_en = 1;
      ctl.sss.grp1_en = 1;
      ctl.sss.one_of_n_wakeup_en = 0;
      ctl.sss.sgis_active_state_en = 1;
      STORE_GICD_REG(__gic, GICD_CTLR_OFFSET, ctl.raw);
      gicd_wait_for_write();

      break;

    case 2:

      GIC_DEBUG("Configuring Insecure Distributor for Two Security State System\n");
      ctl.isc.grp1_en = 0;
      ctl.isc.grp1_aff_en = 0;
      STORE_GICD_REG(__gic, GICD_CTLR_OFFSET, ctl.raw);
      gicd_wait_for_write();

      ctl.raw = LOAD_GICD_REG(__gic, GICD_CTLR_OFFSET);
      ctl.isc.aff_routing_en = 1;
      STORE_GICD_REG(__gic, GICD_CTLR_OFFSET, ctl.raw);
      gicd_wait_for_write();

      ctl.raw = LOAD_GICD_REG(__gic, GICD_CTLR_OFFSET);
      ctl.isc.grp1_en = 0;
      ctl.isc.grp1_aff_en = 1;
      STORE_GICD_REG(__gic, GICD_CTLR_OFFSET, ctl.raw);
      gicd_wait_for_write();

      break;

    default:
      GIC_ERROR("Invalid number of security states for GICv3: %u!\n", __gic.num_security_states);
      return -1;
  }

  gic_dump_state();
  GIC_PRINT("inited globally\n");

  return 0;
}

int per_cpu_init_gic(void) {

  uint64_t icc_sre;
  LOAD_SYS_REG(ICC_SRE_EL1, icc_sre);
  icc_sre |= 1; // Enable ICC Register Access
  STORE_SYS_REG(ICC_SRE_EL1, icc_sre);

  asm volatile ("isb");

  icc_ctl_reg_t ctl_reg;
  LOAD_SYS_REG(ICC_CTLR_EL1, ctl_reg.raw);
  ctl_reg.multistep_eoi = 0; // EOI Priority Drop = Deactivation
  STORE_SYS_REG(ICC_CTLR_EL1, ctl_reg.raw);

  uint64_t icc_pmr = 0xFF;
  STORE_SYS_REG(ICC_PMR_EL1, icc_pmr);

  STORE_SYS_REG(ICC_IGRPEN0_EL1, 1);
  STORE_SYS_REG(ICC_IGRPEN1_EL1, 1);

  GIC_DEBUG("per_cpu GICv3 initialized\n");

  return 0;
}

static inline int is_spurrious_int(uint32_t id) {
  return ((id & (~0b11)) ^ (0x3FC)) == 0;
}

void gic_ack_int(gic_int_info_t *info) {
  uint32_t int_id;
  LOAD_SYS_REG(ICC_IAR0_EL1, int_id);
  if(is_spurrious_int(int_id)) {
    LOAD_SYS_REG(ICC_IAR1_EL1, int_id);
    if(is_spurrious_int(int_id)) {
      info->group = -1;
      GIC_WARN("Spurrious interrupt occurred!\n");
    } else {
      info->group = 1;
    }
  } else {
    info->group = 0;
  }
  info->int_id = int_id;
}

void gic_end_of_int(gic_int_info_t *info) {
  switch(info->group) {
    case 0:
      STORE_SYS_REG(ICC_EOIR0_EL1, info->int_id);
      break;
    case 1:
      STORE_SYS_REG(ICC_EOIR1_EL1, info->int_id);
      break;
    case -1:
    default:
      // Do nothing
  }
}

int gic_enable_int(uint_t irq) {
  if(irq < 32) {
    gicr_bitmap_set(irq, &__gic, GICR_ISENABLER_0_OFFSET, my_cpu_id());
  } else {
    GICD_BITMAP_SET(__gic, irq, GICD_ISENABLER_0_OFFSET);
  }
  return 0;
}

int gic_disable_int(uint_t irq) {
  if(irq < 32) {
    gicr_bitmap_set(irq, &__gic, GICR_ICENABLER_0_OFFSET, my_cpu_id());
  } else {
    GICD_BITMAP_SET(__gic, irq, GICD_ICENABLER_0_OFFSET);
  }
  return 0;
}
int gic_int_enabled(uint_t irq) {
  if(irq < 32) {
    return gicr_bitmap_read(irq, &__gic, GICR_ISENABLER_0_OFFSET, my_cpu_id());
  } else {
    return GICD_BITMAP_READ(__gic, irq, GICD_ISENABLER_0_OFFSET);
  }
}

int gic_clear_int_pending(uint_t irq) {
  if(irq < 32) {
    gicr_bitmap_set(irq, &__gic, GICR_ICPENDR_0_OFFSET, my_cpu_id());
  } else {
    GICD_BITMAP_SET(__gic, irq, GICD_ICPENDR_0_OFFSET);
  }
  return 0;
}

static int gic_int_pending(uint_t irq) {
  if(irq < 32) {
    return gicr_bitmap_read(irq, &__gic, GICR_ISPENDR_0_OFFSET, my_cpu_id());
  } else {
    return GICD_BITMAP_READ(__gic, irq, GICD_ISPENDR_0_OFFSET);
  }
}

void gic_set_target_all(uint_t irq) {
  // TODO
}

static int gic_int_active(uint_t irq) {
  if(irq < 32) {
    return gicr_bitmap_read(irq, &__gic, GICR_ISACTIVER_0_OFFSET, my_cpu_id());
  }
  else {
    return GICD_BITMAP_READ(__gic, irq, GICD_ISACTIVER_0_OFFSET);
  }
}

