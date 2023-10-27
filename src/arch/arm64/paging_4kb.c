
#include<arch/arm64/paging.h>
#include<nautilus/nautilus.h>
#include<nautilus/naut_types.h>
#include<nautilus/macros.h>

// 4KB Granule Size
#define VADDR_L0_4KB(i0) (i0 << 39)
#define VADDR_L1_4KB(i0, i1) (i0 << 39 | i1 << 30)
#define VADDR_L2_4KB(i0, i1, i2) (i0 << 39 | i1 << 30 | i2 << 21)
#define VADDR_L3_4KB(i0, i1, i2, i3) (i0 << 39 | i1 << 30 | i2 << 21 | i3 << 12)

#define L0_INDEX_4KB(addr) ((((uint64_t)addr) >> 39) & 0x1FF)
#define L1_INDEX_4KB(addr) ((((uint64_t)addr) >> 30) & 0x1FF)
#define L2_INDEX_4KB(addr) ((((uint64_t)addr) >> 21) & 0x1FF)
#define L3_INDEX_4KB(addr) ((((uint64_t)addr) >> 12) & 0x1FF)
#define PAGE_OFFSET_4KB(addr) ((uint64_t)addr) & ((1ULL<<12) - 1)
#define PAGE_OFFSET_2MB(addr) ((uint64_t)addr) & ((1ULL<<21) - 1)
#define PAGE_OFFSET_1GB(addr) ((uint64_t)addr) & ((1ULL<<30) - 1)
#define L0_ENTRY_SIZE_4KB (1ULL<<39) //512 GB
#define L1_ENTRY_SIZE_4KB (1ULL<<30) //1 GB
#define L2_ENTRY_SIZE_4KB (1ULL<<21) //2 MB
#define L3_ENTRY_SIZE_4KB (1ULL<<12) //4 KB

int pt_level_num_entries_4kb(struct page_table *table, int level) 
{
  if(table->root_level < level) {
    return 512;
  } else if(table->root_level > level) {
    return 0;
  } else {
    int bits = 64 - table->tnsz;
    bits -= 12; // page offset
    for(int i = level; i < 3; i++) {
      bits -= 9;
    }
    if(bits > 9 || bits <= 0) {
      PAGING_ERROR("Mismatch between tnsz and root_level in page_table! tnsz = %u, root_level = %u\n", table->tnsz, table->root_level);
      return -1;
    }
    return (1<<bits);
  }
}

int pt_level_alignment_4kb(struct page_table *table, int level) 
{
  int align = sizeof(union pt_desc) * 512;

  if(level == table->root_level) 
  {
    int count = pt_level_num_entries(table, level);
    align = sizeof(union pt_desc) * count;
    align = align > 64 ? align : 64;
  }

  return align;
}

uint64_t pt_level_entry_size_4kb(struct page_table *table, int level) 
{
  switch(level) {
    case 0:
      return L0_ENTRY_SIZE_4KB;
    case 1:
      return L1_ENTRY_SIZE_4KB;
    case 2:
      return L2_ENTRY_SIZE_4KB;
    case 3:
      return L3_ENTRY_SIZE_4KB;
    default:
      return 0;
  }
}

// Drill Page Functions
static union pt_desc *drill_to_level_4kb(struct page_table *table, void *vaddr, long level)
{
  //PAGING_DEBUG("drill_to_level_4kb(table=%p, vaddr=%p, level=%d)\n",table,vaddr,level);

  if(table->root_ptr == NULL) {
    table->root_ptr = pt_allocate_level(table, table->root_level);
    if(table->root_ptr == NULL) {
      PAGING_ERROR("Could not allocate root level for 4kb page table!\n");
      return NULL;
    }
  }

