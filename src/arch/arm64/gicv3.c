
#include<arch/arm64/gic.h>

#include<nautilus/nautilus.h>
#include<nautilus/fdt.h>

#include<arch/arm64/sys_reg.h>

#ifndef NAUT_CONFIG_DEBUG_PRINTS
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

#define GIC_PRINT(fmt, args...) INFO_PRINT("[GICv3] " fmt, ##args)
#define GIC_DEBUG(fmt, args...) DEBUG_PRINT("[GICv3] " fmt, ##args)
#define GIC_ERROR(fmt, args...) ERROR_PRINT("[GICv3] " fmt, ##args)
#define GIC_WARN(fmt, args...) WARN_PRINT("[GICv3] " fmt, ##args)

#define GIC_MALLOC_MEM(size) mm_boot_alloc(size)

#ifdef NAUT_CONFIG_ENABLE_ASSERTS
  #define INVALID_INT_NUM(gic, num) (num >= (gic).max_num_ints)
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

#define GICR_CTLR_OFFSET 0x0
#define GICR_PROPBASE_OFFSET 0x70
#define GICR_ISENABLER_0_OFFSET 0x100
#define GICR_ICENABLER_0_OFFSET 0x180
#define GICR_ISPENDR_0_OFFSET 0x200
#define GICR_ICPENDR_0_OFFSET 0x280
#define GICR_ISACTIVER_0_OFFSET 0x300

typedef struct gic {
  uint64_t dist_base;
  uint64_t redist_base;
  uint64_t max_num_ints;
  uint8_t num_sec_states;
} gic_t;

static gic_t __gic;

#define LOAD_GICD_REG(gic, offset) *(volatile uint32_t*)((gic).dist_base + offset)
#define STORE_GICD_REG(gic, offset, val) (*(volatile uint32_t*)((gic).dist_base + offset)) = val
#define GICD_BITMAP_SET(gic, num, offset) \
  *(((volatile uint32_t*)((gic).dist_base + offset))+(num >> 5)) |= (1<<(num&(0x1F)))
#define GICD_BITMAP_READ(gic, num, offset) \
  (!!(*(((volatile uint32_t*)((gic).dist_base + offset))+(num >> 5)) & (1<<(num&(0x1F)))))

#define LOAD_GICR_REG(gic, offset) *(volatile uint32_t*)((gic).redist_base + offset)
#define STORE_GICR_REG(gic, offset, val) (*(volatile uint32_t*)((gic).redist_base + offset)) = val
#define GICR_BITMAP_SET(gic, num, offset) \
  *(((volatile uint32_t*)((gic).redist_base + offset))+(num >> 5)) |= (1<<(num&(0x1F)))
#define GICR_BITMAP_READ(gic, num, offset) \
  (!!(*(((volatile uint32_t*)((gic).redist_base + offset))+(num >> 5)) & (1<<(num&(0x1F)))))



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
static inline void gicr_wait_for_write(void) {
  gicr_ctl_reg_t gicr_ctl_reg;
  do {
    gicr_ctl_reg.raw = LOAD_GICR_REG(__gic, GICR_CTLR_OFFSET);
  } while(gicr_ctl_reg.upstream_write_pending || gicr_ctl_reg.write_pending);
}

