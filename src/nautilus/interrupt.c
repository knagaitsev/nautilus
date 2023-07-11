
#include<nautilus/interrupt.h>

#ifndef NAUT_CONFIG_DEBUG_PRINTS
#undef DEBUG_PRINT
#define DEBUG_PRINT(...)
#endif

#ifdef NAUT_CONFIG_SPARSE_IRQ_VECTORS

#define SPARSE_IVEC_BITS (sizeof(nk_ivec_t) * 8)
#define SPARSE_IVEC_NUM_LEVELS 2
#define SPARSE_IVEC_BITS_PER_LEVEL (SPARSE_IVEC_BITS / SPARSE_IVEC_NUM_LEVELS)
#define SPARSE_IVEC_NUM_ENTRIES_PER_LEVEL (1<<SPARSE_IVEC_BITS_PER_LEVEL)
#define SPARSE_IVEC_INDEX_MASK (SPARSE_IVEC_NUM_ENTRIES_PER_LEVEL-1)

_Static_assert(((SPARSE_IVEC_BITS_PER_LEVEL*SPARSE_IVEC_NUM_LEVELS) == SPARSE_IVEC_BITS), 
    "Sparse IVEC: IVEC_BITS is not divisible by the number of levels in the radix tree!");

static void *sparse_ivec_radix_tree_base = NULL;

int nk_alloc_ivec_desc(nk_ivec_t ivec_num, struct nk_irq_dev *dev, uint16_t type, uint16_t flags) 
{
  void **radix_tree = &sparse_ivec_radix_tree_base;

  for(int i = SPARSE_IVEC_NUM_LEVELS-1; i >= 0; i--) {

    if(*radix_tree == NULL) {

      *radix_tree = malloc(sizeof(void*) * SPARSE_IVEC_NUM_ENTRIES_PER_LEVEL);
      if(*radix_tree == NULL)
      {
        return -1;
      }
      memset(*radix_tree, 0, sizeof(void*) * SPARSE_IVEC_NUM_ENTRIES_PER_LEVEL);
    }

    nk_ivec_t index = ((ivec_num >> (SPARSE_IVEC_BITS_PER_LEVEL * i)) & SPARSE_IVEC_INDEX_MASK);

    radix_tree = ((void**)*radix_tree) + index;

  }

  if(*radix_tree == NULL) {
    *radix_tree = malloc(sizeof(struct nk_ivec_desc));
    memset(*radix_tree, 0, sizeof(struct nk_ivec_desc));
  }

  if(*radix_tree == NULL) 
  {
    return -1;
  }

  struct nk_ivec_desc *desc = (struct nk_ivec_desc*)(*radix_tree);

  desc->irq_dev = dev;
  desc->ivec_num = ivec_num;
  desc->type = type;
  desc->flags = flags;
  spinlock_init(&(desc->lock));

  return 0;

}

int nk_alloc_ivec_descs(nk_ivec_t base, uint32_t n, struct nk_irq_dev *dev, uint16_t type, uint16_t flags) 
{
  DEBUG_PRINT("Allocating Interrupt Vectors: [%u - %u]\n", base, base+n-1); 
  // This is pretty inefficient for now but it should be okay
  for(uint32_t i = 0; i < n; i++) {

    if(nk_alloc_ivec_desc(base + i, dev, type, flags)) 
    {
      return -1;
    }

  }

  return 0;
}

struct nk_ivec_desc * nk_ivec_to_desc(nk_ivec_t ivec_num) 
{
  void **radix_tree = &sparse_ivec_radix_tree_base;

  for(int i = SPARSE_IVEC_NUM_LEVELS-1; i >= 0; i--) {

    if(*radix_tree == NULL) {
      return (struct nk_ivec_desc*)*radix_tree;
    }

    nk_ivec_t index = ((ivec_num >> (SPARSE_IVEC_BITS_PER_LEVEL * i)) & SPARSE_IVEC_INDEX_MASK);

    radix_tree = ((void**)*radix_tree) + index;

  }