  union pt_desc *desc = table->root_ptr;
  switch(table->root_level) {
    case 0: 
      desc += L0_INDEX_4KB(vaddr);
      if(level == 0) {
        break;
      }
      if(!(desc->table.valid && desc->table.is_table)) {
        //PAGING_DEBUG("\tneed to drill 1GB pages first\n");
        pt_block_to_table(desc, 0, table);
        //PAGING_DEBUG("\tdrilled 1GB pages\n");
      }
      desc = PT_TABLE_DESC_PTR(&desc->table);

    case 1:
      desc += L1_INDEX_4KB(vaddr);
      if(level == 1) {
        break;
      }
      if(!(desc->table.valid && desc->table.is_table)) {
        //PAGING_DEBUG("\tneed to drill 2MB pages first\n");
        pt_block_to_table(desc, 1, table);
        //PAGING_DEBUG("\tdrilled 2MB pages\n");
      }
      desc = PT_TABLE_DESC_PTR(&desc->table);

    case 2:
      desc += L2_INDEX_4KB(vaddr);
      if(level == 2) {
        break;
      }
      if(!(desc->table.valid && desc->table.is_table)) {
        //PAGING_DEBUG("\tneed to drill 4KB pages\n");
        pt_block_to_table(desc, 2, table);
        //PAGING_DEBUG("\tdrilled 4KB pages\n");
      } else {
        //PAGING_DEBUG("\tno drilling needed\n");
      }
      desc = PT_TABLE_DESC_PTR(&desc->table);

    case 3:
      desc += L3_INDEX_4KB(vaddr);
      if(level == 3) {
        break;
      } else {
        PAGING_ERROR("Invalid level to drill 4kb granule page table to: level = %u\n", level);
        return NULL;
      }
    default:
      PAGING_ERROR("Cannot drill level on table with root_level = %d\n",table->root_level);
      return NULL;
  }

  return desc;
}

static union pt_desc *drill_1gb_block_4kb(struct page_table *table, void *vaddr) 
{
  return drill_to_level_4kb(table, vaddr, 1);
}
static union pt_desc *drill_2mb_block_4kb(struct page_table *table, void *vaddr) 
{
  return drill_to_level_4kb(table, vaddr, 2);
}
static union pt_desc *drill_4kb_page_4kb(struct page_table *table, void *vaddr) 
{
  return drill_to_level_4kb(table, vaddr, 3);
}

int pt_select_drill_func_4kb(struct page_table *table, uint64_t *start, uint64_t *end, union pt_desc*(**drill_func)(struct page_table*,void*), uint64_t *block_size, int *level, int rounding) 
{
    int ill = table->root_level;

    if((ill <= 1) && ALIGNED_1GB(*start))
    {
      if(rounding == PT_RANGE_ROUND_FILL_END) {
        if(round_down(*end, PAGE_SIZE_1GB) != *start) {
          *end = round_down(*end, PAGE_SIZE_1GB);
        }
      } 
      
      if(ALIGNED_1GB(*end)) {
        //PAGING_DEBUG("selected page 1GB page size for drill\n");
        *block_size = PAGE_SIZE_1GB;
        *drill_func = drill_1gb_block_4kb;
        *level = 1;
        return 0;
      }

    }

    if((ill <= 2) && ALIGNED_2MB(*start)) 
    {
      if(rounding == PT_RANGE_ROUND_FILL_END) {
        if(round_down(*end, PAGE_SIZE_2MB) != *start) {
          *end = round_down(*end, PAGE_SIZE_2MB);
        }
      }

      if(ALIGNED_2MB(*end)) {
        //PAGING_DEBUG("selected page 2MB page size for drill\n");
        *block_size = PAGE_SIZE_2MB;
        *drill_func = drill_2mb_block_4kb;
        *level = 2;
        return 0;
      }

    }

    if(ill <= 3) {

      switch(rounding) {
        case PT_RANGE_ROUND_IN:
          //PAGING_DEBUG("selected 4KB page size for drill (rounding in)\n");
          //PAGING_DEBUG("old_start = %p, old_end = %p\n", (void*)start, (void*)end);
          *end = round_down(*end, PAGE_SIZE_4KB);
          *start = round_up(*start, PAGE_SIZE_4KB);
          //PAGING_DEBUG("new_start = %p, new_end = %p\n", (void*)start, (void*)end);
          break;
        case PT_RANGE_ROUND_OUT:
          //PAGING_DEBUG("selected 4KB page size for drill (rounding out)\n");
          //PAGING_DEBUG("old_start = %p, old_end = %p\n", (void*)start, (void*)end);
          *end = round_up(*end, PAGE_SIZE_4KB);
          *start = round_down(*start, PAGE_SIZE_4KB);
          //PAGING_DEBUG("new_start = %p, new_end = %p\n", (void*)start, (void*)end);
          break;
        case PT_RANGE_ROUND_FILL_END:
          //PAGING_DEBUG("selected 4KB page size for drill (rounding fill end)\n");
          *end = round_down(*end, PAGE_SIZE_4KB);
          if(!ALIGNED_4KB(*start)) {
            PAGING_ERROR("Could not select drill function without rounding start = %p\n", (void*)*start);
            return -1;
          } 
          break;
        case PT_RANGE_ROUND_NONE:
          //PAGING_DEBUG("selected 4KB page size for drill (no rounding)\n");
          if(!ALIGNED_4KB(*start) || !ALIGNED_4KB(*end)) {
            PAGING_ERROR("Could not select drill function without rounding for start = %p, end = %p, with 4KB page tables\n", (void*)*start, (void*)*end);
            return -1;
          }
          //PAGING_DEBUG("start = %p, end = %p\n", (void*)start, (void*)end);
          break;
      }

      *block_size = PAGE_SIZE_4KB;
      *drill_func = drill_4kb_page_4kb;
      *level = 3;
      return 0;
    }

    // If we haven't returned yet we failed
    return -1;
}

