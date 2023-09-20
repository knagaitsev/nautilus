#include<nautilus/interrupt.h>

#ifndef NAUT_CONFIG_DEBUG_INTERRUPT_FRAMEWORK
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) 
#endif

#define IRQ_ERROR(fmt, args...) ERROR_PRINT("[IRQ] " fmt, ##args)
#define IRQ_DEBUG(fmt, args...) DEBUG_PRINT("[IRQ] " fmt, ##args)
#define IRQ_PRINT(fmt, args...) printk("[IRQ] " fmt, ##args)

// Gives us MAX_IRQ_NUM
#include<nautilus/arch.h>

#include<nautilus/shell.h>
#include<nautilus/naut_assert.h>

static nk_irq_t irq_next_base = 0;

static nk_irq_t max_irq(void) {
  return irq_next_base - 1;
}

#ifdef NAUT_CONFIG_SPARSE_IRQ
#include<nautilus/radix_tree.h>

static struct nk_radix_tree sparse_irq_radix_tree;

static inline struct nk_irq_desc * __irq_to_desc(nk_irq_t irq_num) 
{
  return (struct nk_irq_desc*)nk_radix_tree_get(&sparse_irq_radix_tree, (unsigned long)irq_num);
}

static inline int __irq_insert_desc(nk_irq_t irq_num, struct nk_irq_desc *desc) 
{
  return nk_radix_tree_insert(&sparse_irq_radix_tree, irq_num, (void*)desc);
}

#else /* NAUT_CONFIG_SPARSE_IRQ */

#ifndef MAX_IRQ_NUM
#error "MAX_IRQ_NUM is undefined while NAUT_CONFIG_SPARSE_IRQ is not selected!"
#endif

static struct nk_irq_desc * descriptors[MAX_IRQ_NUM];

static inline struct nk_irq_desc * __irq_to_desc(nk_irq_t irq) 
{
  struct nk_irq_desc *desc = descriptors[irq];
  if(desc->irq != irq) {
    panic("desc->irq (%u) != irq (%u)\n", desc->irq, irq);

    return NULL;
  }
  return desc;
}

static inline int __irq_insert_desc(nk_irq_t irq, struct nk_irq_desc *desc) 
{
  if(irq < 0 || irq > MAX_IRQ_NUM) {
    return -1;
  }
  descriptors[irq] = desc; 
  return 0;
}

#endif /* NAUT_CONFIG_SPARSE_IRQ */

struct nk_irq_desc * nk_irq_to_desc(nk_irq_t irq) 
{
  return __irq_to_desc(irq);
}

int nk_request_irq_range(int n, nk_irq_t *out_base) 
{
#ifndef NAUT_CONFIG_SPARSE_IRQ
  if(irq_next_base + n > MAX_IRQ_NUM) {
    // Ran out of room 
    return -1;
  }
#endif
  // This could use some synchronization
  *out_base = irq_next_base;
  irq_next_base += n;
  return 0;
}

struct nk_irq_desc * nk_alloc_irq_desc(nk_hwirq_t hwirq, int flags, struct nk_irq_dev *dev) 
{
  struct nk_irq_desc *desc = malloc(sizeof(struct nk_irq_desc));
  if(desc == NULL) {
    return desc;
  }
  if(nk_setup_irq_desc(desc, hwirq, flags, dev)) {
    free(desc);
    return NULL;
  }
  IRQ_DEBUG("Allocated nk_irq_desc (hwirq=%u, flags=%p, dev=%p)\n", hwirq, flags, dev);
  return desc;
}
struct nk_irq_desc * nk_alloc_irq_descs(int num, nk_hwirq_t base_hwirq, int flags, struct nk_irq_dev *dev) {
  struct nk_irq_desc *descs = malloc(sizeof(struct nk_irq_desc) * num);
  if(descs == NULL) {
    return descs;
  }
  if(nk_setup_irq_descs(num, descs, base_hwirq, flags, dev)) {
    free(descs);
    return NULL;
  } 

  IRQ_DEBUG("Allocated %u nk_irq_descs (base_hwirq=%u, flags=%p, dev=%p)\n", num, base_hwirq, flags, dev);
  return descs;
}