  return (struct nk_ivec_desc*)(*radix_tree);
}

#else /* NAUT_CONFIG_SPARSE_IRQ_VECTORS */

// This is x86 specific TODO (Fix this)
#define MAX_IVEC_NUM 256

static struct nk_ivec_desc descriptors[MAX_IVEC_NUM];

int nk_alloc_ivec_desc(nk_ivec_t num, struct nk_irq_dev *dev, uint16_t type, uint16_t flags) 
{
  descriptors[num].ivec_num = num;
  descriptors[num].flags = flags;
  descriptors[num].type = type;
  descriptors[num].irq_dev = dev;
  spinlock_init(&(descriptors[num].lock));
}

int nk_alloc_ivec_descs(nk_ivec_t base, uint32_t n, struct nk_irq_dev *dev, uint16_t type, uint16_t flags)
{
  // Inefficient but should work
  for(uint32_t i = 0; i < n; i++) 
  {

    if(nk_alloc_ivec_desc(base + i, dev, type, flags)) 
    {
      return -1;  
    }

  }

  return 0;
}

struct nk_ivec_desc * nk_ivec_to_desc(nk_ivec_t ivec) 
{
  return descriptors + ivec;
}

#endif /* NAUT_CONFIG_SPARSE_INTERRUPT_IVECS */


uint16_t nk_ivec_set_flags(nk_ivec_t ivec, uint16_t flags) 
{
  struct nk_ivec_desc *desc = nk_ivec_to_desc(ivec);
  uint8_t old = __sync_fetch_and_or(&desc->flags, flags);
  return old & flags;
}
uint16_t nk_ivec_clear_flags(nk_ivec_t ivec, uint16_t flags)
{
  struct nk_ivec_desc *desc = nk_ivec_to_desc(ivec);
  uint8_t old = __sync_fetch_and_and(&desc->flags, ~flags);
  return old & flags;
}
uint16_t nk_ivec_check_flags(nk_ivec_t ivec, uint16_t flags)
{
  struct nk_ivec_desc *desc = nk_ivec_to_desc(ivec);
  return __sync_fetch_and_add(&desc->flags, 0) & flags; 
}

int nk_ivec_add_callback(nk_ivec_t vec, nk_irq_callback_t callback, void *state)
{
  struct nk_ivec_desc *desc = nk_ivec_to_desc(vec);
  if(desc == NULL) {
    return -1;
  }

  spin_lock(&desc->lock);

  struct nk_irq_action *action = malloc(sizeof(struct nk_irq_action));
  if(action == NULL) {
    return -1;
  }
  memset(action, 0, sizeof(struct nk_irq_action));

  action->callback = callback;
  action->callback_state = state;
  action->type = NK_IRQ_ACTION_TYPE_CALLBACK;
  action->ivec_desc = desc;
  action->flags |= NK_IRQ_ACTION_FLAG_NO_IRQ;
  action->ivec = vec;

  action->next = desc->actions;
  desc->actions = action;
  desc->num_actions += 1;

  spin_unlock(&desc->lock);

  return 0;
}

/*
 * IRQ Action Functions
 */

int nk_handle_irq_actions(struct nk_irq_action *actions, struct nk_regs *regs) 
{ 
  int ret = 0;
  while(actions != NULL) {
    
    switch(actions->type) 
    {
      case NK_IRQ_ACTION_TYPE_CALLBACK:
        ret |= actions->callback(actions, regs, actions->callback_state);
      default:
        ret |= 1;
    }

    actions = actions->next;
  }
  return ret;
}

/*
 * IRQ Functions
 */

#ifdef NAUT_CONFIG_IDENTITY_MAP_IRQ_VECTORS

inline int nk_map_irq_to_ivec(nk_irq_t irq, nk_ivec_t ivec) 
{ 
  return irq != ivec;
}
inline nk_ivec_t nk_irq_to_ivec(nk_irq_t irq) 
{ 
  return (nk_ivec_t)irq; 
}
inline struct nk_ivec_desc * nk_irq_to_desc(nk_irq_t irq) 
{ 
  return nk_ivec_to_desc((nk_ivec_t)irq); 
}