// Table Creation

// Printing
static void dump_l3_page_table_4kb(struct page_table *table, struct pt_perm *hier, union pt_desc *l3_table, uint64_t i0, uint64_t i1, uint64_t i2) 
{
  for(uint64_t i3 = 0; i3 < pt_level_num_entries(table, 3); i3++) 
  {
      struct pt_perm curr_perm = {};
      struct pt_page_desc *page_desc = &l3_table[i3].page;
      uint64_t vaddr_start = VADDR_L3_4KB(i0, i1, i2, i3); 

      if(table->flags & PAGE_TABLE_FLAG_HIERARCHY_ENABLED) {
        curr_perm = *hier;
      }
      pt_add_perm_from_page_desc(page_desc, &curr_perm);
      pt_print_page_desc(table, 3, page_desc, &curr_perm, vaddr_start, PAGE_SIZE_4KB);
  } 
}

static void dump_l2_page_table_4kb(struct page_table *table, struct pt_perm *hier, union pt_desc *l2_table, uint64_t i0, uint64_t i1) 
{
  for(uint64_t i2 = 0; i2 < pt_level_num_entries(table, 2); i2++) {

    struct pt_perm curr_perm = {};
    uint64_t vaddr_start = VADDR_L2_4KB(i0, i1, i2);

    if(table->flags & PAGE_TABLE_FLAG_HIERARCHY_ENABLED) {
      curr_perm = *hier;
    }

    if(l2_table[i2].table.is_table) 
    { 
      struct pt_table_desc *table_desc = &l2_table[i2].table;
      pt_add_perm_from_table_desc(table_desc, &curr_perm);
      union pt_desc *l3_table = (union pt_desc*)PT_TABLE_DESC_PTR(table_desc);
      if(l2_table[i2].valid) {
        pt_print_table_desc(table, 2, table_desc, &curr_perm, vaddr_start, PAGE_SIZE_2MB);
        dump_l3_page_table_4kb(table, &curr_perm, l3_table, i0, i1, i2);
      } else {
        pt_print_table_desc(table, 2, table_desc, &curr_perm, vaddr_start, PAGE_SIZE_2MB);
      }
    } else { 
      struct pt_block_desc *block_desc = &l2_table[i2].block; 
      pt_add_perm_from_block_desc(block_desc, &curr_perm);
      pt_print_block_desc(table, 2, block_desc, &curr_perm, vaddr_start, PAGE_SIZE_2MB);
    }
  }
}

static void dump_l1_page_table_4kb(struct page_table *table, struct pt_perm *hier, union pt_desc *l1_table, uint64_t i0) 
{
  for(uint64_t i1 = 0; i1 < pt_level_num_entries(table, 1); i1++) {

    struct pt_perm curr_perm = {};
    uint64_t vaddr_start = VADDR_L1_4KB(i0, i1);

    if(table->flags & PAGE_TABLE_FLAG_HIERARCHY_ENABLED) {
      curr_perm = *hier;
    }

    if(l1_table[i1].table.is_table) 
    {
      struct pt_table_desc *table_desc = &l1_table[i1].table;
      pt_add_perm_from_table_desc(table_desc, &curr_perm);
      union pt_desc *l2_table = (union pt_desc*)PT_TABLE_DESC_PTR(table_desc);
      if(l1_table[i1].valid) {
        pt_print_table_desc(table, 1, table_desc, &curr_perm, vaddr_start, PAGE_SIZE_1GB);
        dump_l2_page_table_4kb(table, &curr_perm, l2_table, i0, i1);
      } else {
        pt_print_table_desc(table, 1, table_desc, &curr_perm, vaddr_start, PAGE_SIZE_1GB);
      }
    } else { 
      struct pt_block_desc *block_desc = &l1_table[i1].block; 
      pt_add_perm_from_block_desc(block_desc, &curr_perm);
      pt_print_block_desc(table, 1, block_desc, &curr_perm, vaddr_start, PAGE_SIZE_1GB);
    }
  }
}

