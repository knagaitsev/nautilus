
#include <nautilus/irqdesc.h>

#ifdef NAUT_CONFIG_DEBUG_INTERRUPT_FRAMEWORK
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

#define IRQ_PRINT(fmt, args...) printk("[IRQ] " fmt, ##args)
#define IRQ_DEBUG(fmt, args...) DEBUG_PRINT("[IRQ] " fmt, ##args)
#define IRQ_ERROR(fmt, args...) ERROR_PRINT("[IRQ] " fmt, ##args)
#define IRQ_WARN(fmt, args...) WARN_PRINT("[IRQ] " fmt, ##args)

#ifdef NAUT_CONFIG_SPARSE_IRQ

#include<nautilus/radix_tree.h>

static struct nk_radix_tree sparse_irq_radix_tree;

static inline struct nk_irq_desc * __irq_to_desc(nk_irq_t irq_num) 
{
  return (struct nk_irq_desc*)nk_radix_tree_get(&sparse_irq_radix_tree, (unsigned long)irq_num);
}

static inline void __irq_insert_desc(nk_irq_t irq_num, struct nk_irq_desc *desc) 
{
  nk_radix_tree_insert(&sparse_irq_radix_tree, irq_num, (void*)desc);
}

#else /* NAUT_CONFIG_SPARSE_IRQ */

#ifndef MAX_IRQ_NUM
#error "MAX_IRQ_NUM is undefined!"
#endif

static struct nk_irq_desc * descriptors[MAX_IRQ_NUM];

static inline struct nk_irq_desc * __irq_to_desc(nk_irq_t irq) 
{
  struct nk_irq_desc *desc = descriptors + irq;
  if(desc->irq != irq) {
    panic("desc->irq (%u) != irq (%u)\n", desc->irq, irq);
    return NULL;
  }
  if(desc->flags & NK_IRQ_DESC_FLAG_ALLOCATED) {
    return desc;
  }
  return NULL; 
}

static inline void __insert_irq_desc(nk_irq_t irq, struct nk_irq_desc *desc) 
{
  descriptors[irq] = desc; 
}

#endif /* NAUT_CONFIG_SPARSE_INTERRUPT_IVECS */

static inline struct nk_irq_desc * __alloc_irq_desc(nk_irq_t irq) {
  struct nk_irq_desc *desc = malloc(sizeof(struct nk_irq_desc*));
  if(desc == NULL) {
    return desc;
  }
  memset(desc, 0, sizeof(struct nk_irq_desc*));

  desc->irq = irq;

  return desc;
}

struct nk_irq_desc * nk_irq_to_desc(nk_irq_t irq) 
{
  return __irq_to_desc(irq);
}

nk_irq_t nk_alloc_irq_descs(int num, nk_irq_t search_start) 
{
  // Find a range of unallocated irq_descriptors
  // (This is not efficient at all but should only happen a few times at boot)
  int null_count = 0;
  nk_irq_t base = NK_NULL_IRQ;
  for(nk_irq_t i = search_start; i < MAX_IRQ_NUM; i++) {
    if(nk_irq_to_desc(i) == NULL) {
      null_count += 1;
    }
    if(null_count == num) {
      base = (i - (num-1));
    }
  }

  if(base == NK_NULL_IRQ) {
    return base;
  }

  for(int i = 0; i < num; i++) {
    struct nk_irq_desc *desc = __alloc_irq_desc(base+i);

    if(desc == NULL) {
      for(int f = i-1; f >= 0; f--) {
        nk_free_irq_desc(base + f);
      }
      return NK_NULL_IRQ;
    }

    desc->irq_dev = dev;
    desc->type = type;

    desc->flags = flags;
    spinlock_init(&(desc->lock));
  }

  num_irqs_allocated += num;
  return base;
}

void nk_free_irq_descs(nk_irq_t base, int num) {
  for(int i = 0; i < num; i++) {
    struct nk_irq_desc *desc = nk_irq_to_desc

    __free_irq_desc(desc);
  }
}