int global_init_gic(uint64_t dtb) {

  memset(&__gic, 0, sizeof(gic_t));

  int offset = fdt_subnode_offset_namelen((void*)dtb, 0, "intc", 4);

  int compat_result = fdt_node_check_compatible((void*)dtb, offset, "arm,gic-v3");
  if(compat_result) {
    GIC_ERROR("Interrupt controller found in the device tree is not compatible with GICv3!\n");
    panic("Interrupt controller found in the device tree is not compatible with GICv3!\n");
  }

  fdt_reg_t reg[2];
  int getreg_result = fdt_getreg_array((void*)dtb, offset, reg, 2);

  if(!getreg_result) {
     __gic.dist_base = reg[0].address;
     __gic.redist_base = reg[1].address;
    GIC_PRINT("Found GICv3 in the device tree: GICD_BASE = 0x%x GICR_BASE = 0x%x\n", __gic.dist_base, __gic.redist_base);
  } else {
    GIC_ERROR("Could not find an interrupt controller in the device tree!\n");
    panic("Could not find an interrupt controller in the device tree!\n");
  }

  gicd_type_reg_t gicd_type_reg;
  gicd_type_reg.raw = LOAD_GICD_REG(__gic, GICD_TYPER_OFFSET);
  __gic.max_num_ints = (gicd_type_reg.it_lines_num + 1) << 6;
  __gic.num_sec_states = gicd_type_reg.security_ext_impl ? 2 : 1;
  GIC_PRINT("max_num_ints = %u\n", __gic.max_num_ints);
  GIC_PRINT("num_sec_states = %u\n", __gic.num_sec_states);

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

  gicd_ctl_reg_t ctl;
  ctl.raw = LOAD_GICD_REG(__gic, GICD_CTLR_OFFSET);
  
  switch(__gic.num_sec_states) {
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
      GIC_ERROR("Invalid number of security states for GICv3: %u!\n", __gic.num_sec_states);
      return -1;
  }

  GIC_PRINT("inited globally\n");

  return 0;
}

int per_cpu_init_gic(void) {

  uint32_t gicr_wake;
  gicr_wake = LOAD_GICR_REG(__gic, GICR_WAKER_OFFSET);
  if(gicr_wake & 0b10) {
    GIC_DEBUG("Processor's Redistributor thought it was sleeping, Waking...\n");
    gicr_wake |= 0b10;
    STORE_GICR_REG(__gic, GICR_WAKER_OFFSET);
  }
  do {
    // Wait for the children to be marked as awake
    gicr_wake = LOAD_GICR_REG(__gic, GICR_WAKER_OFFSET);
  } while(gicr_wake & 0b100);
  GIC_DEBUG("Redistributor is aware the processor is awake.\n");


  uint64_t icc_sre;
  LOAD_SYS_REG(ICC_SRE_EL1, icc_sre);
  icc_sre |= 1;
  STORE_SYS_REG(ICC_SRE_EL1, icc_sre);

  icc_ctl_reg_t ctl_reg;
  LOAD_SYS_REG(ICC_CTLR_EL1, ctl_reg.raw);
  ctl_reg.multistep_eoi = 0;
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
    //It's either an SGI or PPI
    GICR_BITMAP_SET(__gic, irq, GICR_ISENABLER_0_OFFSET);
  } else {
    GICD_BITMAP_SET(__gic, irq, GICD_ISENABLER_0_OFFSET);
  }
  return 0;
}

int gic_disable_int(uint_t irq) {
  if(irq < 32) {
    GICR_BITMAP_SET(__gic, irq, GICR_ICENABLER_0_OFFSET);
  } else {
    GICD_BITMAP_SET(__gic, irq, GICD_ICENABLER_0_OFFSET);
  }
  return 0;
}
int gic_int_enabled(uint_t irq) {
  if(irq < 32) {
    return GICR_BITMAP_READ(__gic, irq, GICR_ISENABLER_0_OFFSET);
  } else {
    return GICD_BITMAP_READ(__gic, irq, GICD_ISENABLER_0_OFFSET);
  }
}

int gic_clear_int_pending(uint_t irq) {
  if(irq < 32) {
    GICR_BITMAP_SET(__gic, irq, GICR_ICPENDR_0_OFFSET);
  } else {
    GICD_BITMAP_SET(__gic, irq, GICD_ICPENDR_0_OFFSET);
  }
  return 0;
}
int gic_int_pending(uint_t irq) {
  if(irq < 32) {
    return GICR_BITMAP_READ(__gic, irq, GICR_ISPENDR_0_OFFSET);
  } else {
    return GICD_BITMAP_READ(__gic, irq, GICD_ISPENDR_0_OFFSET);
  }
}

