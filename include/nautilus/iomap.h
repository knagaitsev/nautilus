#ifndef __IOMAP_H__
#define __IOMAP_H__

#include <nautilus/naut_types.h>
#include <nautilus/rbtree.h>

#define NK_IO_MAPPING_FLAG_ENABLED (1<<0)

struct nk_io_mapping {
  void *paddr;
  void *vaddr;
  size_t size;

  struct rb_node node;

  int flags;
  int alloc;
};

void * nk_io_map(void *paddr, size_t size, int flags);
int nk_io_unmap(void *paddr);

struct nk_io_mapping *nk_io_map_first(void);
struct nk_io_mapping *nk_io_map_next(struct nk_io_mapping *iter);

void dump_io_map(void);

// KJH - Theoretically these functions could need to be 
// architecture dependant, so use these to access
// mmio regions instead of direct access where possible

static inline uint8_t readb(uint64_t addr) {
  return *(volatile uint8_t*)addr;
}
static inline uint16_t readw(uint64_t addr) {
  return *(volatile uint16_t*)addr;
}
static inline uint32_t readl(uint64_t addr) {
  return *(volatile uint32_t*)addr;
}
static inline uint64_t readq(uint64_t addr) {
  return *(volatile uint64_t*)addr;
}

static inline void writeb(uint8_t val, uint64_t addr) {
  *(volatile uint8_t*)addr = val;
}
static inline void writew(uint16_t val, uint64_t addr) {
  *(volatile uint16_t*)addr = val;
}
static inline void writel(uint32_t val, uint64_t addr) {
  *(volatile uint32_t*)addr = val;
}
static inline void writeq(uint64_t val, uint64_t addr) {
  *(volatile uint64_t*)addr = val;
}

#endif
