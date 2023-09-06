
#include<dev/gic.h>
#include<dev/gicv3.h>

#include<nautilus/nautilus.h>
#include<nautilus/endian.h>
#include<nautilus/interrupt.h>
#include<nautilus/iomap.h>

#include<nautilus/of/fdt.h>
#include<nautilus/of/dt.h>

#include<arch/arm64/sys_reg.h>

#include<lib/bitops.h>

#ifndef NAUT_CONFIG_DEBUG_GIC
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

  uint32_t num_redist_regions;
  uint32_t redist_stride;
  uint64_t *redist_bases;
  uint64_t *redist_sizes;
  struct gicr_v3 *redists;
  uint32_t num_redists;

  nk_irq_t irq_base;
  struct nk_irq_desc *sgi_descs;
  struct nk_irq_desc *ppi_descs;
  struct nk_irq_desc *spi_descs;
  nk_irq_t special_irq_base;
  struct nk_irq_desc *special_descs;
  nk_irq_t espi_irq_base;
  struct nk_irq_desc *espi_descs;

  // Optional
  uint32_t interrupt_cells;

  uint64_t cpu_base;
  uint64_t cpu_size;
  uint64_t hyper_base;
  uint64_t hyper_size;
  uint64_t virt_base;
  uint64_t virt_size;
  
  uint32_t num_spi;
  uint32_t num_espi;
  uint32_t num_lpi;

  uint_t security;
  uint_t spi_are_msi;
};

struct gicr_v3
{
  uint32_t affinity;
  uint64_t base;

  struct nk_irq_dev *dev;

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
  return 0;
}
static int gicr_v3_dev_enable_irq(void *state, nk_irq_t irq)
{
  struct gicr_v3 *gicr = (struct gicr_v3 *)state;
  GICR_BITMAP_SET(gicr, GICR_ISENABLER_0_OFFSET, irq);
  return 0;
}

/*
 * disable_irq
 */
