/*
 * This file is part of the Nautilus AeroKernel developed
 * by the Hobbes and V3VEE Projects with funding from the
 * United States National  Science Foundation and the Department of Energy.
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  The Hobbes Project is a collaboration
 * led by Sandia National Laboratories that includes several national
 * laboratories and universities. You can find out more at:
 * http://www.v3vee.org  and
 * http://xtack.sandia.gov/hobbes
 *
 * Copyright (c) 2015, Kyle C. Hale <kh@u.northwestern.edu>
 * Copyright (c) 2015, The V3VEE Project  <http://www.v3vee.org>
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Authors: Kyle C. Hale <kh@u.northwestern.edu>
 *          Peter Dinda <pdinda@northwestern.edu> (Gem5 - e820)
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#include <nautilus/nautilus.h>
#include <nautilus/mm.h>
#include <nautilus/mb_utils.h>
#include <nautilus/macros.h>
#include <nautilus/multiboot2.h>
#include <nautilus/fdt/fdt.h>

extern char * mem_region_types[6];

#ifndef NAUT_CONFIG_DEBUG_BOOTMEM
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

#define BMM_DEBUG(fmt, args...) DEBUG_PRINT("BOOTMEM: " fmt, ##args)
#define BMM_PRINT(fmt, args...) INFO_PRINT("BOOTMEM: " fmt, ##args)
#define BMM_WARN(fmt, args...)  WARN_PRINT("BOOTMEM: " fmt, ##args)

extern ulong_t kernel_start;
extern ulong_t kernel_end;

void
fdt_reserve_boot_regions (unsigned long fdt)
{

  BMM_PRINT("Reserving kernel region (%p, size %lu)\n", &kernel_start, (&kernel_end - &kernel_start));
  mm_boot_reserve_mem(&kernel_start, (&kernel_end - &kernel_start));

  BMM_PRINT("Reserving FDT region (%p, size %lu)\n", fdt, fdt_totalsize(fdt));
  mm_boot_reserve_mem(fdt, fdt_totalsize(fdt));

  int offset = fdt_subnode_offset_namelen((void *)fdt, 0, "mmode_resv", 10);
  while(offset != -FDT_ERR_NOTFOUND) {
    fdt_reg_t reg = { .address = 0, .size = 0 };
    int getreg_result = fdt_getreg((void*)fdt, offset, &reg);

    if (getreg_result == 0) {
        BMM_PRINT("Reserving region (%p, size %lu)\n", reg.address, reg.size);
        mm_boot_reserve_mem(reg.address, reg.size);
    }
  }
}

static inline int insert_free_region_into_memory_map(mem_map_entry_t *memory_map, mmap_info_t *mm_info, uint64_t start, uint64_t end)
{

    if(mm_info->num_regions == MAX_MMAP_ENTRIES) {
      BMM_WARN("Found extra memory region within FDT, however the MMAP is full! Region: [%p - %p] sz:%llu\n", start, end, end-start);
      return -1;
    }

    memory_map[mm_info->num_regions].addr = start;
    memory_map[mm_info->num_regions].len  = end-start;
    memory_map[mm_info->num_regions].type = MULTIBOOT_MEMORY_AVAILABLE;
    
    BMM_PRINT("Memory map[%d] - [%p - %p] sz:%llu <%s>\n",
        mm_info->num_regions,
        start,
        end,
	end - start,
        mem_region_types[memory_map[mm_info->num_regions].type]);

    mm_info->usable_ram += end - start;

    if ((end >> PAGE_SHIFT) > mm_info->last_pfn) {
      mm_info->last_pfn = end >> PAGE_SHIFT;
    }

    mm_info->total_mem += end-start;

    mm_info->num_regions += 1;

    return 0;
}

void
fdt_detect_mem_map (mmap_info_t * mm_info,
                     mem_map_entry_t * memory_map,
                     ulong_t fdt)
{
  int offset = fdt_node_offset_by_prop_value(fdt, -1, "device_type", "memory", 7);

    BMM_DEBUG("offset = 0x%x\n", offset);
  while(offset != -FDT_ERR_NOTFOUND) {

    fdt_reg_t reg = { .address = 0, .size = 0 };
    int getreg_result = fdt_getreg(fdt, offset, &reg);

    if (getreg_result != 0) {
      BMM_WARN("Found \"%s\" node in DTB with missing REG property!\n", fdt_get_name(fdt, offset, NULL));
      goto err_continue;
    }

    unsigned long long start, end;

    start = round_up(reg.address, PAGE_SIZE_4KB);
    end   = round_down(reg.address + reg.size, PAGE_SIZE_4KB);

    insert_free_region_into_memory_map(memory_map, mm_info, start, end);

err_continue:
    offset = fdt_node_offset_by_prop_value(fdt, offset, "device_type", "memory", 7);
    BMM_DEBUG("offset = 0x%x\n", offset);
  }
}
