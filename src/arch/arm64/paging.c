#include<nautilus/naut_types.h>
#include<nautilus/nautilus.h>
#include<nautilus/of/fdt.h>
#include<nautilus/macros.h>
#include<nautilus/mm.h>

#include<arch/arm64/sys_reg.h>
#include<arch/arm64/paging.h>

// Allocation
struct page_table *pt_allocate_table(void) 
{
  struct page_table *table = mm_boot_alloc(sizeof(struct page_table));
  if(table != NULL) {
    memset(table, 0, sizeof(struct page_table));
  }
  return table;
}
void pt_free_table(struct page_table *table) 
{
  mm_boot_free(table, sizeof(struct page_table));
}

union pt_desc * pt_allocate_level(struct page_table *table, int level) 
{
  int num_entries = pt_level_num_entries(table, level);
  int align = pt_level_alignment(table, level);
  union pt_desc *level_ptr = (union pt_desc*)mm_boot_alloc_aligned(sizeof(union pt_desc) * num_entries, align); 
  if(level_ptr != NULL) {
    memset(level_ptr, 0, sizeof(union pt_desc*) * num_entries);
  }
  return level_ptr;
}

void pt_free_level(union pt_desc *level_ptr, struct page_table *table, int level) 
{
  int num_entries = pt_level_num_entries(table, level);
  mm_boot_free(level_ptr, sizeof(union pt_desc) * num_entries);
}

int pt_block_to_table(union pt_desc *desc, int level, struct page_table *table)
{
  PAGING_DEBUG("pt_block_to_table(desc.raw=0x%016x, level=%u, table=%p)\n", desc->raw, level, table);
  if(level == table->page_level) {
    PAGING_ERROR("Trying to turn page descriptor into a table descriptor!\n");
    return -1;
  }

  int table_level = level+1;

  uint64_t size = pt_level_entry_size(table, table_level);
  int align = pt_level_alignment(table, table_level);
  int count = pt_level_num_entries(table, table_level);

  if(size <= 0) {
    PAGING_ERROR("invalid pt_level_entry_size(level = %u) = %u\n", table_level, size);
    return -1;
  }
  if(count <= 0) {
    PAGING_ERROR("invalid pt_level_num_entries(level = %u) = %u\n", table_level, count);
    return -1;
  }
 
  PAGING_DEBUG("\tNew Block Size = 0x%x\n", size);
  PAGING_DEBUG("\tNew Table Count = %u\n", count);
  PAGING_DEBUG("\tNew Table Alignment = %u\n", align);

  struct pt_block_desc *block_desc = (struct pt_block_desc*)desc;

  if(block_desc->is_table) {
    PAGING_ERROR("Called \"block_to_table\" on block descriptor with \"table\" bit set!\n");
    return -1;
  } 
   
  uint64_t paddr = (uint64_t)PT_BLOCK_DESC_PTR(block_desc);
  PAGING_DEBUG("\tCurrent PADDR = %p\n", paddr);
 
  union pt_desc *desc_table = pt_allocate_level(table, table_level);
  if(desc_table == NULL) {
    PAGING_ERROR("Failed to allocate page table level = %u\n", table_level);
    return -1;
  }

  struct pt_perm perm = {};
  pt_add_perm_from_block_desc(block_desc, &perm);

  for(uint32_t i = 0; i < count; i++) {
    uint64_t new_paddr = (paddr + (size * i));


    if(table_level == table->page_level) {
      PT_PAGE_DESC_SET_PTR(&desc_table[i].page, new_paddr);
      desc_table[i].page.reserved = 1;
      pt_write_all_perm_to_page_desc(table, &desc_table[i].page, &perm);
    } else {
      PT_BLOCK_DESC_SET_PTR(&desc_table[i].block, new_paddr);
      desc_table[i].block.block_translation = block_desc->block_translation;
      desc_table[i].block.is_table = 0;
      pt_write_all_perm_to_block_desc(table, &desc_table[i].block, &perm);
   }

    desc_table[i].valid = block_desc->valid;
    PAGING_DEBUG("\tDrilled new entry: PADDR = %p, RAW = 0x%016x\n", new_paddr, desc_table[i].raw);
  }

  PAGING_DEBUG("Original Block: PADDR = %p, RAW = 0x%016x\n", PT_BLOCK_DESC_PTR(block_desc), ((union pt_desc*)block_desc)->raw);

  struct pt_table_desc *table_desc = (struct pt_table_desc*)block_desc;

  table_desc->valid = 1;
  table_desc->is_table = 1;
  PT_TABLE_DESC_SET_PTR(table_desc, (uint64_t)desc_table);

  // Zero the hierarchical values
  table_desc->user = 0; 
  table_desc->readonly = 0; 
  table_desc->priv_exec_never = 0;
  table_desc->unpriv_exec_never = 0;
  table_desc->non_secure = 0;

  return 0;
}

