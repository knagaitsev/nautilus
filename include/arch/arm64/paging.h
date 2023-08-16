#ifndef __ARM64_PAGING_H__
#define __ARM64_PAGING_H__

#include<nautilus/nautilus.h>
#include<nautilus/naut_types.h>

#ifndef NAUT_CONFIG_DEBUG_PAGING
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

#define PAGING_PRINT(fmt, args...) INFO_PRINT("paging: " fmt, ##args)
#define PAGING_DEBUG(fmt, args...) DEBUG_PRINT("paging: " fmt, ##args)
#define PAGING_ERROR(fmt, args...) ERROR_PRINT("paging: " fmt, ##args)
#define PAGING_WARN(fmt, args...) WARN_PRINT("paging: " fmt, ##args)

#ifndef PAGE_SIZE_4KB
#define PAGE_SIZE_4KB 0x1000ULL
#endif
#ifndef PAGE_SIZE_16KB
#define PAGE_SIZE_16KB 0x4000ULL
#endif
#ifndef PAGE_SIZE_64KB
#define PAGE_SIZE_64KB 0x10000ULL
#endif
#ifndef PAGE_SIZE_2MB
#define PAGE_SIZE_2MB 0x200000ULL
#endif
#ifndef PAGE_SIZE_1GB
#define PAGE_SIZE_1GB 0x40000000ULL
#endif

#define TCR_TTBR0_GRANULE_SIZE_4KB 0b00
#define TCR_TTBR0_GRANULE_SIZE_16KB 0b10
#define TCR_TTBR0_GRANULE_SIZE_64KB 0b01

#define TCR_TTBR1_GRANULE_SIZE_4KB 0b10
#define TCR_TTBR1_GRANULE_SIZE_16KB 0b01
#define TCR_TTBR1_GRANULE_SIZE_64KB 0b11

#define NON_SHAREABLE    0b00
#define INNER_SHAREABLE  0b11
#define OUTER_SHAREABLE  0b10

#define TCR_NON_CACHEABLE                    0b00
#define TCR_WRITE_BACK_ALLOCATE_CACHEABLE    0b01
#define TCR_WRITE_THROUGH_ALLOCATE_CACHEABLE 0b10
#define TCR_WRITE_BACK_NO_ALLOCATE_CACHEABLE 0b11

#define TCR_DEFAULT_CACHEABILITY 0b01
#define TCR_DEFAULT_SHAREABILITY 0b10

#define PAGE_TABLE_GRANULE_SIZE_UNKNOWN 0
#define PAGE_TABLE_GRANULE_SIZE_4KB     4
#define PAGE_TABLE_GRANULE_SIZE_16KB    16
#define PAGE_TABLE_GRANULE_SIZE_64KB    64

#define PAGE_TABLE_FLAG_HIERARCHY_ENABLED (1<<0)
#define PAGE_TABLE_FLAG_SECURE            (1<<1)

struct page_table 
{
  uint8_t root_level;
  uint8_t page_level;
  uint8_t granule_size;
  uint8_t tnsz;
  uint32_t flags;
  union pt_desc *root_ptr;
};

// Descriptor Formats

#define PT_LOWER_ATTR \
  uint_t mair_index : 3; \
  uint_t non_secure : 1; \
  uint_t user : 1; \
  uint_t readonly : 1; \
  uint_t shareability : 2; \
  uint_t access_flag : 1; \
  uint_t non_global : 1;

#define PT_UPPER_ATTR \
  uint_t guarded : 1; \
  uint_t hw_dirty_update : 1; \
  uint_t contiguous : 1; \
  uint_t priv_exec_never : 1; \
  uint_t unpriv_exec_never : 1; \
  uint_t sw_use_0 : 4; \
  uint_t __pbha : 4; \
  uint_t sw_use_1 : 1;

#define PT_TABLE_ATTR \
  uint_t sw_use_8 : 8; \
  uint_t priv_exec_never : 1; \
  uint_t unpriv_exec_never : 1; \
  uint_t user : 1; \
  uint_t readonly : 1; \
  uint_t non_secure : 1;

struct pt_table_desc
{
    uint_t valid : 1;
    uint_t is_table : 1;
    uint_t sw_use_10 : 10;
    ulong_t address_data : 36;
    uint_t __res0 : 3;
    PT_TABLE_ATTR // : 13;
} __attribute__((packed));
_Static_assert(sizeof(struct pt_table_desc) == 8, "pt_table_desc struct is not exactly 8 bytes wide!\n");