inline int nk_map_irq_to_irqdev(nk_irq_t irq, struct nk_irq_dev *dev) 
{
  nk_irq_to_desc(irq)->irq_dev = dev;
  return 0;
}
inline struct nk_irq_dev * nk_irq_to_irqdev(nk_irq_t irq) 
{
  return nk_ivec_to_desc((nk_ivec_t)irq).irq_dev;
}

#else

#ifdef NAUT_CONIFG_SPARSE_IRQ

// We have no architectures which need this case yet
// so I'm just going to leave it empty for now, 
// potentially this would just be another radix tree
int nk_map_irq_to_ivec(nk_irq_t irq, nk_ivec_t ivec) 
{
  // TODO
}
nk_ivec_t nk_irq_to_ivec(nk_irq_t irq) 
{
  // TODO
}
struct nk_ivec_desc * nk_irq_to_desc(nk_irq_t irq) 
{
  // TODO
}

int nk_map_irq_to_irqdev(nk_irq_t irq, struct nk_irq_dev *dev) 
{
  // TODO
}
struct nk_irq_dev * nk_irq_to_irqdev(nk_irq_t irq) 
{
  // TODO
}

#else

static nk_ivec_t irq_to_ivec_array[MAX_IRQ_NUM];
static struct nk_irq_dev * irq_to_irqdev_array[MAX_IRQ_NUM];

int nk_map_irq_to_ivec(nk_irq_t irq, nk_ivec_t ivec) 
{
  irq_to_ivec_array[irq] = ivec;
  return 0;
}
nk_ivec_t nk_irq_to_ivec(nk_irq_t irq) 
{
  return irq_to_ivec_array[irq];
}
struct nk_ivec_desc * nk_irq_to_desc(nk_irq_t irq) 
{
  return nk_ivec_to_desc(irq_to_ivec_array[irq]);
}

int nk_map_irq_to_irqdev(nk_irq_t irq, struct nk_irq_dev *dev) 
{
  irq_to_irqdev_array[irq] = dev;
  return 0;
}
struct nk_irq_dev * nk_irq_to_irqdev(nk_irq_t irq) 
{
  return irq_to_irqdev_array[irq];
}

#endif /* NAUT_CONFIG_SPARSE_IRQ */

#endif /* !NAUT_CONFIG_IDENTITY_MAP_IRQ_VECTORS */

int nk_irq_is_assigned_to_irqdev(nk_irq_t irq) 
{
  return nk_irq_to_irqdev(irq) != NULL;
}

int nk_irq_add_callback(nk_irq_t irq, nk_irq_callback_t callback, void *state)
{
  nk_ivec_t vec = nk_irq_to_ivec(irq);
  struct nk_ivec_desc *desc = nk_ivec_to_desc(vec);
  if(desc == NULL) {
    return -1;
  }

  spin_lock(&desc->lock);

  struct nk_irq_action *action = malloc(sizeof(struct nk_irq_action));
  if(action == NULL) {
    return -1;
  }
  memset(action, 0, sizeof(struct nk_irq_action));

  action->callback = callback;
  action->callback_state = state;
  action->type = NK_IRQ_ACTION_TYPE_CALLBACK;
  action->ivec_desc = desc;
  action->irq = irq;
  action->ivec = vec;

  action->next = desc->actions;
  desc->actions = action;
  desc->num_actions += 1;

  spin_unlock(&desc->lock);

  return 0;
}

int nk_mask_irq(nk_irq_t irq) {
  struct nk_irq_dev *dev = nk_irq_to_irqdev(irq);
  if(dev == NULL) {
    return 1;
  }
  return nk_irq_dev_disable_irq(dev, irq);
}

int nk_unmask_irq(nk_irq_t irq) {
  struct nk_irq_dev *dev = nk_irq_to_irqdev(irq);
  if(dev == NULL) {
    return 1;
  }
  return nk_irq_dev_enable_irq(dev, irq);
}

