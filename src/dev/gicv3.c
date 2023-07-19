
#include<dev/gic.h>
#include<dev/gicv3.h>

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

#define ASSERT_SIZE(type, size) _Static_assert(sizeof(type) == size, \
    "sizeof("#type") is not "#size" bytes!\n") 

/*
 * GICD Register Definitions
 */

union gicd_ctlr {
  uint32_t raw;
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
  };
};
ASSERT_SIZE(union gicd_ctlr, 4);

union gicd_ctlr_insecure {
  uint32_t raw;
  struct {
    uint_t grp1_en : 1;
    uint_t grp1_aff_en : 1;
    uint_t __res0_0 : 2;
    uint_t aff_routing_en : 1;
    uint_t __res0_1 : 25;
    uint_t write_pending : 1;
  }; 
};
ASSERT_SIZE(union gicd_ctlr_insecure, 4);

union gicd_typer {
  uint32_t raw;
  struct {
    uint_t it_lines_num : 5;
    uint_t cpu_num : 3;
    uint_t espi_sup : 1;
    uint_t nmi_sup : 1;
    uint_t sec_ext_impl : 1;
    uint_t num_lpi : 5;
    uint_t spi_msi_sup : 1;
    uint_t lpi_sup : 1;
    uint_t direct_virt_inj_lpi_sup : 1;
    uint_t intid_bits : 5;
    uint_t aff3_valid : 1;
    uint_t no1n_sup : 1;
    uint_t range_sel_sup : 1;
    uint_t espi_range : 5;
  };
};
ASSERT_SIZE(union gicd_typer, 4);

/*
 * ICC Register Definitions
 */

union icc_ctlr {
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
};
ASSERT_SIZE(union icc_ctlr, 8);

/*
 * GICR Register Definitions
 */

union gicr_ctlr {
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
};
ASSERT_SIZE(union gicr_ctlr, 8);

union gicr_baser {
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
};
ASSERT_SIZE(union gicr_baser, 8);

#define LPI_FULLY_SHARED 0b00
#define LPI_SHARED_AFF3  0b01
#define LPI_SHARED_AFF2  0b10
#define LPI_SHARED_AFF1  0b11

#define PPI_MAX_ID_31    0b00000
#define PPI_MAX_ID_1087  0b00001
#define PPI_MAX_ID_1119  0b00010

union gicr_typer {
  uint64_t raw;
  struct {
    uint_t phys_lpi_sup : 1;
    uint_t virt_lpi_sup : 1;
    uint_t dirty : 1;
    uint_t direct_lpi_sup : 1;
    uint_t last_in_region : 1;
    uint_t dpg_sup : 1;
    uint_t mpam_sup : 1;
    uint_t rvpeid : 1;
    uint_t proc_number : 16;
    uint_t common_lpi_aff : 2;
    uint_t virt_sgi_sup : 1;
    uint_t max_ppi_id : 5;
    uint_t affinity : 32;
  };
};
ASSERT_SIZE(union gicr_typer, 8);

struct gicd_v3 
{
  uint64_t dist_base;
  uint64_t dist_size;

  uint_t num_redist_regions;
  uint64_t redist_stride;
  uint64_t *redist_bases;
  uint64_t *redist_sizes;
  struct gicr_v3 *redists;

  uint32_t num_redists;

  // Optional
  uint64_t cpu_base;
  uint64_t cpu_size;
  uint64_t hyper_base;
  uint64_t hyper_size;
  uint64_t virt_base;
  uint64_t virt_size;
  
  uint16_t num_spi;
  uint16_t num_espi;
  uint32_t num_lpi;

  uint_t security;
  uint_t spi_are_msi;
};

struct gicr_v3
{
  uint32_t affinity;
  uint64_t base;

  struct gicd_v3 *gicd;
};

static inline void gicd_wait_for_write(struct gicd_v3 *gicd) 
{
  union gicd_ctlr ctlr;
  do {
    ctlr.raw = GICD_LOAD_REG(gicd, GICD_CTLR_OFFSET);
  } while(ctlr.write_pending);
}

static inline void gicr_cpu_wait_for_write(struct gicr_v3 *gicr) 
{
  union gicr_ctlr ctlr;
  do {
    ctlr.raw = GICR_LOAD_REG64(gicr, GICR_CTLR_OFFSET);
  } while(ctlr.upstream_write_pending || ctlr.write_pending);
}

