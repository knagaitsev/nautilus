
#include<nautilus/interrupt.h>

// Gives us MAX_IRQ_NUM and MAX_IVEC_NUM
#include<nautilus/arch.h>
#include<nautilus/shell.h>
#include<nautilus/naut_assert.h>

#ifdef NAUT_CONFIG_DEBUG_INTERRUPT_FRAMEWORK
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

#define IRQ_PRINT(fmt, args...) printk("[IRQ] " fmt, ##args)
#define IRQ_DEBUG(fmt, args...) DEBUG_PRINT("[IRQ] " fmt, ##args)
#define IRQ_ERROR(fmt, args...) ERROR_PRINT("[IRQ] " fmt, ##args)
#define IRQ_WARN(fmt, args...) WARN_PRINT("[IRQ] " fmt, ##args)

static nk_ivec_t __max_ivec_num = 0;
static nk_irq_t __max_irq_num = 0;

static inline nk_ivec_t max_ivec(void) {
  return __max_ivec_num;
}

static inline nk_irq_t max_irq(void) {
#ifdef NAUT_CONFIG_IDENTITY_MAP_IRQ_VECTORS
  return (nk_irq_t)max_ivec();
#else
  return __max_irq_num;
#endif
}

static inline void update_max_ivec(nk_ivec_t vec) {
  __max_ivec_num = vec > __max_ivec_num ? vec : __max_ivec_num;
}

