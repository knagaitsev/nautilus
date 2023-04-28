#ifndef __ARM64_GIC_H__
#define __ARM64_GIC_H__

// Definitions for this file are taken from the Programmer Model section of:
// https://www.cl.cam.ac.uk/research/srg/han/ACS-P35/zynq/arm_gic_architecture_specification.pdf

#include<nautilus/naut_types.h>

#define ASSERT_SIZE(type, size) _Static_assert(sizeof(type) == size, \
    "sizeof("#type") is not "#size" bytes!\n") 

typedef struct gic {
  void *dist_base;
  void *cpu_interface_base;

  uint_t max_ints;
  uint_t max_cpu_interfaces;
  
} gic_t;

#ifdef NAUT_CONFIG_ENABLE_ASSERTS
  #define INVALID_INT_NUM(gic, num) (num >= gic->max_ints)
#else
  #define INVALID_INT_NUM(gic, num) 0
#endif

#define LOAD_GICD_REG(gic, offset) *(volatile uint32_t*)(gic->dist_base + offset)
#define STORE_GICD_REG(gic, offset, val) (*(volatile uint32_t*)(gic->dist_base + offset)) = val

#define LOAD_GICC_REG(gic, offset) *(volatile uint32_t*)(gic->cpu_interface_base + offset)
#define STORE_GICC_REG(gic, offset, val) *(volatile uint32_t*)(gic->cpu_interface_base + offset) = val

#define GICD_BITMAP_SET(gic, num, offset) \
  *((volatile uint32_t*)(gic->dist_base + offset)+(num >> 6)) |= (1<<(num&0x3F))
#define GICD_BITMAP_UNSET(gic, num, offset) \
  *((volatile uint32_t*)(gic->dist_base + offset)+(num >> 6)) &= ~(1<<(num&0x3F))
#define GICD_BITMAP_READ(gic, num, offset) \
  (!!(*((volatile uint32_t*)(gic->dist_base + offset)+(num >> 6)) & (1<<(num&0x3F))))

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

// CPU Interface Register Offsets
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

extern gic_t *__gic_ptr;

int global_init_gic(void *dtb, gic_t *gic);
int per_cpu_init_gic(void);
void print_gic(void);

// Unique (non-bitmap) GICD Registers
typedef union gicd_ctl_reg {
  uint32_t raw;
  struct {
    uint_t enabled : 1;
    uint_t grp0_enabled : 1;
    // Rest reserved
  };
} gicd_ctl_reg_t;
ASSERT_SIZE(gicd_ctl_reg_t, 4);

typedef union gicd_type_reg {
  uint32_t raw;
  struct {
    uint_t it_lines_num : 5;
    uint_t cpu_num : 3;
    uint_t __resv1 : 2;
    uint_t security_ext : 1;
    uint_t lspi : 5;
    // Rest reserved
  };
} gicd_type_reg_t;
ASSERT_SIZE(gicd_type_reg_t, 4);

typedef union gicd_impl_id_reg {
  uint32_t raw;
  struct {
    uint_t implementor : 12;
    uint_t revision : 4;
    uint_t variant : 4;
    uint_t __resv1 : 4;
    uint_t product_id : 8;
  };
} gicd_impl_id_reg_t;
ASSERT_SIZE(gicd_impl_id_reg_t, 4);

typedef union gicd_sgi_reg {
  uint32_t raw;
  struct {
    uint_t sgi_int_id : 4;
    uint_t __resv1 : 11;
    uint_t nsatt : 1;
    uint_t cpu_target_list : 8;
    uint_t target_list_filter : 2;
    // Rest reserved
  };
} gicd_sgi_reg_t;
ASSERT_SIZE(gicd_sgi_reg_t, 4);

// Bitmapped GICD Register Access

// Interrupt Groups
static inline int gicd_set_int_group(gic_t *gic, uint_t int_num, uint_t grp1) {
  if(INVALID_INT_NUM(gic, int_num)) {
    return -1;
  }
  if(grp1) {
    GICD_BITMAP_SET(gic, int_num, GICD_IGROUPR_0_OFFSET);
  } else {
    GICD_BITMAP_UNSET(gic, int_num, GICD_IGROUPR_0_OFFSET);
  }
  return 0;
}

// Enabled/Disabled Interrupts
static inline int gicd_enable_int(gic_t *gic, uint_t int_num) {
  if(INVALID_INT_NUM(gic, int_num)) {
    return -1;
  }
  GICD_BITMAP_SET(gic, int_num, GICD_ISENABLER_0_OFFSET);
  return 0;
}
static inline int gicd_disable_int(gic_t *gic, uint_t int_num) {
  if(INVALID_INT_NUM(gic, int_num)) {
    return -1;
  }
  GICD_BITMAP_SET(gic, int_num, GICD_ICENABLER_0_OFFSET);
  return 0;
}
static inline int gicd_int_enabled(gic_t *gic, uint_t int_num) {
  if(INVALID_INT_NUM(gic, int_num)) {
    return 0;
  } else {
    return GICD_BITMAP_READ(gic, int_num, GICD_ISENABLER_0_OFFSET);
  }
}