/*
 * get_characteristics
 */
static int gicd_v3_dev_get_characteristics(void *state, struct nk_irq_dev_characteristics *c)
{
  memset(c, 0, sizeof(c));
  return 0;
}
static int gicr_v3_dev_get_characteristics(void *state, struct nk_irq_dev_characteristics *c)
{
  memset(c, 0, sizeof(c));
  return 0;
}

/*
 * initialize_cpu
 */
static int gicv3_dev_initialize_cpu_common(struct gicd_v3 *gicd) {

  uint64_t icc_sre;
  LOAD_SYS_REG(ICC_SRE_EL1, icc_sre);
  icc_sre |= 1; // Enable ICC Register Access
  STORE_SYS_REG(ICC_SRE_EL1, icc_sre);

  asm volatile ("isb");

  union icc_ctlr ctl;
  LOAD_SYS_REG(ICC_CTLR_EL1, ctl.raw);
  ctl.multistep_eoi = 0; // EOI Priority Drop = Deactivation
  STORE_SYS_REG(ICC_CTLR_EL1, ctl.raw);

  uint64_t icc_pmr = 0xFF;
  STORE_SYS_REG(ICC_PMR_EL1, icc_pmr);

  STORE_SYS_REG(ICC_IGRPEN0_EL1, 1);
  STORE_SYS_REG(ICC_IGRPEN1_EL1, 1);

  GIC_DEBUG("GICv3 initialized for CPU %u\n", my_cpu_id());

  return 0;
}
static int gicd_v3_dev_initialize_cpu(void *state) 
{
  struct gicd_v3 *gicd = (struct gicd_v3 *)state;
  GIC_WARN("Initializing CPU %u using the GICD: Redistributors were not fully assigned to CPU's!\n", my_cpu_id());
  return gicv3_dev_initialize_cpu_common(gicd);
}
static int gicr_v3_dev_initialize_cpu(void *state) 
{
  struct gicr_v3 *gicr = (struct gicr_v3 *)state;
  return gicv3_dev_initialize_cpu_common(gicr->gicd); 
}

/*
 * ack_irq
 */
static int gicv3_dev_ack_irq_common(struct gicd_v3 *gicd, nk_irq_t *irq) 
{
  uint32_t int_id;
  if(gicd->security) {
    LOAD_SYS_REG(ICC_IAR1_EL1, int_id);
  } else {
    // Single Security State Systems default IGROUPRn to all 0's
    LOAD_SYS_REG(ICC_IAR0_EL1, int_id);
  }

  *irq = int_id;

  return IRQ_DEV_ACK_SUCCESS;
}
static int gicd_v3_dev_ack_irq(void *state, nk_irq_t *irq) 
{
  struct gicd_v3 *gicd = (struct gicd_v3 *)state;
  return gicv3_dev_ack_irq_common(gicd, irq);
}
static int gicr_v3_dev_ack_irq(void *state, nk_irq_t *irq)
{
  struct gicr_v3 *gicr = (struct gicr_v3 *)state;
  return gicv3_dev_ack_irq_common(gicr->gicd, irq);
}

/*
 * eoi_irq
 */
static int gicv3_dev_eoi_irq_common(struct gicd_v3 *gicd, nk_irq_t irq) 
{
  if(gicd->security) {
    STORE_SYS_REG(ICC_EOIR1_EL1, irq);
  } else {
    // Single Security State Systems default IGROUPRn to all 0's
    STORE_SYS_REG(ICC_EOIR0_EL1, irq);
  }

  return IRQ_DEV_EOI_SUCCESS;
}
static int gicd_v3_dev_eoi_irq(void *state, nk_irq_t irq) 
{
  struct gicd_v3 *gicd = (struct gicd_v3 *)state;
  return gicv3_dev_eoi_irq_common(gicd, irq);
}
static int gicr_v3_dev_eoi_irq(void *state, nk_irq_t irq)
{
  struct gicr_v3 *gicr = (struct gicr_v3 *)state;
  return gicv3_dev_eoi_irq_common(gicr->gicd, irq);
}

/*
 * enable_irq
 */
static int gicd_v3_dev_enable_irq(void *state, nk_irq_t irq)
{
  struct gicd_v3 *gicd = (struct gicd_v3 *)state;
  GICD_BITMAP_SET(gicd, GICD_ISENABLER_0_OFFSET, irq);
}
static int gicr_v3_dev_enable_irq(void *state, nk_irq_t irq)
{
  struct gicr_v3 *gicr = (struct gicr_v3 *)state;
  GICR_BITMAP_SET(gicr, GICR_ISENABLER_0_OFFSET, irq);
}