static inline void update_max_irq(nk_irq_t irq) {
  __max_irq_num = irq > __max_irq_num ? irq : __max_irq_num;
}

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
  update_max_ivec(ivec_num);

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
  IRQ_DEBUG("Allocating Interrupt Vectors: [%u - %u]\n", base, base+n-1); 
  // This is pretty inefficient for now but it should be okay
  for(uint32_t i = 0; i < n; i++) {

    if(nk_alloc_ivec_desc(base + i, dev, type, flags)) 
    {
      IRQ_ERROR("Failed to allocate ivec decriptor: %u\n", base+i);
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

#ifndef MAX_IVEC_NUM
#error "MAX_IVEC_NUM is undefined!"
#endif

static struct nk_ivec_desc descriptors[MAX_IVEC_NUM];

int nk_alloc_ivec_desc(nk_ivec_t num, struct nk_irq_dev *dev, uint16_t type, uint16_t flags) 
{
  update_max_ivec(num);
  descriptors[num].ivec_num = num;
  descriptors[num].flags = flags;
  descriptors[num].type = type;
  descriptors[num].irq_dev = dev;
  spinlock_init(&(descriptors[num].lock));

  return 0;
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
  struct nk_ivec_desc *desc = descriptors + ivec;
  ASSERT(desc->ivec_num != ivec);
  return desc; 
}

#endif /* NAUT_CONFIG_SPARSE_INTERRUPT_IVECS */

int nk_ivec_add_handler(nk_ivec_t vec, nk_irq_handler_t handler, void *state) 
{
  return nk_ivec_add_handler_dev(vec, handler, state, NULL);
}
int nk_ivec_add_handler_dev(nk_ivec_t vec, nk_irq_handler_t handler, void *state, struct nk_dev *dev)
{
  struct nk_ivec_desc *desc = nk_ivec_to_desc(vec);
  if(desc == NULL) {
    return -1;
  }

  spin_lock(&desc->lock);

  if(desc->num_actions > 0) {

    struct nk_irq_action *action = malloc(sizeof(struct nk_irq_action));
    if(action == NULL) {
      return -1;
    }
    memset(action, 0, sizeof(struct nk_irq_action));

    action->handler = handler;
    action->handler_state = state;
    action->type = NK_IRQ_ACTION_TYPE_HANDLER;
    action->ivec_desc = desc;
    action->flags |= NK_IRQ_ACTION_FLAG_NO_IRQ;
    action->ivec = vec;
    action->dev = dev;

    action->next = desc->action.next;
    desc->action.next = action;

  } else {

    desc->action.handler = handler;
    desc->action.handler_state = state;
    desc->action.type = NK_IRQ_ACTION_TYPE_HANDLER;
    desc->action.ivec_desc = desc;
    desc->action.flags |= NK_IRQ_ACTION_FLAG_NO_IRQ;
    desc->action.ivec = vec;
    desc->action.dev = dev;

  }

  desc->num_actions += 1;
  spin_unlock(&desc->lock);

  return 0;
}

int nk_ivec_set_handler_early(nk_ivec_t vec, nk_irq_handler_t handler, void *state)
{
  struct nk_ivec_desc *desc = nk_ivec_to_desc(vec);
  if(desc == NULL) {
    return -1;
  }

  spin_lock(&desc->lock);

  if(desc->num_actions > 0) {
    spin_unlock(&desc->lock);
    return -1;
  }

  desc->action.handler = handler;
  desc->action.handler_state = state;
  desc->action.type = NK_IRQ_ACTION_TYPE_HANDLER;
  desc->action.ivec_desc = desc;
  desc->action.flags |= NK_IRQ_ACTION_FLAG_NO_IRQ;
  desc->action.ivec = vec;

  desc->num_actions += 1;

  spin_unlock(&desc->lock);

  return 0;
}

static inline int flags_compat(uint16_t flags, uint16_t requested, uint16_t banned) 
{
  return ((flags & requested) == requested) & !(flags & banned);
}

int nk_ivec_find_range(nk_ivec_t num_needed, int aligned, uint16_t needed_flags, uint16_t banned_flags, uint16_t set_flags, uint16_t clear_flags, nk_ivec_t *first) 
{
  nk_ivec_t i,j;

  // This a limit from the IDT but it seems reasonable for now
  if (num_needed>32) { 
    return -1;
  }

  for (i=0;i <= (max_ivec()+1)-num_needed;i += aligned ? num_needed : 1) {
    int run_good = 1;
    for(j=0;j<num_needed;j++) {
      
      struct nk_ivec_desc *desc = nk_ivec_to_desc(i+j);
      if(desc == NULL) {
        run_good = 0;
	break;
      }
      else if(desc->type == NK_IVEC_DESC_TYPE_INVALID) {
        run_good = 0;
	break;
      }
      else if(!flags_compat(desc->flags, needed_flags, banned_flags)) {
        run_good = 0;
	break;
      }    
    }

    if(run_good) {
      for(j = 0; j < num_needed; j++) {
        struct nk_ivec_desc *desc = nk_ivec_to_desc(i+j);
	desc->flags |= set_flags;
        desc->flags &= ~clear_flags;
	*first = i;
	return 0;
      }
    }
  }

  return -1;

}

/*
 * IRQ Action Functions
 */

int nk_handle_irq_actions(struct nk_ivec_desc * desc, struct nk_regs *regs) 
{ 
  int ret = 0;

  // This is just to get a rough estimate,
  // so we're going to tolerate not locking
  desc->triggered_count += 1;

  struct nk_irq_action *action = &desc->action;
  for(int i = 0; (i < desc->num_actions) && (action != NULL); i++, action = action->next) {

    switch(action->type) 
    {
      case NK_IRQ_ACTION_TYPE_HANDLER:
        ret |= action->handler(action, regs, action->handler_state);
	break;
      default:
        ret |= 1;
    }
  }
  return ret;
}

/*
 * IRQ Functions
 */

#ifdef NAUT_CONFIG_IDENTITY_MAP_IRQ_VECTORS
int nk_irq_init(void) 
{
  return 0;
}
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
  return nk_ivec_to_desc((nk_ivec_t)irq)->irq_dev;
}

#else

#ifdef NAUT_CONIFG_SPARSE_IRQ

int nk_irq_init(void) 
{
  return 0;
}
// We have no architectures which need this case yet
// so I'm just going to leave it empty for now, 
// potentially this would just be another radix tree
int nk_map_irq_to_ivec(nk_irq_t irq, nk_ivec_t ivec) 
{
  update_max_irq(irq);
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

#ifndef MAX_IRQ_NUM
#error "MAX_IRQ_NUM is undefined!"
#endif

static nk_ivec_t irq_to_ivec_array[MAX_IRQ_NUM];
static struct nk_irq_dev * irq_to_irqdev_array[MAX_IRQ_NUM];

int nk_irq_init(void) {
  memset(irq_to_ivec_array, 0, sizeof(irq_to_ivec_array));
  memset(irq_to_irqdev_array, 0, sizeof(irq_to_irqdev_array));
  return 0;
}
int nk_map_irq_to_ivec(nk_irq_t irq, nk_ivec_t ivec) 
{
  update_max_irq(irq);
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

int nk_irq_add_handler(nk_irq_t irq, nk_irq_handler_t handler, void *state) 
{
  return nk_irq_add_handler_dev(irq, handler, state, NULL);
}
int nk_irq_add_handler_dev(nk_irq_t irq, nk_irq_handler_t handler, void *state, struct nk_dev *dev)
{
  nk_ivec_t vec = nk_irq_to_ivec(irq);
  struct nk_ivec_desc *desc = nk_ivec_to_desc(vec);
  if(desc == NULL) {
    return -1;
  }

  spin_lock(&desc->lock);

  if(desc->num_actions > 0) {

    struct nk_irq_action *action = malloc(sizeof(struct nk_irq_action));
    if(action == NULL) {
      return -1;
    }
    memset(action, 0, sizeof(struct nk_irq_action));

    action->handler = handler;
    action->handler_state = state;
    action->type = NK_IRQ_ACTION_TYPE_HANDLER;
    action->ivec_desc = desc;
    action->irq = irq;
    action->ivec = vec;
    action->dev = dev;

    action->next = desc->action.next;
    desc->action.next = action;

  } else {

    desc->action.handler = handler;
    desc->action.handler_state = state;
    desc->action.type = NK_IRQ_ACTION_TYPE_HANDLER;
    desc->action.ivec_desc = desc;
    desc->action.irq = irq;
    desc->action.ivec = vec;
    desc->action.dev = dev;

  }

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

void nk_dump_ivec_info(void) 
{
  for(nk_ivec_t i = 0; i < max_ivec()+1; i++) 
  {
    struct nk_ivec_desc *desc = nk_ivec_to_desc(i);
    if(desc == NULL || desc->type == NK_IVEC_DESC_TYPE_INVALID) 
    {
      continue;
    }
    
    IRQ_PRINT("Interrupt Vector %u: type = %s, triggered count = %u, num_actions = %u, irqdev = %s, %s%s%s%s%s\n",
		    desc->ivec_num,
                    desc->type == NK_IVEC_DESC_TYPE_DEFAULT ? "DEFAULT" :
                    desc->type == NK_IVEC_DESC_TYPE_EXCEPTION ? "EXCEPTION" :
                    desc->type == NK_IVEC_DESC_TYPE_INVALID ? "INVALID" : // It should be skipped but just in case
                    desc->type == NK_IVEC_DESC_TYPE_SPURRIOUS ? "SPURRIOUS" : "UNKNOWN",
                    desc->triggered_count,
		    desc->num_actions,
		    desc->irq_dev == NULL ? "NULL" : desc->irq_dev->dev.name,
		    desc->flags & NK_IVEC_DESC_FLAG_RESERVED ? "[RESERVED] " : "",
		    desc->flags & NK_IVEC_DESC_FLAG_MSI ? "[MSI] " : "",
		    desc->flags & NK_IVEC_DESC_FLAG_MSI_X ? "[MSI-X] " : "",
		    desc->flags & NK_IVEC_DESC_FLAG_PERCPU ? "[PERCPU] " : "",
		    desc->flags & NK_IVEC_DESC_FLAG_IPI ? "[IPI] " : ""
		    );

    struct nk_irq_action *action = &desc->action;
    for(int act_num = 0; (act_num < desc->num_actions) && (action != NULL); act_num++, action = action->next) 
    {
      IRQ_PRINT("\tAction %u: type = %s, handler = %p, handler_state = %p, device = %s\n",
		act_num,
		action->type == NK_IRQ_ACTION_TYPE_HANDLER ? "handler" : "invalid",
		action->handler,
		action->handler_state,
		action->dev == NULL ? "NULL" : action->dev->name);
      if(!(action->flags & NK_IRQ_ACTION_FLAG_NO_IRQ)) {
        struct nk_irq_dev *dev = nk_irq_to_irqdev(action->irq);
	IRQ_PRINT("\t\tirq = %u, irq_dev = %s\n",
			action->irq,
			dev == NULL ? "NULL" : dev->dev.name);	
      }
    }    
  }
}

static int handle_shell_interrupt(char * buf, void * priv) 
{
  nk_dump_ivec_info();
  return 0;
}

static struct shell_cmd_impl interrupt_impl = {
  .cmd = "interrupts",
  .help_str = "dump interrupt info",
  .handler = handle_shell_interrupt,
};
nk_register_shell_cmd(interrupt_impl);