int gic_int_active(uint_t irq) {
  if(irq < 32) {
    return GICR_BITMAP_READ(__gic, irq, GICR_ISACTIVER_0_OFFSET);
  }
  else {
    return GICD_BITMAP_READ(__gic, irq, GICD_ISACTIVER_0_OFFSET);
  }
}



void gic_dump_state(void) {

  GIC_PRINT("Maximum Number of Interrupts = %u (Not Counting LPI's)\n", __gic.max_num_ints);
  GIC_PRINT("Number of Security States = %u\n", __gic.num_sec_states);

  gicd_ctl_reg_t ctl_reg;
  ctl_reg.raw = LOAD_GICD_REG(__gic, GICD_CTLR_OFFSET);
  GIC_PRINT("Distributor Control Register\n");
  GIC_PRINT("\traw = 0x%08x\n", ctl_reg.raw);
  GIC_PRINT("\twrite pending = %u\n", ctl_reg.common.write_pending);

  switch(__gic.num_sec_states) {
    case 1:
      break;
    case 2:
      break;
    default:
      GIC_WARN("INVALID NUMBER OF SECURITY STATES!\n");
      break;
  }

  uint64_t reg;
  LOAD_SYS_REG(ICC_CTLR_EL1, reg);
  GIC_PRINT("ICC_CTLR_EL1 = 0x%08x\n", reg);
  LOAD_SYS_REG(ICC_PMR_EL1, reg);
  GIC_PRINT("ICC_PMR_EL1 = 0x%08x\n", reg);
  LOAD_SYS_REG(ICC_IGRPEN0_EL1, reg);
  GIC_PRINT("ICC_IGRPEN0_EL1 = 0x%08x\n", reg);
  LOAD_SYS_REG(ICC_IGRPEN1_EL1, reg);
  GIC_PRINT("ICC_IGPREN1_EL1 = 0x%08x\n", reg);

  GIC_PRINT("Redistributor Enabled Interrupts:\n");
  for(uint_t i = 32; i < 32; i++) {
    if(gic_int_enabled(i)) {
      GIC_PRINT("\tINTERRUPT %u: ENABLED\n", i);
    } else {
 //     GIC_PRINT("\tINTERRUPT %u: DISABLED\n", i);
    }
  }

  GIC_PRINT("Redistributor Pending Interrupts:\n");
  for(uint_t i = 0; i < 32; i++) {
    if(gic_int_pending(i)) {
      GIC_PRINT("\tINTERRUPT %u: PENDING\n", i);
    } else {
 //     GIC_PRINT("\tINTERRUPT %u: NOT PENDING\n", i);
    }
  }

  GIC_PRINT("Redistributor Active Interrupts:\n");
  for(uint_t i = 0; i < 32; i++) {
    if(gic_int_active(i)) {
      GIC_PRINT("\tINTERRUPT %u: ACTIVE\n", i);
    } else {
//      GIC_PRINT("\tINTERRUPT %u: INACTIVE\n", i);
    }
  }

  GIC_PRINT("Distributor Enabled Interrupts:\n");
  for(uint_t i = 32; i < __gic.max_num_ints; i++) {
    if(gic_int_enabled(i)) {
      GIC_PRINT("\tINTERRUPT %u: ENABLED\n", i);
    } else {
 //     GIC_PRINT("\tINTERRUPT %u: DISABLED\n", i);
    }
  }

  GIC_PRINT("Distributor Pending Interrupts:\n");
  for(uint_t i = 32; i < __gic.max_num_ints; i++) {
    if(gic_int_pending(i)) {
      GIC_PRINT("\tINTERRUPT %u: PENDING\n", i);
    } else {
 //     GIC_PRINT("\tINTERRUPT %u: NOT PENDING\n", i);
    }
  }

  GIC_PRINT("Distributor Active Interrupts:\n");
  for(uint_t i = 32; i < __gic.max_num_ints; i++) {
    if(gic_int_active(i)) {
      GIC_PRINT("\tINTERRUPT %u: ACTIVE\n", i);
    } else {
//      GIC_PRINT("\tINTERRUPT %u: INACTIVE\n", i);
    }
  }
}
