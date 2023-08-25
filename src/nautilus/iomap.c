
#include<nautilus/iomap.h>
#include<nautilus/nautilus.h>
#include<nautilus/rbtree.h>
#include<nautilus/mm.h>

#ifndef NAUT_CONFIG_DEBUG_IOMAP
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) 
#endif

#define ERROR(fmt, args...) ERROR_PRINT("iomap: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("iomap: " fmt, ##args)
#define INFO(fmt, args...) INFO_PRINT("iomap: " fmt, ##args)

#define NK_NUM_STATIC_IO_MAPPINGS 16
static struct nk_io_mapping __static_io_mappings[NK_NUM_STATIC_IO_MAPPINGS];

static struct rb_root iomap_rb_root = RB_ROOT;

// "alloc" fields
#define NK_IO_MAPPING_ALLOC_VALID        (1<<0)
#define NK_IO_MAPPING_ALLOC_STATIC       (1<<1)
#define NK_IO_MAPPING_ALLOC_BOOT         (1<<2)
#define NK_IO_MAPPING_ALLOC_KMEM         (1<<3)

void * nk_io_map(void *paddr, size_t size, int flags) 
{
  struct nk_io_mapping *mapping = NULL;

  // Regardless of the active allocator, try to use any free statically allocated mappings
  for(int i = 0; i < NK_NUM_STATIC_IO_MAPPINGS; i++) {
    if((__static_io_mappings[i].alloc & NK_IO_MAPPING_ALLOC_VALID) == 0) {
      mapping = &__static_io_mappings[i];
      break;
    }
  }

  if(mapping != NULL) {
    memset(mapping, 0, sizeof(struct nk_io_mapping));
    mapping->alloc |= NK_IO_MAPPING_ALLOC_VALID | NK_IO_MAPPING_ALLOC_STATIC;
  } else {
    switch(nk_current_allocator) {
      case NK_ALLOCATOR_BOOT:
        mapping = mm_boot_alloc(sizeof(struct nk_io_mapping));
        if(mapping == NULL) {
          ERROR("Failed to allocate nk_io_mapping using boot_mem!\n");
          return NULL;
        }
        memset(mapping, 0, sizeof(struct nk_io_mapping));
        mapping->alloc |= NK_IO_MAPPING_ALLOC_VALID | NK_IO_MAPPING_ALLOC_BOOT;
        break;
      case NK_ALLOCATOR_KMEM:
        mapping = malloc(sizeof(struct nk_io_mapping));
        if(mapping == NULL) {
          ERROR("Failed to allocate nk_io_mapping using kmem!\n");
          return NULL;
        }
        memset(mapping, 0, sizeof(struct nk_io_mapping));
        mapping->alloc |= NK_IO_MAPPING_ALLOC_VALID | NK_IO_MAPPING_ALLOC_BOOT;
        break;
      case NK_ALLOCATOR_NONE:
        ERROR("Ran out of statically allocated nk_io_mapping's before any allocator was initialized!\n");
        return NULL;
      default:
        ERROR("Ran out of statically allocated nk_io_mapping's with undefined allocator state!\n");
        return NULL;
    }
  }

  mapping->paddr = paddr;
  mapping->vaddr = paddr; // Still flat mapping
  mapping->size = size;
  mapping->flags = flags;

  DEBUG("before arch_handle_io_map(): paddr = %p, vaddr = %p, size = %p, flags = %x\n",
      mapping->paddr,
      mapping->vaddr,
      mapping->size,
      mapping->flags);
  if((flags & NK_IO_MAPPING_FLAG_ENABLED) == 0) {
    if(arch_handle_io_map(mapping)) {
      ERROR("arch_handle_io_map() failed!\n");
      return NULL;
    }
  }

  DEBUG("after arch_handle_io_map(): paddr = %p, vaddr = %p, size = %p, flags = %x\n",
      mapping->paddr,
      mapping->vaddr,
      mapping->size,
      mapping->flags);

  // Insert it into the rb_tree
  struct rb_node **new = &(iomap_rb_root.rb_node);
  struct rb_node *parent = NULL;

  while (*new) {
  	struct nk_io_mapping *this = container_of(*new, struct nk_io_mapping, node);
  
  	parent = *new;
  	if (mapping->paddr < this->paddr) {
  	  new = &((*new)->rb_left);
        }
  	else if (mapping->paddr > this->paddr) {
  	  new = &((*new)->rb_right);
        }
  	else {
          ERROR("Cannot insert two nk_io_mapping's with the exact same starting paddr! (paddr=%p)\n", mapping->paddr);
          return NULL;
        }
  }
  
  rb_link_node(&mapping->node, parent, new);
  nk_rb_insert_color(&mapping->node, &iomap_rb_root);  

  return mapping->vaddr;
}

int nk_io_unmap(void *paddr) 
{
  // TODO
  struct nk_io_mapping *mapping = NULL;
  return arch_handle_io_unmap(mapping);
}

struct nk_io_mapping * nk_io_map_first(void) 
{
  struct rb_node *node = nk_rb_first(&iomap_rb_root);
  if(node == NULL) {
    return NULL;
  }
  return container_of(node, struct nk_io_mapping, node);
}
struct nk_io_mapping * nk_io_map_next(struct nk_io_mapping *iter) 
{
  struct rb_node *node = &iter->node;
  node = nk_rb_next(node);
  if(node == NULL) {
    return NULL;
  }
  return container_of(node, struct nk_io_mapping, node);
}

void dump_io_map(void) 
{
  struct nk_io_mapping *iter = nk_io_map_first();
  INFO("--- I/O Mappings ---\n");
  while(iter != NULL) {
    INFO("\t[%p - %p] -> [%p - %p]\n",
        iter->vaddr, iter->vaddr + iter->size,
        iter->paddr, iter->paddr + iter->size);
    iter = nk_io_map_next(iter);
  }
}

