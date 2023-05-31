
#define PAGE_SIZE_4KB 0x1000
#define PAGE_SIZE_16KB 0x4000
#define PAGE_SIZE_64KB 0x10000

#include<nautilus/naut_types.h>
#include<nautilus/nautilus.h>
#include<arch/arm64/sys_reg.h>

#ifndef NAUT_CONFIG_DEBUG_PRINTS
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

#define PAGING_PRINT(fmt, args...) INFO_PRINT("paging: " fmt, ##args)
#define PAGING_DEBUG(fmt, args...) DEBUG_PRINT("paging: " fmt, ##args)
#define PAGING_ERROR(fmt, args...) ERROR_PRINT("paging: " fmt, ##args)
#define PAGING_WARN(fmt, args...) WARN_PRINT("paging: " fmt, ##args)

#define TCR_GRANULE_SIZE_4KB 0b10
#define TCR_GRANULE_SIZE_16KB 0b01
#define TCR_GRANULE_SIZE_64KB 0b11

#define TCR_DEFAULT_CACHEABILITY 0b11
#define TCR_DEFAULT_SHAREABILITY 0b11

typedef union tc_reg {
  uint64_t raw;
  struct {
    uint_t low_mem_msb_mask : 6;
    uint_t __res0_0 : 1;
    uint_t ttbr0_do_not_walk : 1;
    uint_t ttbr0_table_inner_cacheability : 2;
    uint_t ttbr0_table_outer_cacheability : 2;
    uint_t ttbr0_table_shareability : 2;
    uint_t ttbr0_granule_size : 2;
    uint_t high_mem_msb_mask : 6;
    uint_t high_mem_defines_asid : 1;
    uint_t ttbr1_do_not_walk : 1;
    uint_t ttbr1_table_inner_cacheability : 2;
    uint_t ttbr1_table_outer_cacheability : 2;
    uint_t ttbr1_table_shareability : 2;
    uint_t ttbr1_granule_size : 2;
    uint_t ips_size : 3;
    uint_t __res0_1 : 1;
    uint_t asid_16bit : 1;
    uint_t ttbr0_top_bit_ignored : 1;
    uint_t ttbr1_top_bit_ignored : 1;
    // There are more fields but these are more than enough
  };
} tc_reg_t;

_Static_assert(sizeof(tc_reg_t) == 8, "TCR structure is not exactly 8 bytes wide!\n");

#define PAGE_TABLE_DESC_TYPE_INVALID 0b00
#define PAGE_TABLE_DESC_TYPE_BLOCK 0b01
#define PAGE_TABLE_DESC_TYPE_TABLE 0b11

typedef union page_table_descriptor {
  uint64_t raw;
  struct {
    uint_t desc_type : 2;
    uint_t mair_index : 3;
    uint_t security : 1;
    uint_t access_permission : 2;
    uint_t writeable : 1;
    uint_t shareable_attr : 2;
    uint_t accessed : 1;

    uint64_t __address_data : 40;

    uint_t priv_exec_never : 1;
    uint_t unpriv_exec_never : 1;
    uint_t extra_data : 4;
    uint_t __res0_1 : 6;
  };
} page_table_descriptor_t;

_Static_assert(sizeof(page_table_descriptor_t) == 8, "Page table descriptor structure is not 8 bytes wide!\n");

// These are some common ones
#define MAIR_ATTR_DEVICE_nGnRnE 0b00000000
#define MAIR_ATTR_NORMAL_NON_CACHEABLE 0b01000100
#define MAIR_ATTR_NORMAL_INNER_OUTER_WB_CACHEABLE 0b11111111

#define MAIR_NORMAL_INDEX 0
#define MAIR_DEVICE_INDEX 1

typedef union mair_reg {
  uint64_t raw;
  uint8_t attrs[8];
} mair_reg_t;

_Static_assert(sizeof(mair_reg_t) == 8, "MAIR Register Structure is not 8 bytes wide!");

static page_table_descriptor_t *generate_invalid_page_table(void) {

  page_table_descriptor_t *l0_table = malloc(sizeof(page_table_descriptor_t) * 512);

  if(l0_table == NULL) {
    goto err_exit_nalloc;
  }

  if((uint64_t)l0_table & 0xFFF) {
    goto err_exit_l0;
  }

  for(uint_t i = 0; i < 512; i++) {
    l0_table[i].desc_type = PAGE_TABLE_DESC_TYPE_INVALID;
  }

  return l0_table;

err_exit_l0:
  free(l0_table);
err_exit_nalloc:
  return NULL;
}