int pt_drill_range(struct page_table *table, uint64_t start, uint64_t paddr, uint64_t size, struct pt_perm perm, int valid, int rounding) 
{
  int level;
  uint64_t block_size;
  union pt_desc *(*drill_func)(struct page_table*, void*); 

  uint64_t end = start + size;

  uint64_t adjusted_start = start;

  int res;
  if((res = pt_select_drill_func(table, &adjusted_start, &end, &drill_func, &block_size, &level, rounding))) {
    PAGING_ERROR("Cannot drill range: pt_select_drill_func returned %u!\n", res);
    return -1;
  }

  // Fix paddr if start was modified
  paddr += (adjusted_start - start);
  start = adjusted_start;
  PAGING_DEBUG("Drilling Range: [%p - %p]%s%s%s%s\n", (void*)start, (void*)end,
      valid ? " VALID" : "",
      perm.priv_exec_never ? " PXN" : "",
      perm.unpriv_exec_never ? " UXN" : "",
      perm.user ? " USER" : "",
      perm.readonly ? "RD_ONLY" : "");

  for(; start < end; start += block_size, paddr += block_size) {
    union pt_desc *desc;
    desc = (*drill_func)(table, (void*)start);

    if(desc == NULL) {
      PAGING_WARN("Failed to drill page in pt_drill_range!\n");
      return -1;
    }

    if(level == table->page_level) {
      struct pt_page_desc *page_desc = &desc->page;
      pt_write_all_perm_to_page_desc(table, page_desc, &perm);
      PT_PAGE_DESC_SET_PTR(page_desc, paddr);
      page_desc->reserved = 1;
      page_desc->valid = valid;
    } else {
      if(desc->table.is_table) {
        PAGING_WARN("Drill function returned pt_table_desc* when drilling range!\n"); 
      }
      struct pt_block_desc *block_desc = &desc->block;
      pt_write_all_perm_to_block_desc(table, block_desc, &perm);
      PT_BLOCK_DESC_SET_PTR(block_desc, paddr);
      block_desc->is_table = 0;
      block_desc->valid = valid;
    }
  }
  return 0;
}

static inline int pt_get_paddr_bits(id_aa64mmfr0_el1_t *mmfr0) {
  switch(mmfr0->pa_range) {
    case 0b0000:
      return 32;
    case 0b0001:
      return 36;
    case 0b0010:
      return 40;
    case 0b0011:
      return 42;
    case 0b0100:
      return 44;
    case 0b0101:
      return 48;
    case 0b0110:
      return 52;
    case 0b0111:
      return 56;
    default:
      PAGING_ERROR("Could not read physical address bits from ID_AA64MMFR0_EL1!\n");
      return 0;
  }
}

static inline uint8_t ttbr_ips_value(int paddr_bits) {
  switch(paddr_bits) {
    case 32:
      return 0b000;
    case 36:
      return 0b001;
    case 40:
      return 0b010;
    case 42:
      return 0b011;
    case 44:
      return 0b100;
    case 48:
      return 0b101;
    case 52:
      return 0b110;
    case 56:
      return 0b111;
    default:
      PAGING_WARN("Invalid physical address bits for TTBRn_EL1.IPS! paddr_bits=%u (defaulting to assuming 48bit physical addresses)\n", paddr_bits);
      return 0b101;
  }
}