int nk_setup_irq_desc(struct nk_irq_desc *desc, nk_hwirq_t hwirq, int flags, struct nk_irq_dev *dev) 
{
  memset(desc, 0, sizeof(struct nk_irq_desc));
  desc->irq = NK_NULL_IRQ;
  desc->hwirq = hwirq;
  desc->flags = flags;
  desc->irq_dev = dev;
  return 0;
}
int nk_setup_irq_descs(int n, struct nk_irq_desc *descs, nk_hwirq_t base_hwirq, int flags, struct nk_irq_dev *dev) 
{
  memset(descs, 0, sizeof(struct nk_irq_desc) * n);
  for(int i = 0; i < n; i++) {
    descs[i].irq = NK_NULL_IRQ;
    descs[i].hwirq = base_hwirq + i;
    descs[i].flags = flags;
    descs[i].irq_dev = dev;
  }
  return 0;
}

int nk_setup_irq_desc_devless(struct nk_irq_desc *desc, nk_hwirq_t hwirq, int flags, int status) 
{
  memset(desc, 0, sizeof(struct nk_irq_desc));
  desc->irq = NK_NULL_IRQ;
  desc->hwirq = hwirq;
  desc->flags = flags;
  desc->irq_dev = NULL;
  desc->devless_status = status;
  return 0;
}
int nk_setup_irq_descs_devless(int n, struct nk_irq_desc *descs, nk_hwirq_t base_hwirq, int flags, int status) 
{
  memset(descs, 0, sizeof(struct nk_irq_desc) * n);
  for(int i = 0; i < n; i++) {
    descs[i].irq = NK_NULL_IRQ;
    descs[i].hwirq = base_hwirq + i;
    descs[i].flags = flags;
    descs[i].irq_dev = NULL;
    descs[i].devless_status = status;
  }
  return 0;
}

int nk_assign_irq_desc(nk_irq_t irq, struct nk_irq_desc *desc) 
{
  desc->irq = irq;
  int res = __irq_insert_desc(irq, desc);
  if(res) {
    return res;
  }
  IRQ_DEBUG("Assigned irq=%u, irq_desc=%p\n", irq, desc);
  return 0;
}

int nk_assign_irq_descs(int num, nk_irq_t base_irq, struct nk_irq_desc *descs) 
{
  IRQ_DEBUG("Assigning IRQ's [%u - %u] descriptors\n", base_irq, base_irq + (num-1));
  for(int i = 0; i < num; i++) {
    descs[i].irq = base_irq + i;
    if(__irq_insert_desc(base_irq + i, &descs[i])) {
      return -1;
    }
  }
  IRQ_DEBUG("Assigned irq range [%u - %u] to descs=%p\n", base_irq, base_irq + (num-1), descs);
  return 0;
}

int nk_set_irq_dev_percpu(cpu_id_t cpuid, nk_irq_t irq, struct nk_irq_dev *dev) 
{
  struct nk_irq_desc *desc = nk_irq_to_desc(irq);
  if(desc == NULL) {
    return -1;
  }
  if(!(desc->flags & NK_IRQ_DESC_FLAG_PERCPU)) {
    return -1;
  }

  if(desc->per_cpu_irq_devs == NULL) {
    desc->per_cpu_irq_devs = malloc(sizeof(struct nk_irq_dev*) * nk_get_num_cpus());
    if(desc->per_cpu_irq_devs == NULL) {
      return -1;
    }
  }

  desc->per_cpu_irq_devs[cpuid] = dev;
  return -1;
}

int nk_set_irq_devs_percpu(int n, cpu_id_t cpuid, nk_irq_t irq, struct nk_irq_dev *dev) 
{
  int fail_count = 0;
  for(int i = 0; i < n; i++) {
    if(nk_set_irq_dev_percpu(cpuid, irq+i, dev)) {
      fail_count += 1;
    }
  }
  return fail_count;
}