static void dump_l0_page_table_4kb(struct page_table *table, struct pt_perm *hier, union pt_desc *l0_table) 
{
  for(uint64_t i0 = 0; i0 < pt_level_num_entries(table, 0); i0++) {

    struct pt_perm curr_perm = {};
    uint64_t vaddr_start = VADDR_L0_4KB(i0);

    if(table->flags & PAGE_TABLE_FLAG_HIERARCHY_ENABLED) {
      curr_perm = *hier;
    }

    if(l0_table[i0].table.is_table) 
    {
      struct pt_table_desc *table_desc = &l0_table[i0].table;
      pt_add_perm_from_table_desc(table_desc, &curr_perm);
      union pt_desc *l1_table = (union pt_desc*)PT_TABLE_DESC_PTR(table_desc);
      if(l0_table[i0].valid) {
        pt_print_table_desc(table, 0, table_desc, &curr_perm, vaddr_start, PAGE_SIZE_1GB*512);
        dump_l1_page_table_4kb(table, &curr_perm, l1_table, i0);
      } else {
        pt_print_table_desc(table, 0, table_desc, &curr_perm, vaddr_start, PAGE_SIZE_1GB*512);
      }
    } else { 
      struct pt_block_desc *block_desc = &l0_table[i0].block; 
      pt_add_perm_from_block_desc(block_desc, &curr_perm);
      struct pt_block_desc copy_desc = *block_desc;
      copy_desc.valid = 0;
      pt_print_block_desc(table, 0, &copy_desc, &curr_perm, vaddr_start, PAGE_SIZE_1GB*512);
    }
  }
}

void pt_dump_page_table_4kb(struct page_table *table) 
{
  union pt_desc *desc = table->root_ptr;
  struct pt_perm open_perm = {};

  switch(table->root_level) 
  {
    case 0:
      dump_l0_page_table_4kb(table, &open_perm, desc);
      break;
    case 1:
      dump_l1_page_table_4kb(table, &open_perm, desc, 0);
      break;
    case 2:
      dump_l2_page_table_4kb(table, &open_perm, desc, 0, 0);
      break;
    case 3:
      dump_l3_page_table_4kb(table, &open_perm, desc, 0, 0, 0);
      break;
    default:
      PAGING_ERROR("Cannot Print Contents of page_table with root_level = %d\n", table->root_level);
      break;
  }
  return;
}

// Init

struct page_table * pt_create_table_4kb(int tnsz) 
{
  struct page_table *table = pt_allocate_table();
  if(table == NULL) {
    goto err_exit_nalloc;
  }

  table->granule_size = PAGE_TABLE_GRANULE_SIZE_4KB;
  table->tnsz = tnsz;
  table->page_level = 3;

  if(tnsz < 12) {
    // This is not a valid value
    PAGING_ERROR("Cannot create 4kb page table with tnsz < 12 (tnsz = %u)\n", tnsz);
    goto err_exit;
  } else if(tnsz <= 15) {
    table->root_level = -1;
  } else if(tnsz <= 24) {
    table->root_level = 0;
  } else if(tnsz <= 33) {
    table->root_level = 1;
  } else if(tnsz <= 42) {
    table->root_level = 2;
  } else if(tnsz <= 48) {
    table->root_level = 3;
  } else {
    PAGING_ERROR("Cannot create 4kb page table with tnsz > 48 (tnsz = %u)\n", tnsz);
    goto err_exit;
  }

  //PAGING_DEBUG("Created 4KB page table with root_level = %u, tnsz = %u\n", table->root_level, table->tnsz);
  return table;

err_exit:
  pt_free_table(table);
err_exit_nalloc:
  return NULL;
}