static int handle_normal_memory_regions(struct page_table *table, void *fdt) {

  int offset = fdt_node_offset_by_prop_value(fdt, -1, "device_type", "memory", 7); 

  int err_count = 0;

  while(offset != -FDT_ERR_NOTFOUND) {
    fdt_reg_t reg = {.address = 0, .size = 0};
    int getreg_result = fdt_getreg(fdt, offset, &reg);

    if(getreg_result == 0) {   
      uint64_t paddr_start = reg.address;
      uint64_t size = reg.size;
      if(pt_init_table_normal(table, paddr_start, paddr_start, size)) {
        PAGING_ERROR("Error initializing normal memory region: [%u - %u]!\n", paddr_start, paddr_start + size);
        err_count += 1;
      }
    }

    offset = fdt_node_offset_by_prop_value(fdt, offset, "device_type", "memory", 7);
  }

  return err_count;
}

static int handle_text_memory_regions(struct page_table *table) {
  extern int _textStart[];
  extern int _textEnd[];

  uint64_t start = (uint64_t)_textStart;
  uint64_t end = (uint64_t)_textEnd;

  PAGING_DEBUG("Setting .text Region as Executable: [0x%016x - 0x%016x]\n", start, end);

  if(pt_init_table_text(table, start, start, end-start)) {
    return -1;
  }

  return 0;
}

static int handle_rodata_memory_regions(struct page_table *table) {
  extern int _rodataStart[];
  extern int _rodataEnd[];

  uint64_t start = (uint64_t)_rodataStart;
  uint64_t end = (uint64_t)_rodataEnd;

  PAGING_DEBUG("Drilling .rodata section\n");
  if(pt_init_table_rodata(table, start, start, end-start)) {
    PAGING_ERROR("Failed to drill .rodata section!\n");
    return -1;
  }

  PAGING_DEBUG("Drilling FDT as readonly\n");
  void *fdt = (void*)nautilus_info.sys.dtb;
  if(pt_init_table_rodata(table, fdt, fdt, fdt_totalsize(fdt))) {
    PAGING_ERROR("Failed to drill FDT as readonly!\n");
    return -1;
  }

  return 0;
}

static struct page_table *ttbr0_table;
static struct page_table *ttbr1_table;
static int paddr_bits;

int arch_paging_init(struct nk_mem_info *mm_info, void *fdt) {

  PAGING_PRINT("init\n");

  id_aa64mmfr0_el1_t mmfr0; 
  LOAD_SYS_REG(ID_AA64MMFR0_EL1, mmfr0.raw);
  paddr_bits = pt_get_paddr_bits(&mmfr0);
  int tnsz = 64 - paddr_bits;

  PAGING_DEBUG("Physical Address Bits = %u\n", paddr_bits);
  PAGING_DEBUG("tnsz = %u\n", tnsz);

  ttbr0_table = pt_create_table_4kb(tnsz);

  if(ttbr0_table == NULL) {
    PAGING_ERROR("Could not create low mem page table!\n");
    return -1;
  }

  if(pt_init_table_device(ttbr0_table, 0, 0, 1ULL<<paddr_bits)) {
    PAGING_ERROR("Failed to handle device page regions!\n");
    return -1;
  }

#ifdef NAUT_CONFIG_DEBUG_PAGING
  //PAGING_DEBUG("TTBR0 Table with device memory region:\n");
  //pt_dump_page_table(ttbr0_table);
#endif

  if(handle_normal_memory_regions(ttbr0_table, fdt)) {
    PAGING_ERROR("Failed to handle normal page regions! (All memory is device memory)\n");
    return -1;
  }

#ifdef NAUT_CONFIG_DEBUG_PAGING
  //PAGING_DEBUG("TTBR0 Table with normal memory region:\n");
  //pt_dump_page_table(ttbr0_table);
#endif

  if(handle_text_memory_regions(ttbr0_table)) {
    PAGING_ERROR("Failed to handle text page regions!\n");
    return -1;
  }

#ifdef NAUT_CONFIG_DEBUG_PAGING
  //PAGING_DEBUG("TTBR0 Table with .text region:\n");
  //pt_dump_page_table(ttbr0_table);
#endif

  if(handle_rodata_memory_regions(ttbr0_table)) {
    PAGING_ERROR("Failed to handle rodata page regions! (Read only sections may be modified without an exception!)\n");
    return -1;
  }

#ifdef NAUT_CONFIG_DEBUG_PAGING
  //PAGING_DEBUG("TTBR0 Table with .rodata region:\n");
  //pt_dump_page_table(ttbr0_table);
#endif

  ttbr1_table = pt_create_table_4kb(tnsz);

  if(ttbr1_table == NULL) {
    PAGING_ERROR("Could not create high mem page table!\n");
    return -1;
  }

  if(pt_init_table_invalid(ttbr1_table)) {
    PAGING_ERROR("Could not set high mem page table as invalid!\n");
    return -1;
  }

  PAGING_PRINT("globally initialized\n");

  return 0;
}