int nk_set_all_irq_dev_percpu(nk_irq_t irq, struct nk_irq_dev **devs) 
{
  struct nk_irq_desc *desc = nk_irq_to_desc(irq);
  if(desc == NULL) {
    return -1;
  }
  if(!(desc->flags & NK_IRQ_DESC_FLAG_PERCPU)) {
    return -1; 
  }
  if(desc->per_cpu_irq_devs == NULL) {
    desc->per_cpu_irq_devs = devs;
    return 0;
  } else {
    return -1;
  }
}

int nk_set_all_irq_devs_percpu(int n, nk_irq_t irq, struct nk_irq_dev **devs) 
{
  int fail_count = 0;
  for(int i = 0; i < n; i++) {
    if(nk_set_all_irq_dev_percpu(irq+i, devs)) {
      fail_count += 1;
    }
  }
  return fail_count;
}

struct nk_irq_action * nk_irq_desc_add_action(struct nk_irq_desc *desc) 
{
  if(desc->num_actions > 0) 
  {
    struct nk_irq_action *action = malloc(sizeof(struct nk_irq_action));
    if(action == NULL) {
      return action;
    }
    memset(action, 0, sizeof(struct nk_irq_action));

    action->next = desc->action.next;
    desc->action.next = action;

    action->type = NK_IRQ_ACTION_TYPE_INVALID;
    action->desc = desc;
    action->irq = desc->irq;
    desc->num_actions += 1;

    return action;

  } else {

    memset(&desc->action, 0, sizeof(struct nk_irq_action)); 

    desc->action.type = NK_IRQ_ACTION_TYPE_INVALID;
    desc->action.desc = desc;
    desc->action.irq = desc->irq;
    desc->num_actions += 1;

    return &desc->action;
  }
}

int nk_irq_add_handler(nk_irq_t irq, nk_irq_handler_t handler, void *state) 
{
  return nk_irq_add_handler_dev(irq, handler, state, NULL);
}
int nk_irq_add_handler_dev(nk_irq_t irq, nk_irq_handler_t handler, void *state, struct nk_dev *dev)
{
  struct nk_irq_desc *desc = nk_irq_to_desc(irq);
  if(desc == NULL) {
    return -1;
  }

  spin_lock(&desc->lock);

  struct nk_irq_action *action = nk_irq_desc_add_action(desc);

  if(action == NULL) {
    spin_unlock(&desc->lock);
    return -1;
  }

  action->handler = handler;
  action->handler_state = state;
  action->type = NK_IRQ_ACTION_TYPE_HANDLER;
  action->irq = irq;
  action->dev = dev;

  spin_unlock(&desc->lock);
  return 0;
}

int nk_irq_set_handler_early(nk_irq_t irq, nk_irq_handler_t handler, void *state)
{
  struct nk_irq_desc *desc = nk_irq_to_desc(irq);
  if(desc == NULL) {
    return -1;
  }

  if(desc->num_actions) {
    // Make sure we use the action within the descriptor itself
    return -1;
  }
   
  return nk_irq_add_handler(irq, handler, state);
}

int nk_irq_add_link(nk_irq_t src, nk_irq_t dest) {
  return nk_irq_add_link_dev(src, dest, NULL);
}
int nk_irq_add_link_dev(nk_irq_t src, nk_irq_t dest, struct nk_dev *dev) 
{
  struct nk_irq_desc *src_desc = nk_irq_to_desc(src);
  if(src_desc == NULL) {
    return -1;
  }

  spin_lock(&src_desc->lock);

  struct nk_irq_action *action = nk_irq_desc_add_action(src_desc);

  if(action == NULL) {
    spin_unlock(&src_desc->lock);
    return -1;
  }

  action->link_irq = dest;
  action->type = NK_IRQ_ACTION_TYPE_LINK;
  action->irq = src;
  action->dev = dev;

  spin_unlock(&src_desc->lock);
  return 0;
}