// Pending interrupts
static inline int gicd_set_int_pending(gic_t *gic, uint_t int_num) {
  if(INVALID_INT_NUM(gic, int_num)) {
    return -1;
  }
  GICD_BITMAP_SET(gic, int_num, GICD_ISPENDR_0_OFFSET);
  return 0;
}
static inline int gicd_clear_int_pending(gic_t *gic, uint_t int_num) {
  if(INVALID_INT_NUM(gic, int_num)) {
    return -1;
  }
  GICD_BITMAP_SET(gic, int_num, GICD_ICPENDR_0_OFFSET);
}
static inline int gicd_int_pending(gic_t *gic, uint_t int_num) {
  if(INVALID_INT_NUM(gic, int_num)) {
    return 0;
  }
  else {
    return GICD_BITMAP_READ(gic, int_num, GICD_ISPENDR_0_OFFSET);
  }
}

// Active Interrupts
static inline int gicd_set_int_active(gic_t *gic, uint_t int_num) {
  if(INVALID_INT_NUM(gic, int_num)) {
    return -1;
  }
  GICD_BITMAP_SET(gic, int_num, GICD_ISACTIVER_0_OFFSET);
  return 0;
}
static inline int gicd_clear_int_active(gic_t *gic, uint_t int_num) {
  if(INVALID_INT_NUM(gic, int_num)) {
    return -1;
  }
  GICD_BITMAP_SET(gic, int_num, GICD_ICACTIVER_0_OFFSET);
  return 0;
}
static inline int gicd_int_active(gic_t *gic, uint_t int_num) {
  if(INVALID_INT_NUM(gic, int_num)) {
    return 0;
  }
  else {
    return GICD_BITMAP_READ(gic, int_num, GICD_ISACTIVER_0_OFFSET);
  }
}

/*
 * There are more to implement like "active" saving and restoring, 
 */

// Unique (non-bitmap) GICC Registers

typedef union gicc_ctl_reg {
  uint32_t raw;
  struct {
    uint_t grp0_enabled : 1;
    uint_t grp1_enabled : 1;
    uint_t ack_ctl : 1;
    uint_t grp0_fiq : 1;
    uint_t bpr_aliased : 1;
    uint_t sig_bypass_fiq : 1;
    uint_t sig_bypass_irq : 1;
    uint_t sig_bypass_fiq_alias : 1;
    uint_t sig_bypass_irq_alias : 1;
    uint_t dir_required_for_eoi : 1;
    uint_t dir_required_for_eoi_alias : 1;
    // Rest reserved
  };
} gicc_ctl_reg_t;
ASSERT_SIZE(gicc_ctl_reg_t, 4);

typedef union gicc_priority_reg {
  uint32_t raw;
  struct {
    uint_t priority : 8;
    // Rest reserved
  };
} gicc_priority_reg_t;
ASSERT_SIZE(gicc_priority_reg_t, 4);

typedef union gicc_binary_point_reg {
  uint32_t raw;
  struct {
    uint_t point : 3;
    // Rest reserved
  };
} gicc_binary_point_reg_t;
ASSERT_SIZE(gicc_binary_point_reg_t, 4);

typedef union gic_int_info {
  uint32_t raw;
  struct {
    uint_t int_id : 10;
    uint_t cpu_id : 3;
  };
} gic_int_info_t;
typedef gic_int_info_t gicc_int_info_reg_t;

ASSERT_SIZE(gicc_int_info_reg_t, 4);

// Acknowleges the interrupt by reading from the ack register
static inline int gic_get_int_info(gic_t *gic, gic_int_info_t *info) {
  info->raw = LOAD_GICC_REG(gic, GICC_IAR_OFFSET);
  return 0;
}
// Signals end of interrupt to the GIC
static __attribute__((noinline)) int gic_end_of_int(gic_t *gic, gic_int_info_t *info) {
  STORE_GICC_REG(gic, GICC_EOIR_OFFSET, info->raw);
  return 0;
}

typedef union gicc_interface_id_reg {
  uint32_t raw;
  struct {
    uint_t impl : 12;
    uint_t revision : 4;
    uint_t arch_version : 4;
    uint_t product_id : 12;
  };
} gicc_interface_id_reg_t;
ASSERT_SIZE(gicc_interface_id_reg_t, 4);

#endif
