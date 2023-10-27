
#include<nautilus/iomap.h>
#include<nautilus/arch.h>
#include<nautilus/atomic.h>

#include<arch/arm64/paging.h>

int arch_handle_io_map(struct nk_io_mapping *mapping) 
{
  if(mapping->flags & NK_IO_MAPPING_FLAG_ENABLED) {
    return 0;
  }

  if(ttbr0_table == NULL) {
    return 0;
  } else {
    int flags = atomic_or(mapping->flags, NK_IO_MAPPING_FLAG_ENABLED);
    return pt_init_table_device(ttbr0_table, mapping->vaddr, mapping->paddr, mapping->size);
  }
}
int arch_handle_io_unmap(struct nk_io_mapping *mapping) 
{
  // TODO
  return -1;
}