int per_cpu_paging_init(void) {

  PAGING_PRINT("per_cpu init\n");

  mair_el1_t mair;
  LOAD_SYS_REG(MAIR_EL1, mair.raw);
  mair.attrs[MAIR_DEVICE_INDEX] = MAIR_ATTR_DEVICE_nGnRnE;
  mair.attrs[MAIR_NORMAL_INDEX] = MAIR_ATTR_NORMAL_INNER_OUTER_WB_CACHEABLE;
  STORE_SYS_REG(MAIR_EL1, mair.raw);

  PAGING_DEBUG("Configured MAIR register: MAIR_EL1 = 0x%x\n", mair.raw);

  // Setting the TTBR Registers
  ttbr0_el1_t ttbr0;
  ttbr1_el1_t ttbr1;

  ttbr0.raw = 0;
  ttbr1.raw = 0;

  ttbr0_el1_set_base_addr(&ttbr0, ttbr0_table->root_ptr);
  ttbr1_el1_set_base_addr(&ttbr1, ttbr1_table->root_ptr);

  STORE_SYS_REG(TTBR0_EL1, ttbr0.raw);
  STORE_SYS_REG(TTBR1_EL1, ttbr1.raw);

  PAGING_DEBUG("Set Low and High Mem Page Tables: TTBR0_EL1 = 0x%x, TTBR1_EL1 = 0x%x\n", ttbr0.raw, ttbr1.raw);

  // Configuring TCR_EL1 using the Memory Model Feature Registers
  id_aa64mmfr0_el1_t mmfr0; 
  LOAD_SYS_REG(ID_AA64MMFR0_EL1, mmfr0.raw);
  id_aa64mmfr1_el1_t mmfr1;
  LOAD_SYS_REG(ID_AA64MMFR1_EL1, mmfr1.raw);
  id_aa64mmfr2_el1_t mmfr2; 
  LOAD_SYS_REG(ID_AA64MMFR2_EL1, mmfr2.raw); 

  tcr_el1_t tcr;
  LOAD_SYS_REG(TCR_EL1, tcr.raw);

  tcr.ttbr0_top_byte_ignored = 0;
  tcr.ttbr1_top_byte_ignored = 0;

  tcr.ttbr0_table_inner_cacheability = TCR_DEFAULT_CACHEABILITY;
  tcr.ttbr1_table_inner_cacheability = TCR_DEFAULT_CACHEABILITY;

  tcr.ttbr0_table_outer_cacheability = TCR_DEFAULT_CACHEABILITY;
  tcr.ttbr1_table_outer_cacheability = TCR_DEFAULT_CACHEABILITY;

  tcr.ttbr0_table_shareability = TCR_DEFAULT_SHAREABILITY;
  tcr.ttbr1_table_shareability = TCR_DEFAULT_SHAREABILITY;
 
  tcr.t0sz = ttbr0_table->tnsz;
  tcr.t1sz = ttbr1_table->tnsz;

  tcr.ttbr0_do_not_walk = 0;
  tcr.ttbr1_do_not_walk = 0;

  if(mmfr0.tgran_4kb != 0b1111) {
    tcr.ttbr0_granule_size = TCR_TTBR0_GRANULE_SIZE_4KB;
    tcr.ttbr1_granule_size = TCR_TTBR1_GRANULE_SIZE_4KB;
  } else {
    PAGING_ERROR("4KB Translation Granule is not supported!\n");
    return -1;
  } 

  if(mmfr1.hier_perm_disable_sup) {
    ttbr0_table->flags &= ~PAGE_TABLE_FLAG_HIERARCHY_ENABLED;
    ttbr1_table->flags &= ~PAGE_TABLE_FLAG_HIERARCHY_ENABLED;
    tcr.ttbr0_disable_hierarchy = 1;
    tcr.ttbr1_disable_hierarchy = 1;
  } else {
    PAGING_WARN("Hierarchical Permissions Disable is not supported!\n");
    ttbr0_table->flags |= PAGE_TABLE_FLAG_HIERARCHY_ENABLED;
    ttbr1_table->flags |= PAGE_TABLE_FLAG_HIERARCHY_ENABLED;
    tcr.ttbr0_disable_hierarchy = 0;
    tcr.ttbr1_disable_hierarchy = 0;
  }

  tcr.ttbr1_defines_asid = 0;
  if(mmfr0.asid_bits == 2) {
    PAGING_DEBUG("Using 16-bit ASID\n");
    tcr.asid_16bit = 1;
  } else {
    PAGING_DEBUG("Using 8-bit ASID\n");
    tcr.asid_16bit = 0;
  }

  tcr.ips = ttbr_ips_value(pt_get_paddr_bits(&mmfr0));

  tcr.hw_update_access_flag = 0;
  tcr.hw_update_dirty_bit = 0;

#ifdef NAUT_CONFIG_DEBUG_PAGING
  dump_tcr_el1(tcr);
  pt_dump_page_table(ttbr0_table);
#endif

  STORE_SYS_REG(TCR_EL1, tcr.raw);

  PAGING_DEBUG("Enabling the MMU\n");

  asm volatile ("isb");

  extern void arm64_mmu_enable(void);
  arm64_mmu_enable();

  PAGING_PRINT("MMU Enabled\n");

  return 0;
}

