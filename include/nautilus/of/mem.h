#ifndef __FDT_MEM_H__
#define __FDT_MEM_H__

int fdt_detect_mem_map(mmap_info_t *mm_info, mem_map_entry_t *memory_map, unsigned long mbd);
int fdt_reserve_boot_regions(unsigned long mbd);

#endif