static page_table_descriptor_t *generate_minimal_page_table(void) {

  page_table_descriptor_t *l0_table = malloc(sizeof(page_table_descriptor_t) * 512);

  if(l0_table == NULL) {
    goto err_exit_nalloc;
  }

  if((uint64_t)l0_table & 0xFFF) {
    goto err_exit_l0;
  }

  for(uint_t i = 1; i < 512; i++) {
    l0_table[i].desc_type = PAGE_TABLE_DESC_TYPE_INVALID;
  }

  // Initialize the first 4TB as device memory
  page_table_descriptor_t *l1_table = malloc(sizeof(page_table_descriptor_t) * 512);

  if(l1_table == NULL) {
    goto err_exit_l0;
  }

  if((uint64_t)l1_table & 0xFFF) {
    goto err_exit_l1;
  }

  l0_table[0].raw = 0;
  l0_table[0].desc_type = PAGE_TABLE_DESC_TYPE_TABLE;
  l0_table[0].raw |= ((uint64_t)l1_table & ~0xFFF);

  for(uint64_t i = 0; i < 512; i++) {
    l1_table[i].desc_type = PAGE_TABLE_DESC_TYPE_BLOCK;
    l1_table[i].mair_index = MAIR_DEVICE_INDEX;
    l1_table[i].access_permission = 0;
    l1_table[i].writeable = 1;
    l1_table[i].shareable_attr = 0b11;
    l1_table[i].priv_exec_never = 0;
    l1_table[i].unpriv_exec_never = 0;
    
    l1_table[i].__address_data = 0;
    l1_table[i].raw |= (i<<30);
  }

  return l0_table;

err_exit_l1:
  free(l1_table);
err_exit_l0:
  free(l0_table);
err_exit_nalloc:
  return NULL;
}

int arch_paging_init(struct nk_mem_info *mm_info, void *fdt) {
 
  mair_reg_t mair;
  LOAD_SYS_REG(MAIR_EL1, mair.raw);
  mair.attrs[MAIR_DEVICE_INDEX] = MAIR_ATTR_DEVICE_nGnRnE;
  mair.attrs[MAIR_NORMAL_INDEX] = MAIR_ATTR_NORMAL_INNER_OUTER_WB_CACHEABLE;
  STORE_SYS_REG(MAIR_EL1, mair.raw);

  PAGING_DEBUG("Configured MAIR register: MAIR_EL1 = 0x%x\n", mair.raw);

  page_table_descriptor_t *low_mem_table = generate_minimal_page_table();

  if(low_mem_table == NULL) {
    PAGING_ERROR("Could not create low mem page table!\n");
    return -1;
  }

  page_table_descriptor_t *high_mem_table = generate_invalid_page_table();

  if(high_mem_table == NULL) {
    PAGING_ERROR("Could not create high mem page table!\n");
    return -1;
  }

  STORE_SYS_REG(TTBR0_EL1, (uint64_t)low_mem_table);
  STORE_SYS_REG(TTBR1_EL1, (uint64_t)high_mem_table);

  PAGING_DEBUG("Set Low and High Mem Page Tables: TTBR0_EL1 = 0x%x, TTBR1_EL1 = 0x%x\n", (uint64_t)low_mem_table, (uint64_t)high_mem_table);

  tc_reg_t tcr;

  LOAD_SYS_REG(TCR_EL1, tcr.raw);

  tcr.low_mem_msb_mask = 16;
  tcr.high_mem_msb_mask = 16;

  tcr.ttbr0_do_not_walk = 0;
  tcr.ttbr0_granule_size = TCR_GRANULE_SIZE_4KB;
  tcr.ttbr0_top_bit_ignored = 0;
  tcr.ttbr0_table_inner_cacheability = TCR_DEFAULT_CACHEABILITY;
  tcr.ttbr0_table_outer_cacheability = TCR_DEFAULT_CACHEABILITY;
  tcr.ttbr0_table_shareability = TCR_DEFAULT_SHAREABILITY;
  
  tcr.ttbr1_do_not_walk = 0;
  tcr.ttbr1_granule_size = TCR_GRANULE_SIZE_4KB;
  tcr.ttbr1_top_bit_ignored = 0;
  tcr.ttbr1_table_inner_cacheability = TCR_DEFAULT_CACHEABILITY;
  tcr.ttbr1_table_outer_cacheability = TCR_DEFAULT_CACHEABILITY;
  tcr.ttbr1_table_shareability = TCR_DEFAULT_SHAREABILITY;

  tcr.high_mem_defines_asid = 0;
  tcr.asid_16bit = 0;
  tcr.ips_size = 0b101; // 48 bits physical

  STORE_SYS_REG(TCR_EL1, tcr.raw);

  PAGING_DEBUG("Set TCR_EL1 = 0x%x\n", tcr.raw);

  // Instruction barrier
  asm volatile ("isb");

  PAGING_DEBUG("Enabling the MMU\n");

  sys_ctrl_reg_t sys_ctrl_reg;

  LOAD_SYS_REG(SCTLR_EL1, sys_ctrl_reg.raw);

  sys_ctrl_reg.mmu_en = 1;
  sys_ctrl_reg.write_exec_never = 0;

  STORE_SYS_REG(SCTLR_EL1, sys_ctrl_reg.raw);

  asm volatile ("isb");

  PAGING_DEBUG("MMU Enabled\n");

  return 0;
}