// Granule Size Mutexed Functions
#define PAGING_SWITCH_FUNC_CALL_GRANULE_SIZE(table, func_name, ...) \
  do { \
  switch(table->granule_size) { \
    case 4: \
      return func_name ## _4kb(__VA_ARGS__); \
    case 16: \
    case 64: \
      PAGING_ERROR("Granule Size 16KB or 64KB are unimplemented!\n"); \
      break; \
    default: \
      PAGING_ERROR("Unknown Granule Size in " #func_name "!\n"); \
      break; \
  } \
  } while(0)

int pt_select_drill_func(struct page_table *table, uint64_t *start, uint64_t *end, union pt_desc*(**drill_func)(struct page_table*,void*), uint64_t *block_size, int *level, int rounding) {
  extern int pt_select_drill_func_4kb(struct page_table *, uint64_t *, uint64_t *, union pt_desc*(**)(struct page_table*,void*), uint64_t *, int*, int);
  //extern int pt_select_drill_func_16kb(struct page_table *, uint64_t *, uint64_t *, union pt_desc*(**)(struct page_table*,void*), uint64_t *, int*, int);
  //extern int pt_select_drill_func_64kb(struct page_table *, uint64_t *, uint64_t *, union pt_desc*(**)(struct page_table*,void*), uint64_t *, int*, int);
  
  // This places the correct call and returns, or continues if an error occurred
  PAGING_SWITCH_FUNC_CALL_GRANULE_SIZE(table, pt_select_drill_func, table, start, end, drill_func, block_size, level, rounding);
  // We haven't returned so an error occurred
  return -1;
}

int pt_level_entry_size(struct page_table *table, int level) {
  extern int pt_level_entry_size_4kb(struct page_table *table, int level);
  //extern int pt_level_entry_size_16kb(struct page_table *table, int level);
  //extern int pt_level_entry_size_64kb(struct page_table *table, int level);
  PAGING_SWITCH_FUNC_CALL_GRANULE_SIZE(table, pt_level_entry_size, table, level);
  return 0;
}
int pt_level_alignment(struct page_table *table, int level) {
  extern int pt_level_alignment_4kb(struct page_table *table, int level);
  //extern int pt_level_alignment_16kb(struct page_table *table, int level);
  //extern int pt_level_alignment_64kb(struct page_table *table, int level);
  PAGING_SWITCH_FUNC_CALL_GRANULE_SIZE(table, pt_level_alignment, table, level);
  return -1;
}
int pt_level_num_entries(struct page_table *table, int level) {
  extern int pt_level_num_entries_4kb(struct page_table *table, int level);
  //extern int pt_level_num_entries_16kb(struct page_table *table, int level);
  //extern int pt_level_num_entries_64kb(struct page_table *table, int level);
  PAGING_SWITCH_FUNC_CALL_GRANULE_SIZE(table, pt_level_num_entries, table, level);
  return 0;
}

void pt_dump_page_table(struct page_table *table) {
  extern void pt_dump_page_table_4kb(struct page_table *table);
  //extern void pt_dump_page_table_16kb(struct page_table *table);
  //extern void pt_dump_page_table_64kb(struct page_table *table);
  PAGING_SWITCH_FUNC_CALL_GRANULE_SIZE(table, pt_dump_page_table, table);
}

int pt_init_table_invalid(struct page_table *table) 
{
  union pt_desc *root = pt_allocate_level(table, table->root_level);
  if(root == NULL) {
    PAGING_ERROR("Failed to allocate invalid page table root level!\n");
    return -1;
  }
  int count = pt_level_num_entries(table, table->root_level);
  for(int i = 0; i < count; i++) {
    root[i].valid = 0;
  }
  table->root_ptr = root;
  return 0;
}

int pt_init_table_device(struct page_table *table, uint64_t vaddr_start, uint64_t paddr_start, uint64_t size)
{
  static struct pt_perm device_perm = {
    .user = 0,
    .readonly = 0,
    .priv_exec_never = 1,
    .unpriv_exec_never = 1,
    .mair_index = MAIR_DEVICE_INDEX,
    .shareability = OUTER_SHAREABLE,
    .access_flag = 1,
    .non_global = 0,
    .hw_dirty_update = 0,
    .contiguous = 0,
  };

  PAGING_DEBUG("Drilling device memory region\n");
  return pt_drill_range(table, vaddr_start, paddr_start, size, device_perm, 1, PT_RANGE_ROUND_OUT);
}
int pt_init_table_normal(struct page_table *table, uint64_t vaddr_start, uint64_t paddr_start, uint64_t size)
{
  static struct pt_perm normal_perm = {
    .user = 0,
    .readonly = 0,
    .priv_exec_never = 1,
    .unpriv_exec_never = 1,
    .mair_index = MAIR_NORMAL_INDEX,
    .shareability = OUTER_SHAREABLE,
    .access_flag = 1,
    .non_global = 0,
    .hw_dirty_update = 0,
    .contiguous = 0,
  };

  PAGING_DEBUG("Drilling normal memory region\n");
  return pt_drill_range(table, vaddr_start, paddr_start, size, normal_perm, 1, PT_RANGE_ROUND_IN);
}

int pt_init_table_text(struct page_table *table, uint64_t vaddr_start, uint64_t paddr_start, uint64_t size)
{
  static struct pt_perm text_perm = {
    .user = 0,
    .readonly = 1,
    .priv_exec_never = 0,
    .unpriv_exec_never = 1,
    .mair_index = MAIR_NORMAL_INDEX,
    .shareability = OUTER_SHAREABLE,
    .access_flag = 1,
    .non_global = 0,
    .hw_dirty_update = 0,
    .contiguous = 0,
  };

  PAGING_DEBUG("Drilling text memory region\n");
  return pt_drill_range(table, vaddr_start, paddr_start, size, text_perm, 1, PT_RANGE_ROUND_OUT);
}

int pt_init_table_rodata(struct page_table *table, uint64_t vaddr_start, uint64_t paddr_start, uint64_t size)
{
  static struct pt_perm rodata_perm = {
    .user = 0,
    .readonly = 1,
    .priv_exec_never = 1,
    .unpriv_exec_never = 1,
    .mair_index = MAIR_NORMAL_INDEX,
    .shareability = OUTER_SHAREABLE,
    .access_flag = 1,
    .non_global = 0,
    .hw_dirty_update = 0,
    .contiguous = 0,
  };

  PAGING_DEBUG("Drilling rodata memory region\n");
  return pt_drill_range(table, vaddr_start, paddr_start, size, rodata_perm, 1, PT_RANGE_ROUND_IN);
}