struct pt_block_desc 
{
    uint_t valid : 1;
    uint_t is_table : 1;
    PT_LOWER_ATTR // : 10;
    uint_t __res0_0 : 4;
    uint_t block_translation : 1; 
    uint_t address_data : 31;
    uint_t __res0_1 : 2;
    PT_UPPER_ATTR // : 14;
} __attribute__((packed));
_Static_assert(sizeof(struct pt_block_desc) == 8, "pt_block_desc struct is not exactly 8 bytes wide!\n");

struct pt_page_desc
{
    uint_t valid : 1;
    uint_t reserved : 1;
    PT_LOWER_ATTR // : 10;  
    ulong_t address_data : 36;
    uint_t __res0 : 2; 
    PT_UPPER_ATTR // : 14;
} __attribute__((packed));
_Static_assert(sizeof(struct pt_page_desc) == 8, "pt_page_desc struct is not exactly 8 bytes wide!\n");

union pt_desc {
  uint64_t raw;

  struct {
    uint_t valid : 1;
  };

  struct pt_table_desc table;
  struct pt_block_desc block;
  struct pt_page_desc page;
} __attribute__((packed));
_Static_assert(sizeof(union pt_desc) == 8, "pt_desc union is not exactly 8 bytes wide!\n");

struct pt_perm {
  uint_t non_secure : 1;
  uint_t user : 1;
  uint_t readonly : 1;
  uint_t priv_exec_never : 1;
  uint_t unpriv_exec_never : 1;
  uint_t mair_index : 3;
  uint_t shareability : 2;
  uint_t access_flag : 1;
  uint_t non_global : 1;
  uint_t hw_dirty_update : 1;
  uint_t contiguous : 1;
  uint_t guarded : 1;
};

inline static void pt_write_all_perm_to_block_desc(struct page_table *table, struct pt_block_desc *desc, struct pt_perm *perm) {
  desc->non_secure = perm->non_secure;
  desc->user = perm->user;
  desc->readonly = perm->readonly;
  desc->priv_exec_never = perm->priv_exec_never;
  desc->unpriv_exec_never = perm->unpriv_exec_never;
  desc->mair_index = perm->mair_index;
  desc->shareability = perm->shareability;
  desc->access_flag = perm->access_flag;
  desc->non_global = perm->non_global;
  desc->hw_dirty_update = perm->hw_dirty_update;
  desc->contiguous = perm->contiguous; // res0 for blocks
  desc->guarded = perm->guarded;
}
inline static void pt_add_perm_from_block_desc(struct pt_block_desc *desc, struct pt_perm *perm) 
{
  perm->non_secure |= desc->non_secure;
  perm->user |= desc->user;
  perm->readonly |= desc->readonly;
  perm->priv_exec_never |= desc->priv_exec_never;
  perm->unpriv_exec_never |= desc->unpriv_exec_never;
  perm->mair_index = desc->mair_index;
  perm->shareability = desc->shareability;
  perm->access_flag = desc->access_flag;
  perm->non_global = desc->non_global;
  perm->hw_dirty_update = desc->hw_dirty_update;
  perm->contiguous = desc->contiguous; // res0 for blocks
  perm->guarded = desc->guarded;
}
inline static void pt_write_all_perm_to_page_desc(struct page_table *table, struct pt_page_desc *desc, struct pt_perm *perm) {
  desc->non_secure = perm->non_secure;
  desc->user = perm->user;
  desc->readonly = perm->readonly;
  desc->priv_exec_never = perm->priv_exec_never;
  desc->unpriv_exec_never = perm->unpriv_exec_never;
  desc->mair_index = perm->mair_index;
  desc->shareability = perm->shareability;
  desc->access_flag = perm->access_flag;
  desc->non_global = perm->non_global;
  desc->hw_dirty_update = perm->hw_dirty_update;
  desc->contiguous = perm->contiguous; 
  desc->guarded = perm->guarded;
}
inline static void pt_add_perm_from_page_desc(struct pt_page_desc *desc, struct pt_perm *perm) 
{
  perm->non_secure |= desc->non_secure;
  perm->user |= desc->user;
  perm->readonly |= desc->readonly;
  perm->priv_exec_never |= desc->priv_exec_never;
  perm->unpriv_exec_never |= desc->unpriv_exec_never;
  perm->mair_index = desc->mair_index;
  perm->shareability = desc->shareability;
  perm->access_flag = desc->access_flag;
  perm->non_global = desc->non_global;
  perm->hw_dirty_update = desc->hw_dirty_update;
  perm->contiguous = desc->contiguous; 
  perm->guarded = desc->guarded;
}
inline static void pt_add_perm_from_table_desc(struct pt_table_desc *desc, struct pt_perm *perm) 
{
  perm->non_secure |= desc->non_secure;
  perm->user |= desc->user;
  perm->readonly |= desc->readonly;
  perm->priv_exec_never |= desc->priv_exec_never;
  perm->unpriv_exec_never |= desc->unpriv_exec_never;
}