static int gicd_v3_dev_disable_irq(void *state, nk_irq_t irq)
{
  struct gicd_v3 *gicd = (struct gicd_v3 *)state;
  GICD_BITMAP_SET(gicd, GICD_ICENABLER_0_OFFSET, irq);
  return 0;
}
static int gicr_v3_dev_disable_irq(void *state, nk_irq_t irq)
{
  struct gicr_v3 *gicr = (struct gicr_v3 *)state;
  GICR_BITMAP_SET(gicr, GICR_ICENABLER_0_OFFSET, irq);
  return 0;
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

static int gicd_v3_dev_revmap(void *state, nk_hwirq_t hwirq, nk_irq_t *irq) 
{
  struct gicd_v3 *gicd = (struct gicd_v3*)state;

  if(hwirq < 0) {
    return -1;
  }
  else if(hwirq <= gicd->num_spi + 31) {
    *irq = gicd->irq_base + hwirq;
    return 0;
  } else if(hwirq >= 1020 && hwirq < 1024) {
    *irq = gicd->special_irq_base + (hwirq - 1020);
    return 0;
  } else if(hwirq >= 4096 && hwirq < (4096 + gicd->num_espi)) {
    *irq = gicd->espi_irq_base + (hwirq - 4096);
  }

  return -1;
}
static int gicr_v3_dev_revmap(void *state, nk_hwirq_t hwirq, nk_irq_t *irq) 
{
  struct gicr_v3 *gicr = (struct gicr_v3*)state;
  return gicd_v3_dev_revmap((void*)gicr->gicd, hwirq, irq);
}

static int gicv3_translate_of_irq(struct gicd_v3 *gicd, nk_irq_t *irq, uint32_t *raw) 
{
  if(gicd->interrupt_cells < 3) {
    return -1;
  }

  uint32_t is_ppi = be32toh(raw[0]);
  uint32_t offset = be32toh(raw[1]);
  uint32_t flags = be32toh(raw[2]);

  if(is_ppi) {
    *irq = (nk_irq_t)(16+offset);
  } else { // is_spi
    *irq = (nk_irq_t)(32+offset);
  }

  return 0;
}

static int gicd_v3_dev_translate_irqs(void *state, nk_dev_info_type_t type, void *raw, int raw_length, nk_irq_t *buf, int *cnt) 
{
  struct gicd_v3 *gicd = (struct gicd_v3*)state;

  switch(type) {
    case NK_DEV_INFO_OF:
      GIC_DEBUG("Translating Device Tree IRQ\n");
      if(gicd->interrupt_cells < 3) {
        GIC_ERROR("GICv3 has invalid interrupt-cells property! value=%u\n", gicd->interrupt_cells);
        return -1;
      }
      int num_cells = raw_length / 4;
      if(num_cells % gicd->interrupt_cells != 0) {
        GIC_ERROR("Device Tree IRQ has incorrect number of cells! GICv3 interrupt-cells = %u, num_cells = %u\n", gicd->interrupt_cells, num_cells);
        return -1;
      }
      int num_irqs = num_cells / gicd->interrupt_cells;
      *cnt = *cnt < num_irqs ? *cnt : num_irqs; // minimum

      for(int i = 0; i<*cnt; i++) {
        if(gicv3_translate_of_irq(gicd, buf+i, raw+(i * gicd->interrupt_cells * 4))) {
          return -1;
        }
      }
      break;
    default:
      GIC_ERROR("Translating IRQ's from ACPI is not currently supported!\n");
      return -1;
  }

  return 0;
}
static int gicr_v3_dev_translate_irqs(void *state, nk_dev_info_type_t type, void *raw, int raw_length, nk_irq_t *buf, int *cnt) 
{
  GIC_WARN("Using GICR device to translate interrupts (shouldn't be happening but recoverable)\n");
  struct gicr_v3 *gicr = (struct gicr_v3*)state;
  return gicd_v3_dev_translate_irqs((void*)gicr->gicd, type, raw, raw_length, buf, cnt);
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
  .irq_status = gicd_v3_dev_irq_status,
  .revmap = gicd_v3_dev_revmap,
  .translate_irqs = gicd_v3_dev_translate_irqs
};

static struct nk_irq_dev_int gicr_v3_dev_int = {
  .get_characteristics = gicr_v3_dev_get_characteristics,
  .initialize_cpu = gicr_v3_dev_initialize_cpu,
  .ack_irq = gicr_v3_dev_ack_irq,
  .eoi_irq = gicr_v3_dev_eoi_irq,
  .enable_irq = gicr_v3_dev_enable_irq,
  .disable_irq = gicr_v3_dev_disable_irq,
  .irq_status = gicr_v3_dev_irq_status,
  .revmap = gicr_v3_dev_revmap,
  .translate_irqs = gicr_v3_dev_translate_irqs
};

static int gicv3_init_gicr_dev(struct gicd_v3 *gicd, struct gicr_v3 *gicr, int i)
{
  int registered = 0;

  gicr->gicd = gicd;

  union gicr_typer typer;
  typer.raw = GICR_LOAD_REG64(gicr, GICR_TYPER_OFFSET);

  gicr->affinity = typer.affinity;

  char gicr_dev_name_buf[DEV_NAME_LEN];
  snprintf(gicr_dev_name_buf,DEV_NAME_LEN,"gicv3-redist%u", i);

  struct nk_irq_dev *gicr_dev = nk_irq_dev_register(gicr_dev_name_buf, 0, &gicr_v3_dev_int, (void*)gicr);
  if(gicr_dev == NULL) {
    GIC_ERROR("Failed to create GICR IRQ device!\n");
    goto gicr_err_exit;
  }
  registered = 1;

  gicr->dev = gicr_dev;

  GIC_DEBUG("%s affinity = %u\n", gicr_dev_name_buf, gicr->affinity);
  GIC_DEBUG("LPI's shared: %s\n", 
      typer.common_lpi_aff == LPI_FULLY_SHARED ? "globally" :
      typer.common_lpi_aff == LPI_SHARED_AFF3 ? "aff3" :
      typer.common_lpi_aff == LPI_SHARED_AFF2 ? "aff2" : "aff1");

  mpidr_el1_t mpid;
  mpid.raw = 0;
  mpid.aff0 = typer.affinity & 0xFF;
  mpid.aff1 = (typer.affinity>>8) & 0xFF;
  mpid.aff2 = (typer.affinity>>16) & 0xFF;
  mpid.aff3 = (typer.affinity>>24) & 0xFF;

  if(nk_assign_cpu_irq_dev(gicr_dev, mpidr_el1_to_cpu_id(&mpid))) {
    GIC_ERROR("Failed to assign GICR to CPU!\n");
    goto gicr_err_exit;
  }

  return 0;

gicr_err_exit:
  if(registered) {
    nk_irq_dev_unregister(gicr_dev);
  }
  return -1;
}

static int gicv3_init_dev_info(struct nk_dev_info *info) 
{
  int allocated_gicd = 0;
  int allocated_redist_sizes = 0;
  int allocated_redist_bases = 0;
  int allocated_redists = 0;
  int registered_gicd = 0;
  int num_redists_registered = 0;

  struct gicd_v3 *gicd = malloc(sizeof(struct gicd_v3));
  if(gicd == NULL) {
    goto err_exit;
  }
  allocated_gicd = 1;

  if(nk_dev_info_read_u32(info, "#interrupt-cells", &gicd->interrupt_cells)) {
    gicd->interrupt_cells = 3;
  }

  if(nk_dev_info_read_u32(info, "#redistributor-regions", &gicd->num_redist_regions))
  {
    gicd->num_redist_regions = 1;
  }

  GIC_DEBUG("num_redist_regions = 0x%x\n", gicd->num_redist_regions);

  gicd->redist_bases = malloc(sizeof(typeof(gicd->redist_bases)) * gicd->num_redist_regions); 
  if(gicd->redist_bases == NULL) {
    goto err_exit;
  }
  allocated_redist_bases = 1;

  gicd->redist_sizes = malloc(sizeof(typeof(gicd->redist_sizes)) * gicd->num_redist_regions); 
  if(gicd->redist_sizes == NULL) {
    goto err_exit;
  }
  allocated_redist_sizes = 1;

  if(nk_dev_info_read_u32(info, "redistributor-stride", &gicd->redist_stride)) 
  {
    gicd->redist_stride = GICR_DEFAULT_STRIDE;
  }

  GIC_DEBUG("redist_stride = 0x%x\n", gicd->redist_stride);

  {
    int num_register_blocks = gicd->num_redist_regions + 4;
    void *reg_bases[num_register_blocks];
    int reg_sizes[num_register_blocks];

    if(nk_dev_info_read_register_blocks(info, reg_bases, reg_sizes, &num_register_blocks)) {
      goto err_exit;
    }

    if(num_register_blocks < 1+gicd->num_redist_regions) {
      goto err_exit;
    }

    // Required register blocks
    gicd->dist_size = (uint64_t)reg_sizes[0];
    gicd->dist_base = (uint64_t)nk_io_map(reg_bases[0], gicd->dist_size, 0);
    if(gicd->dist_base == NULL) {
      GIC_ERROR("Failed to map GICD register region!\n");
      goto err_exit;
    }
    GIC_PRINT("GICD_BASE = %p, GICD_SIZE = 0x%x\n", gicd->dist_base, gicd->dist_size);

    for(int i = 1; i < 1+gicd->num_redist_regions; i++) {
      gicd->redist_sizes[i-1] = (uint64_t)reg_sizes[i];
      gicd->redist_bases[i-1] = (uint64_t)nk_io_map(reg_bases[i], reg_sizes[i], 0);
      if(gicd->redist_bases[i-1] == NULL) {
        GIC_ERROR("Failed to map GICR%u register region!\n", i-1);
        goto err_exit;
      }
      GIC_PRINT("GICR_BASE_%u = %p, GICR_SIZE_%u = 0x%x\n", i-1, gicd->redist_bases[i-1], i-1, gicd->redist_sizes[i-1]);
    }

    // Check for the optional register blocks (But don't map them)
    if(num_register_blocks >= 2+gicd->num_redist_regions) {
      gicd->cpu_base = (uint64_t)reg_bases[num_register_blocks-3];
      gicd->cpu_size = (uint64_t)reg_sizes[num_register_blocks-3];
      GIC_PRINT("GICC_BASE = %p, GICC_SIZE = 0x%x\n", gicd->cpu_base, gicd->cpu_size);
    }
    if(num_register_blocks >= 3+gicd->num_redist_regions) {
      gicd->hyper_base = (uint64_t)reg_bases[num_register_blocks-2];
      gicd->hyper_size = (uint64_t)reg_sizes[num_register_blocks-2];
      GIC_PRINT("GICH_BASE = %p, GICH_SIZE = 0x%x\n", gicd->hyper_base, gicd->hyper_size);
    } 
    if(num_register_blocks >= 4+gicd->num_redist_regions) {
      gicd->virt_base = (uint64_t)reg_bases[num_register_blocks-1];
      gicd->virt_size = (uint64_t)reg_sizes[num_register_blocks-1];
      GIC_PRINT("GICV_BASE = %p, GICV_SIZE = 0x%x\n", gicd->virt_base, gicd->virt_size);
    }
  }

  // Register the GICD
  struct nk_irq_dev *gicd_dev = nk_irq_dev_register("gicv3-dist", 0, &gicd_v3_dev_int, (void*)gicd);

  if(gicd_dev == NULL) {
    goto err_exit;
  }
  registered_gicd = 1;

  if(nk_dev_info_set_device(info, (struct nk_dev*)gicd_dev)) {
    GIC_ERROR("Failed to assign GICD to it's nk_dev_info structure!\n");
    goto err_exit;
  }
  
  // This should be fully overriden by the GICR devices
  // However we're going to be paranoid and assign these so warnings are printed correctly
  if(nk_assign_all_cpus_irq_dev(gicd_dev)) {
    GIC_ERROR("Failed to assign the GIC to all CPU's in the system!\n");
    goto err_exit;
  } else {
    GIC_DEBUG("Assigned the GIC to all CPU's in the system\n");
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
      GIC_DEBUG("Found Redistributor %u in Region %u with GICR_TYPER = 0x%016x, stride = %u\n", gicd->num_redists + i, region, r_typer.raw, gicd->redist_stride);

      i++;
    } while(!r_typer.last_in_region);

    gicd->num_redists += i;
  }
 
  // Allocate room for the redistributors
  gicd->redists = malloc(sizeof(struct gicr_v3) * gicd->num_redists);
  if(gicd->redists == NULL) {
    goto err_exit;
  }
  allocated_redists = 1;

  memset(gicd->redists, 0, sizeof(struct gicr_v3) * gicd->num_redists);

  // Initialize the redistributors
  int gicr_num = 0;
  for(int region = 0; region < gicd->num_redist_regions; region++) {
    int i = 0;

    do {
      gicd->redists[gicr_num].base = gicd->redist_bases[region] + (i * gicd->redist_stride);
      r_typer.raw = GICR_LOAD_REG64(gicd->redists + gicr_num, GICR_TYPER_OFFSET);
      
      if(gicv3_init_gicr_dev(gicd, gicd->redists + gicr_num, gicr_num)) {
        goto err_exit;
      } else {
        num_redists_registered += 1;
      }

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

  // Setting up irq descriptors

#define SGI_HWIRQ      0
#define SGI_NUM        16
#define PPI_HWIRQ      16
#define PPI_NUM        16
#define SPI_HWIRQ      32
#define SPECIAL_HWIRQ  1020
#define SPECIAL_NUM    4
#define ESPI_HWIRQ     4096

  if(nk_request_irq_range(SGI_NUM + PPI_NUM + gicd->num_spi + SPECIAL_NUM + gicd->num_espi, &gicd->irq_base)) {
    GIC_ERROR("Failed to get IRQ numbers for interrupts!\n");
    goto err_exit;
  } 

  nk_irq_t curr_base = gicd->irq_base;

  // Set up 0-15 for SGI
  gicd->sgi_descs = nk_alloc_irq_descs(SGI_NUM, SGI_HWIRQ, NK_IRQ_DESC_FLAG_PERCPU | NK_IRQ_DESC_FLAG_IPI, gicd_dev);
  if(gicd->sgi_descs == NULL) {
    GIC_ERROR("Failed to allocate IRQ descriptors for SGI interrupts!\n");
    goto err_exit;
  }
  if(nk_assign_irq_descs(SGI_NUM, curr_base, gicd->sgi_descs)) {
    GIC_ERROR("Failed to assign IRQ descriptors for SGI interrupts!\n");
    goto err_exit;
  } 
  curr_base += SGI_NUM;
  GIC_DEBUG("Set Up SGI's\n");
  
  // Set up 16-31 for PPI
  gicd->ppi_descs = nk_alloc_irq_descs(PPI_NUM, PPI_HWIRQ, NK_IRQ_DESC_FLAG_PERCPU, gicd_dev);
  if(gicd->ppi_descs == NULL) {
    GIC_ERROR("Failed to allocate IRQ descriptors for PPI interrupts!\n");
    goto err_exit;
  }
  if(nk_assign_irq_descs(PPI_NUM, curr_base, gicd->ppi_descs)) {
    GIC_ERROR("Failed to assign IRQ descriptors for PPI interrupts!\n");
    goto err_exit;
  }
  curr_base += PPI_NUM;
  GIC_DEBUG("Set Up PPI Vectors\n");

  // Set up the correct number of SPI
  if(gicd->num_spi) 
  {
    gicd->spi_descs = nk_alloc_irq_descs(gicd->num_spi, SPI_HWIRQ, 0, gicd_dev);
    if(gicd->spi_descs == NULL) {
      GIC_ERROR("Failed to allocate IRQ descriptors for SPI interrupts!\n");
      goto err_exit;
    }
    if(nk_assign_irq_descs(gicd->num_spi, gicd->irq_base + SPI_HWIRQ, gicd->spi_descs)) 
    {
      GIC_ERROR("Failed to assign IRQ descriptors for SPI interrupts!\n");
      goto err_exit;
    }
    curr_base += gicd->num_spi;
    GIC_DEBUG("Set Up SPI Vectors\n");
  } else {
    GIC_DEBUG("No SPI in the standard range\n");
  }

  // Set up spurrious interrupt descriptors
  gicd->special_irq_base = curr_base;
  gicd->special_descs = nk_alloc_irq_descs(SPECIAL_NUM, SPECIAL_HWIRQ, 0, gicd_dev);
  if(gicd->special_descs == NULL) {
    GIC_ERROR("Failed to allocate IRQ descriptors for Special INTID's!\n");
    goto err_exit;
  }
  if(nk_assign_irq_descs(SPECIAL_NUM, curr_base, gicd->special_descs)) {
    GIC_ERROR("Failed to assign IRQ descriptors for Special INTID's!\n");
    goto err_exit;
  }
  curr_base += SPECIAL_NUM;
  GIC_DEBUG("Added Special INTID's\n");

  // Extended SPI
  gicd->espi_irq_base = curr_base;
  if(gicd->num_espi) {
    gicd->espi_descs = nk_alloc_irq_descs(gicd->num_espi, ESPI_HWIRQ, 0, gicd_dev);
    if(gicd->espi_descs == NULL) {
      GIC_ERROR("Failed to allocate IRQ descriptors for Extended SPI interrupts!\n");
      goto err_exit;
    }
    if(nk_assign_irq_descs(gicd->num_espi, curr_base, gicd->espi_descs)) 
    {
      GIC_ERROR("Failed to assign IRQ descriptors for Extended SPI interrupts!\n");
      goto err_exit;
    }
    curr_base += gicd->num_espi;
    GIC_DEBUG("Set Up SPI Vectors\n");
  } else {
    GIC_DEBUG("Extended SPI are not supported\n");
  }

#ifdef NAUT_CONFIG_GIC_VERSION_3_ITS
  if(gicv3_its_init(dtb, state)) {
    GIC_WARN("Failed to initialize MSI Support!\n");
  }
#endif

  return 0;

err_exit:

  for(int i = num_redists_registered; i>0; i--) {
    nk_irq_dev_unregister(gicd->redists[i].dev);
  }
  if(allocated_redists) {
    free(gicd->redists);
  }
  if(allocated_redist_bases) {
    free(gicd->redist_bases);
  }
  if(allocated_redist_sizes) {
    free(gicd->redist_sizes);
  }
  if(allocated_gicd) {
    free(gicd);
  }

  return -1;
}

static const char *gicv3_of_properties_names[] = {
  "interrupt-controller"
};

static const struct of_dev_properties gicv3_of_properties = {
  .names = gicv3_of_properties_names,
  .count = sizeof(gicv3_of_properties_names)/sizeof(char*)
};

static const struct of_dev_id gicv3_of_dev_ids[] = {
  { .properties = &gicv3_of_properties, .compatible = "arm,gic-v3" },
  { .properties = &gicv3_of_properties, .compatible = "qcom,msm8996-gic-v3" }
};

static const struct of_dev_match gicv3_of_dev_match = {
  .ids = gicv3_of_dev_ids,
  .num_ids = sizeof(gicv3_of_dev_ids)/sizeof(struct of_dev_id),
  .max_num_matches = 1
};

int gicv3_init(void) 
{  
  return of_for_each_match(&gicv3_of_dev_match, gicv3_init_dev_info);
}