static inline int flags_compat(uint16_t flags, uint16_t requested, uint16_t banned) 
{
  return ((flags & requested) == requested) & !(flags & banned);
}
nk_irq_t nk_irq_find_range(int num_needed, int flags, uint16_t needed_flags, uint16_t banned_flags, uint16_t set_flags, uint16_t clear_flags) 
{
/*
  nk_irq_t i,j;
  int aligned = flags & NK_IRQ_RANGE_ALIGNED;

  for (i=0;i <= (max_irq()+1)-num_needed;i += aligned ? num_needed : 1) {
    int run_good = 1;
    for(j=0;j<num_needed;j++) {
      
      struct nk_irq_desc *desc = nk_irq_to_desc(i+j);
      if(desc->type == NK_IRQ_DESC_TYPE_INVALID) {
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
        struct nk_irq_desc *desc = nk_irq_to_desc(i+j);
	desc->flags |= set_flags;
        desc->flags &= ~clear_flags;
      }
      return i;
    }
  }
*/
  return NK_NULL_IRQ;
}

/*
 * IRQ Action Functions
 */

// Really just to guard against loops
#define MAX_IRQ_CHAIN_DEPTH 64

static int __handle_irq_actions(struct nk_irq_desc *desc, struct nk_regs *regs, int depth) 
{
  if(depth > MAX_IRQ_CHAIN_DEPTH) {
    return -1;
  }

  // This is just to get a rough estimate,
  // so we're going to tolerate not locking
  desc->triggered_count += 1;

  struct nk_irq_action *action = &desc->action;
  int ret = 0;
  for(int i = 0; (i < desc->num_actions) && (action != NULL); i++, action = action->next) {

    struct nk_irq_desc *link_desc;

    switch(action->type) 
    {
      case NK_IRQ_ACTION_TYPE_HANDLER:
        ret |= action->handler(action, regs, action->handler_state);
	break;
      case NK_IRQ_ACTION_TYPE_LINK:
        link_desc = nk_irq_to_desc(action->link_irq);
        if(link_desc == NULL) {
          ret = 1;
        }
        else {
          ret |= __handle_irq_actions(link_desc, regs, depth+1);
        }
        break;
      default:
        ret |= 1;
    }
  }
  return ret;
}

int nk_handle_irq_actions(struct nk_irq_desc * desc, struct nk_regs *regs) 
{ 
  return __handle_irq_actions(desc, regs, 0);
}

/*
 * IRQ Functions
 */

int nk_map_irq_to_irqdev(nk_irq_t irq, struct nk_irq_dev *dev) 
{
  struct nk_irq_desc *desc = nk_irq_to_desc(irq);
  if(desc == NULL) {
    return -1;
  }
  desc->irq_dev = dev;
  return 0;
}

struct nk_irq_dev * nk_irq_desc_to_irqdev(struct nk_irq_desc *desc) 
{
  struct nk_irq_dev *dev = desc->irq_dev;

  if(desc->flags & NK_IRQ_DESC_FLAG_PERCPU) {
    if(desc->per_cpu_irq_devs != NULL) {
      dev = desc->per_cpu_irq_devs[my_cpu_id()];
    } else {
      // default to the main irq_dev
    }
  }

  return dev;
}

struct nk_irq_dev * nk_irq_to_irqdev(nk_irq_t irq) {
  struct nk_irq_desc *desc = nk_irq_to_desc((nk_irq_t)irq);
  if(desc == NULL) {
    return NULL;
  }
  return nk_irq_desc_to_irqdev(desc);
}

int nk_irq_is_assigned_to_irqdev(nk_irq_t irq) 
{
  return nk_irq_to_irqdev(irq) != NULL;
}

int nk_mask_irq(nk_irq_t irq) {
  struct nk_irq_desc *desc = nk_irq_to_desc(irq);
  if(desc == NULL) {
    return 1;
  }
  struct nk_irq_dev *irq_dev = nk_irq_desc_to_irqdev(desc);
  if(irq_dev == NULL) {
    if(desc->devless_status & IRQ_STATUS_ENABLED) {
      return -1;
    } else {
      return 0;
    }
  }
  return nk_irq_dev_disable_irq(irq_dev, desc->hwirq);
}

int nk_unmask_irq(nk_irq_t irq) {
  struct nk_irq_desc *desc = nk_irq_to_desc(irq);
  if(desc == NULL) {
    return 1;
  }
  struct nk_irq_dev *irq_dev = nk_irq_desc_to_irqdev(desc);
  if(irq_dev == NULL) {
    if(desc->devless_status & IRQ_STATUS_ENABLED) {
      return 0;
    } else {
      return -1;
    }
  }
  return nk_irq_dev_enable_irq(irq_dev, desc->hwirq);
}