#define PT_TABLE_DESC_ADDR_SHIFT 12
#define PT_TABLE_DESC_ADDR_MASK  ((1ULL<<36)-1)
#define PT_BLOCK_DESC_ADDR_SHIFT 17
#define PT_BLOCK_DESC_ADDR_MASK  ((1ULL<<31)-1)
#define PT_PAGE_DESC_ADDR_SHIFT  12 
#define PT_PAGE_DESC_ADDR_MASK   ((1ULL<<36)-1)

#define PT_TABLE_DESC_PTR(table_desc) (union pt_desc*)(((uint64_t)(table_desc)->address_data) << PT_TABLE_DESC_ADDR_SHIFT)
#define PT_BLOCK_DESC_PTR(block_desc) (void *)(((uint64_t)(block_desc)->address_data) << PT_BLOCK_DESC_ADDR_SHIFT)
#define PT_PAGE_DESC_PTR(page_desc)   (void *)(((uint64_t)(page_desc)->address_data) << PT_PAGE_DESC_ADDR_SHIFT)

#define PT_TABLE_DESC_SET_PTR(table_desc, ptr) (table_desc)->address_data = ((((uint64_t)(void*)ptr)>>PT_TABLE_DESC_ADDR_SHIFT) & PT_TABLE_DESC_ADDR_MASK)
#define PT_BLOCK_DESC_SET_PTR(block_desc, ptr) (block_desc)->address_data = ((((uint64_t)(void*)ptr)>>PT_BLOCK_DESC_ADDR_SHIFT) & PT_BLOCK_DESC_ADDR_MASK)
#define PT_PAGE_DESC_SET_PTR(page_desc, ptr) (page_desc)->address_data = ((((uint64_t)(void*)ptr)>>PT_PAGE_DESC_ADDR_SHIFT) & PT_PAGE_DESC_ADDR_MASK)

#define ALIGNED_4KB(addr) ((((uint64_t)addr) & ((1<<12) - 1)) == 0)
#define ALIGNED_2MB(addr) ((((uint64_t)addr) & ((1<<21) - 1)) == 0)
#define ALIGNED_1GB(addr) ((((uint64_t)addr) & ((1<<30) - 1)) == 0)

// These are some common MAIR entry values
#define MAIR_ATTR_DEVICE_nGnRnE 0b00000000
#define MAIR_ATTR_NORMAL_NON_CACHEABLE 0b01000100
#define MAIR_ATTR_NORMAL_INNER_OUTER_WB_CACHEABLE 0b11111111

// MAIR indices to use
#define MAIR_UNDEFINED_INDEX 0
#define MAIR_NORMAL_INDEX 1
#define MAIR_DEVICE_INDEX 2