/*
 * disable_irq
 */
static int gicd_v3_dev_disable_irq(void *state, nk_irq_t irq)
{
  struct gicd_v3 *gicd = (struct gicd_v3 *)state;
  GICD_BITMAP_SET(gicd, GICD_ICENABLER_0_OFFSET, irq);
}
static int gicr_v3_dev_disable_irq(void *state, nk_irq_t irq)
{
  struct gicr_v3 *gicr = (struct gicr_v3 *)state;
  GICR_BITMAP_SET(gicr, GICR_ICENABLER_0_OFFSET, irq);
}

/*
 * irq_status
 */
static int gicd_v3_dev_irq_status(void *state, nk_irq_t irq) 
{
  struct gicd_v3 *gicd = (struct gicd_v3*)state;

  int status = 0;

  status |= GICD_BITMAP_READ(gicd, GICD_ISENABLER_0_OFFSET, irq) ?
    IRQ_DEV_STATUS_ENABLED : 0;

  status |= GICD_BITMAP_READ(gicd, GICD_ISPENDR_0_OFFSET, irq) ?
    IRQ_DEV_STATUS_PENDING : 0;

  status |= GICD_BITMAP_READ(gicd, GICD_ISACTIVER_0_OFFSET, irq) ?
    IRQ_DEV_STATUS_ACTIVE : 0;

  return status;
}
static int gicr_v3_dev_irq_status(void *state, nk_irq_t irq) 
{
  struct gicr_v3 *gicr = (struct gicr_v3*)state;

  int status = 0;

  status |= GICR_BITMAP_READ(gicr, GICR_ISENABLER_0_OFFSET, irq) ?
    IRQ_DEV_STATUS_ENABLED : 0;

  status |= GICR_BITMAP_READ(gicr, GICR_ISPENDR_0_OFFSET, irq) ?
    IRQ_DEV_STATUS_PENDING : 0;

  status |= GICR_BITMAP_READ(gicr, GICR_ISACTIVER_0_OFFSET, irq) ?
    IRQ_DEV_STATUS_ACTIVE : 0;

  return status;
}

/*
 * Initialization
 */

static struct nk_irq_dev_int gicd_v3_dev_int = {
  .get_characteristics = gicd_v3_dev_get_characteristics,
  .initialize_cpu = gicd_v3_dev_initialize_cpu,
  .ack_irq = gicd_v3_dev_ack_irq,
  .eoi_irq = gicd_v3_dev_eoi_irq,
  .enable_irq = gicd_v3_dev_enable_irq,
  .disable_irq = gicd_v3_dev_disable_irq,
  .irq_status = gicd_v3_dev_irq_status
};

static struct nk_irq_dev_int gicr_v3_dev_int = {
  .get_characteristics = gicr_v3_dev_get_characteristics,
  .initialize_cpu = gicr_v3_dev_initialize_cpu,
  .ack_irq = gicr_v3_dev_ack_irq,
  .eoi_irq = gicr_v3_dev_eoi_irq,
  .enable_irq = gicr_v3_dev_enable_irq,
  .disable_irq = gicr_v3_dev_disable_irq,
  .irq_status = gicr_v3_dev_irq_status
};

static const char *gicv3_compat_list[] = {
  "arm,gic-v3",
  "qcom,msm8996-gic-v3"
};