int nk_handle_interrupt_generic(struct nk_irq_action *action, struct nk_regs *regs, struct nk_irq_dev *irq_dev) 
{
  nk_hwirq_t hwirq;
  nk_irq_dev_ack(irq_dev, &hwirq);

  nk_irq_t irq = NK_NULL_IRQ;
  if(nk_irq_dev_revmap(irq_dev, hwirq, &irq)) {
    return -1;
  }

  struct nk_irq_desc *desc = nk_irq_to_desc(irq);
  nk_handle_irq_actions(desc, regs);

  nk_irq_dev_eoi(irq_dev, hwirq);
  return 0;
}

void nk_dump_irq_info(void) 
{
  IRQ_PRINT("--- Interrupt Descriptors: (total = %u) ---\n", max_irq()+1);
  for(nk_irq_t i = 0; i < max_irq()+1; i++) 
  {
    struct nk_irq_desc *desc = nk_irq_to_desc(i);

    if(desc == NULL) 
    {
      continue;
    }

    int status = 0;
    if(desc->irq_dev != NULL) {
      status = nk_irq_dev_irq_status(desc->irq_dev, i);
    } else {
      status = desc->devless_status;
    }
   
    IRQ_PRINT("%u: triggered count = %u, num_actions = %u, irqdev = %s, hwirq = %u "
              "%s%s%s%s%s%s%s\n",
		    desc->irq,
                    desc->triggered_count,
		    desc->num_actions,
		    desc->flags & NK_IRQ_DESC_FLAG_PERCPU ? "PERCPU" : (desc->irq_dev != NULL ? desc->irq_dev->dev.name : "NULL"),
                    desc->hwirq,
                    status & IRQ_STATUS_ENABLED ? "[ENABLED] " : "[DISABLED] ",
                    status & IRQ_STATUS_PENDING ? "[PENDING] " : "",
                    status & IRQ_STATUS_ACTIVE ? "[ACTIVE] " : "",
		    desc->flags & NK_IRQ_DESC_FLAG_MSI ? "[MSI] " : "",
		    desc->flags & NK_IRQ_DESC_FLAG_MSI_X ? "[MSI-X] " : "",
		    desc->flags & NK_IRQ_DESC_FLAG_PERCPU ? "[PERCPU] " : "",
		    desc->flags & NK_IRQ_DESC_FLAG_IPI ? "[IPI] " : "",
                    desc->flags & NK_IRQ_DESC_FLAG_NMI ? "[NMI] " : ""
		    );

    struct nk_irq_action *action = &desc->action;
    for(int act_num = 0; (act_num < desc->num_actions) && (action != NULL); act_num++, action = action->next) 
    {
      IRQ_PRINT("\tAction %u: type = %s, device = %s %s\n",
		act_num,
		action->type == NK_IRQ_ACTION_TYPE_HANDLER ? "HANDLER" : 
		action->type == NK_IRQ_ACTION_TYPE_LINK    ? "LINK" :
		"INVALID",
		action->dev == NULL ? "NULL" : action->dev->name,
                action->flags & NK_IRQ_ACTION_FLAG_MASKED ? "[MASKED] " : ""
                );
      switch(action->type) {
        case NK_IRQ_ACTION_TYPE_HANDLER:
          IRQ_PRINT("\t\t{handler = %p, state = %p}\n",
              action->handler,
              action->handler_state
              );
	  break;
	case NK_IRQ_ACTION_TYPE_LINK:
	  IRQ_PRINT("\t\t{link = %u}\n", action->link_irq);
          break;
        default:
          IRQ_PRINT("\t\t{INVALID ACTION TYPE}\n");
          break;
      }
    }    
  }
}

static int handle_shell_interrupt(char * buf, void * priv) 
{
  nk_dump_irq_info();
  return 0;
}

static struct shell_cmd_impl interrupt_impl = {
  .cmd = "interrupts",
  .help_str = "interrupts",
  .handler = handle_shell_interrupt,
};
nk_register_shell_cmd(interrupt_impl);