// Printing
static inline void pt_print_block_desc(struct page_table *table, long level, struct pt_block_desc *desc, struct pt_perm *perm, uint64_t vaddr, uint64_t size) {
  uint64_t paddr = (uint64_t)PT_BLOCK_DESC_PTR(desc);
  if(desc->valid && !desc->is_table) {
    PAGING_PRINT("VA[0x%016x - 0x%016x] -> PA[0x%016x - 0x%016x]%s%s%s%s%s%s%s%s%s%s%s (L%d BLOCK) (raw=0x%016x)\n",
      vaddr, vaddr + size, paddr, paddr + size,
      desc->mair_index == MAIR_DEVICE_INDEX ? " Device" : 
      desc->mair_index == MAIR_NORMAL_INDEX ? " Normal" :
      " [UNKNOWN MAIR INDEX]",
      table->flags & PAGE_TABLE_FLAG_SECURE ? (perm->non_secure ? "" : " SECURE") : "",
      perm->priv_exec_never ? " PXN" : "",
      perm->unpriv_exec_never ? " UXN" : "",
      perm->readonly ? " RD_ONLY" : "",
      perm->user ? " USER" : "",
      perm->contiguous ? " CONTIGUOUS" : "",
      perm->non_global ? " NON-GLOBAL" : "",
      perm->guarded ? " GUARDED" : "",
      desc->block_translation ? " nT" : "",
      desc->access_flag ? " ACCESS" : "",
      level,
      ((union pt_desc*)desc)->raw
      );
  }
  else {
    PAGING_PRINT("VA[0x%016x - 0x%016x] -> PA[0x%016x - 0x%016x] INVALID (L%d BLOCK) (raw=0x%016x)\n",
        vaddr, vaddr + size, paddr, paddr + size,
        level,
        ((union pt_desc*)desc)->raw
        );
  }
} 
static inline void pt_print_page_desc(struct page_table *table, long level, struct pt_page_desc *desc, struct pt_perm *perm, uint64_t vaddr, uint64_t size) {
  uint64_t paddr = (uint64_t)PT_PAGE_DESC_PTR(desc);
  if(desc->valid && desc->reserved) {
    PAGING_PRINT("VA[0x%016x - 0x%016x] -> PA[0x%016x - 0x%016x]%s%s%s%s%s%s%s%s%s%s (L%d PAGE) (raw=0x%016x)\n",
      vaddr, vaddr + size, paddr, paddr + size,
      desc->mair_index == MAIR_DEVICE_INDEX ? " Device" : 
      desc->mair_index == MAIR_NORMAL_INDEX ? " Normal" :
      " [UNKNOWN MAIR INDEX]",
      table->flags & PAGE_TABLE_FLAG_SECURE ? perm->non_secure ? "" : " SECURE" : "",
      perm->priv_exec_never ? " PXN" : "",
      perm->unpriv_exec_never ? " UXN" : "",
      perm->readonly ? " RD_ONLY" : "",
      perm->user ? " USER" : "",
      perm->contiguous ? " CONTIGUOUS" : "",
      perm->non_global ? " NON-GLOBAL" : "",
      perm->guarded ? " GUARDED" : "",
      desc->access_flag ? " ACCESS" : "",
      level,
      ((union pt_desc*)desc)->raw
      );
  }
  else {
    PAGING_PRINT("VA[0x%016x - 0x%016x] -> PA[0x%016x - 0x%016x] INVALID (L%d PAGE) (raw=0x%016x)\n",
        vaddr, vaddr + size, paddr, paddr + size,
        level,
        ((union pt_desc*)desc)->raw
        );
  } 
}
static inline void pt_print_table_desc(struct page_table *table, long level, struct pt_table_desc *desc, struct pt_perm *perm, uint64_t vaddr, uint64_t size) {
  if(desc->valid) {
    PAGING_PRINT("VA[0x%016x - 0x%016x]%s%s%s%s%s perm = %x (L%d TABLE) (raw=0x%016x)\n",
        vaddr, vaddr + size,
        table->flags & PAGE_TABLE_FLAG_SECURE ? perm->non_secure ? "" : " SECURE" : "",
        perm->priv_exec_never ? " PXN" : "",
        perm->unpriv_exec_never ? " UXN" : "",
        perm->readonly ? " RD_ONLY" : "",
        perm->user ? " USER" : "",
        *(uint32_t*)perm,
        level,
        ((union pt_desc*)desc)->raw
        );
  } else {
    PAGING_PRINT("VA[0x%016x - 0x%016x] -> INVALID (L%d TABLE) (raw=0x%016x)\n",
        vaddr, vaddr + size,
        level,
        ((union pt_desc*)desc)->raw
        );
  }
}

void pt_dump_page_table(struct page_table *table);

#define PT_RANGE_ROUND_NONE     0
#define PT_RANGE_ROUND_OUT      1
#define PT_RANGE_ROUND_IN       2
#define PT_RANGE_ROUND_FILL_END 3

// General Functions
struct page_table * pt_allocate_table(void);
void pt_free_table(struct page_table *table);
union pt_desc * pt_allocate_level(struct page_table *table, int level);
void pt_free_level(union pt_desc *level_ptr, struct page_table *table, int level);

int pt_block_to_table(union pt_desc *level_ptr, int level, struct page_table *table);
int pt_drill_range(struct page_table *table, uint64_t vaddr, uint64_t paddr, uint64_t size, struct pt_perm perm, int valid, int rounding);

// Granule Size Mutexed Functions
int pt_select_drill_func(struct page_table *table, uint64_t *start, uint64_t *end, union pt_desc*(**drill_func)(struct page_table*,void*), uint64_t *block_size, int *level, int rounding);

int pt_level_entry_size(struct page_table *table, int level);
int pt_level_alignment(struct page_table *table, int level);
int pt_level_num_entries(struct page_table *table, int level);

// Granule Specific Functions
struct page_table * pt_create_table_4kb(int tnsz);
//struct page_table * pt_create_table_16kb(int tnsz);
//struct page_table * pt_create_table_64kb(int tnsz);

int pt_init_table_invalid(struct page_table *);
int pt_init_table_device(struct page_table *, uint64_t vaddr, uint64_t paddr, uint64_t size);
int pt_init_table_normal(struct page_table *, uint64_t vaddr, uint64_t paddr, uint64_t size);
int pt_init_table_text(struct page_table *, uint64_t vaddr, uint64_t paddr, uint64_t size);
int pt_init_table_rodata(struct page_table *, uint64_t vaddr, uint64_t paddr, uint64_t size);

#endif