static int gicv3_dtb_init(struct gicd_v3 *gicd, uint64_t dtb) {

  int offset = fdt_node_offset_by_prop_value((void*)dtb, -1, "interrupt-controller", NULL, NULL);

  while(offset >= 0) { 
    int compat_result = 1;
    for(uint_t i = 0; i < sizeof(gicv3_compat_list)/sizeof(const char*); i++) {
      compat_result &= fdt_node_check_compatible((void*)dtb, offset, gicv3_compat_list[i]);
      if(!compat_result) {
        GIC_DEBUG("Found compatible GICv3 controller in device tree: %s\n", gicv3_compat_list[i]);
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
        gicd->num_redist_regions = be32toh(*(uint32_t *)gicr_regions_p);
    } else if(lenp == 8) {
        gicd->num_redist_regions = be64toh(*(uint32_t *)gicr_regions_p);
    }
  } else {
    gicd->num_redist_regions = 1;
  }

  gicd->redist_bases = malloc(sizeof(typeof(gicd->redist_bases)) * gicd->num_redist_regions);
  gicd->redist_sizes = malloc(sizeof(typeof(gicd->redist_sizes)) * gicd->num_redist_regions);

  void *gicr_stride_p = fdt_getprop(dtb, offset, "#redistributor-stride", &lenp);
  if(gicr_stride_p != NULL) {
    if(lenp == 4) {
        gicd->redist_stride = be32toh(*(uint32_t *)gicr_stride_p);
    } else if(lenp == 8) {
        gicd->redist_stride = be64toh(*(uint64_t *)gicr_stride_p);
    }
  } else {
    gicd->redist_stride = GICR_DEFAULT_STRIDE;
  }

  int num_read = 4 + gicd->num_redist_regions;
  fdt_reg_t reg[num_read];

  int getreg_result = fdt_getreg_array((void*)dtb, offset, reg, &num_read);

  if(!getreg_result && num_read >= 1+gicd->num_redist_regions) {

      // Required MMIO Regions
      for(uint_t i = 0; i < gicd->num_redist_regions; i++) {
        gicd->redist_bases[i] = reg[1+i].address;
        gicd->redist_sizes[i] = reg[1+i].size;
        GIC_PRINT("GICR_BASE_%u = %p, GICR_SIZE_%u = 0x%x\n", i, gicd->redist_bases[i], i, gicd->redist_sizes[i]);
      }
      gicd->dist_base = reg[0].address;
      gicd->dist_size = reg[0].size;
      GIC_PRINT("GICD_BASE = %p, GICD_SIZE = 0x%x\n", gicd->dist_base, gicd->dist_size);

      // Optional extra MMIO regions
      if(num_read >= 2+gicd->num_redist_regions) {
        gicd->cpu_base = reg[num_read-3].address;
        gicd->cpu_size = reg[num_read-3].size;
        GIC_PRINT("GICC_BASE = %p, GICC_SIZE = 0x%x\n", gicd->cpu_base, gicd->cpu_size);
      }
      if(num_read >= 3+gicd->num_redist_regions) {
        gicd->hyper_base = reg[num_read-2].address;
        gicd->hyper_size = reg[num_read-2].size;
        GIC_PRINT("GICH_BASE = %p, GICH_SIZE = 0x%x\n", gicd->hyper_base, gicd->hyper_size);
      } 
      if(num_read >= 4+gicd->num_redist_regions) {
        gicd->virt_base = reg[num_read-1].address;
        gicd->virt_size = reg[num_read-1].size;
        GIC_PRINT("GICV_BASE = %p, GICV_SIZE = 0x%x\n", gicd->virt_base, gicd->virt_size);
      }
      if(num_read > 4+gicd->num_redist_regions) {
        GIC_WARN("Read more REG entries in device tree than expected! num_read = %d\n", num_read);
      }
  } else {
    GIC_ERROR("GICv3 found in device tree has invalid or missing REG property! num_read = %d, gicd->num_redist_regions = %u\n", num_read, gicd->num_redist_regions);
    return -1;
  }

  return 0;
}

static int gicv3_init_gicr_dev(struct gicd_v3 *gicd, struct gicr_v3 *gicr, int i)
{
  gicr->gicd = gicd;

  union gicr_typer typer;
  typer.raw = GICR_LOAD_REG64(gicr, GICR_TYPER_OFFSET);

  gicr->affinity = typer.affinity;

  char gicr_dev_name_buf[DEV_NAME_LEN];
  snprintf(gicr_dev_name_buf,DEV_NAME_LEN,"gicv3-redist%u", i);

  struct nk_irq_dev *gicr_dev = nk_irq_dev_register(gicr_dev_name_buf, 0, &gicr_v3_dev_int, (void*)gicr);

  GIC_DEBUG("%s affinity = %u\n", gicr_dev_name_buf, gicr->affinity);
  GIC_DEBUG("LPI's shared: %s\n", 
      typer.common_lpi_aff == LPI_FULLY_SHARED ? "globally" :
      typer.common_lpi_aff == LPI_SHARED_AFF3 ? "aff3" :
      typer.common_lpi_aff == LPI_SHARED_AFF2 ? "aff2" : "aff1");

  nk_assign_cpu_irq_dev(gicr_dev, gicr->affinity);
}

static int gicv3_dev_init(uint64_t dtb, struct gicd_v3 *gicd) {

  // Offset should now point to the GICv3 entry in the device tree
  if(gicv3_dtb_init(gicd, dtb)) {
    GIC_ERROR("Could not initialize GICv3 using device tree: global initialization failed!\n");
    return -1;
  }

  // Determine the number of redistributors
  struct gicr_v3 poke_gicr;
  union gicr_typer r_typer;

  // Count the redistributors
  for(int region = 0; region < gicd->num_redist_regions; region++) {
    int i = 0;

    do {
      poke_gicr.base = gicd->redist_bases[region] + (i * gicd->redist_stride);
      r_typer.raw = GICR_LOAD_REG64(&poke_gicr, GICR_TYPER_OFFSET);
      GIC_DEBUG("Found Redistributor %u in Region %u with GICR_TYPER = 0x%016x, stide = %u\n", gicd->num_redists + i, region, r_typer.raw, gicd->redist_stride);

      i++;
    } while(!r_typer.last_in_region);

    gicd->num_redists += i;
  }

  gicd->redists = malloc(sizeof(struct gicr_v3) * gicd->num_redists);
  memset(gicd->redists, 0, sizeof(struct gicr_v3) * gicd->num_redists);

  // Initialize the redistributors
  int gicr_num = 0;
  for(int region = 0; region < gicd->num_redist_regions; region++) {
    int i = 0;

    do {
      gicd->redists[gicr_num].base = gicd->redist_bases[region] + (i * gicd->redist_stride);
      r_typer.raw = GICR_LOAD_REG64(gicd->redists + gicr_num, GICR_TYPER_OFFSET);
      gicv3_init_gicr_dev(gicd, gicd->redists + gicr_num, gicr_num);

      i++;
      gicr_num++;
    } while(!r_typer.last_in_region);
  }

  // Read info from GICD_TYPER
  union gicd_typer d_typer;
  d_typer.raw = GICD_LOAD_REG(gicd, GICD_TYPER_OFFSET);
  gicd->num_spi = ((d_typer.it_lines_num + 1) << 6) - 32;
  gicd->security = d_typer.sec_ext_impl;

  if(d_typer.espi_sup) {
    gicd->num_espi = (32*(d_typer.espi_range+1));
  } else {
    gicd->num_espi = 0;
  }

  GIC_PRINT("num_spi = %u\n", gicd->num_spi);
  GIC_PRINT("num_espi = %u\n", gicd->num_espi);
  GIC_PRINT("security = %u\n", gicd->security);

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
  
  if(gicd->security) {

      GIC_DEBUG("Configuring Insecure Distributor for Two Security State System\n");

      union gicd_ctlr_insecure ctl;
      ctl.raw = GICD_LOAD_REG(gicd, GICD_CTLR_OFFSET);

      // Disable All
      ctl.grp1_en = 0;
      ctl.grp1_aff_en = 0;
      GICD_STORE_REG(gicd, GICD_CTLR_OFFSET, ctl.raw);
      gicd_wait_for_write(gicd);

      // Enable Affinity Routing
      ctl.raw = GICD_LOAD_REG(gicd, GICD_CTLR_OFFSET);
      ctl.aff_routing_en = 1;
      GICD_STORE_REG(gicd, GICD_CTLR_OFFSET, ctl.raw);
      gicd_wait_for_write(gicd);

      // Re-enable Group 1 Affinity Routed
      ctl.raw = GICD_LOAD_REG(gicd, GICD_CTLR_OFFSET);
      ctl.grp1_en = 0;
      ctl.grp1_aff_en = 1;
      GICD_STORE_REG(gicd, GICD_CTLR_OFFSET, ctl.raw);
      gicd_wait_for_write(gicd);

  } else {

      GIC_DEBUG("Configuring Distributor for Single Security State System\n");

      union gicd_ctlr ctl;
      ctl.raw = GICD_LOAD_REG(gicd, GICD_CTLR_OFFSET);

      // Disable all groups (required)
      ctl.grp0_en = 0;
      ctl.grp1_en = 0;
      GICD_STORE_REG(gicd, GICD_CTLR_OFFSET, ctl.raw);
      gicd_wait_for_write(gicd);

      // Enable affinity routing
      ctl.raw = GICD_LOAD_REG(gicd, GICD_CTLR_OFFSET);
      ctl.aff_routing_en = 1;
      GICD_STORE_REG(gicd, GICD_CTLR_OFFSET, ctl.raw);
      gicd_wait_for_write(gicd);

      // Re-enable groups (and other config)
      ctl.raw = GICD_LOAD_REG(gicd, GICD_CTLR_OFFSET);
      ctl.grp0_en = 1;
      ctl.grp1_en = 1;
      ctl.one_of_n_wakeup_en = 0;
      ctl.sgis_active_state_en = 1;
      GICD_STORE_REG(gicd, GICD_CTLR_OFFSET, ctl.raw);
      gicd_wait_for_write(gicd);

  }

  GIC_DEBUG("GICD_CTLR Configured\n");

  return 0;
}

static int gicv3_dev_ivec_desc_init(uint64_t dtb, void *state, struct nk_irq_dev *gicd_dev) 
{

  struct gicd_v3 *gicd = (struct gicd_v3 *)state;
  
  nk_alloc_ivec_descs(0, 16, NULL,
      NK_IVEC_DESC_TYPE_DEFAULT,
      NK_IVEC_DESC_FLAG_PERCPU | NK_IVEC_DESC_FLAG_IPI);
  GIC_DEBUG("Set up SGI Vector Range\n");

  nk_alloc_ivec_descs(16, 16, NULL,
      NK_IVEC_DESC_TYPE_DEFAULT,
      NK_IVEC_DESC_FLAG_PERCPU);
  GIC_DEBUG("Set up PPI Vector Range\n");

  if(gicd->num_spi) {
    nk_alloc_ivec_descs(32, gicd->num_spi, gicd_dev,
        NK_IVEC_DESC_TYPE_DEFAULT,
        gicd->spi_are_msi ? NK_IVEC_DESC_FLAG_MSI | NK_IVEC_DESC_FLAG_MSI_X : 0
        );
    GIC_DEBUG("Set up standard SPI Vector Range\n");
  } else {
    GIC_WARN("No SPI were found in the standard range!\n");
  }

  if(gicd->num_espi) {
    nk_alloc_ivec_descs(4096, gicd->num_espi, gicd_dev,
        NK_IVEC_DESC_TYPE_DEFAULT,
        0
        );
    GIC_DEBUG("Set up Extended SPI Vector Range\n");
  } else {
    GIC_DEBUG("Extended SPI are not supported\n");
  }

  // Add the special INTIDS
  nk_alloc_ivec_descs(1020, 4, gicd_dev, NK_IVEC_DESC_TYPE_SPURRIOUS, 0);
  GIC_DEBUG("Set up Special Interrupt Vector Range\n");

  return 0;
}
int gicv3_init(uint64_t dtb) 
{
  struct gicd_v3 *state = malloc(sizeof(struct gicd_v3));
  memset(state, 0, sizeof(struct gicd_v3));

  if(state == NULL) {
    GIC_ERROR("Failed to allocate gicv3 struct!\n");
    return -1;
  }

  struct nk_irq_dev *dev = nk_irq_dev_register("gicv3-dist", 0, &gicd_v3_dev_int, (void*)state);

  // This should be fully overriden by the GICR devices
  // However we're going to be paranoid and assign these so warnings are printed correctly
  if(nk_assign_all_cpus_irq_dev(dev)) {
    GIC_ERROR("Failed to assign the GIC to all CPU's in the system!\n");
    return -1;
  } else {
    GIC_DEBUG("Assigned the GIC to all CPU's in the system\n");
  }

  if(dev == NULL) {
    GIC_ERROR("Failed to register as an IRQ device!\n");
    return -1;
  }

  // Initialize internal state and set up the GICR devices
  if(gicv3_dev_init(dtb, state)) {
    GIC_ERROR("Failed to initialize GIC device!\n");
    return -1;
  }

#ifdef NAUT_CONFIG_GIC_VERSION_3_ITS
  if(gicv3_its_init(dtb, state)) {
    GIC_WARN("Failed to initialize MSI Support!\n");
  }
#endif

  if(gicv3_dev_ivec_desc_init(dev, state, dev)) {
    GIC_ERROR("Failed to set up interrupt vector descriptors!\n");
    return -1;
  }

#ifdef NAUT_CONFIG_GIC_VERSION_3_ITS
  if(gicv3_its_ivec_init(dtb, state)) {
    GIC_WARN("Failed to set up interrupt vector MSI info!\n");
  }
#endif

  return 0;
}

