#include<nautilus/interrupt.h>

// Gives us MAX_IRQ_NUM and MAX_IVEC_NUM
#include<nautilus/arch.h>
#include<nautilus/shell.h>
#include<nautilus/naut_assert.h>

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

  if(desc->num_actions > 0) {

    struct nk_irq_action *action = malloc(sizeof(struct nk_irq_action));
    if(action == NULL) {
      return -1;
    }
    memset(action, 0, sizeof(struct nk_irq_action));

    action->handler = handler;
    action->handler_state = state;
    action->type = NK_IRQ_ACTION_TYPE_HANDLER;
    action->desc = desc;
    action->flags |= NK_IRQ_ACTION_FLAG_NO_IRQ;
    action->irq = irq;
    action->dev = dev;

    action->next = desc->action.next;
    desc->action.next = action;

  } else {

    desc->action.handler = handler;
    desc->action.handler_state = state;
    desc->action.type = NK_IRQ_ACTION_TYPE_HANDLER;
    desc->action.desc = desc;
    desc->action.flags |= NK_IRQ_ACTION_FLAG_NO_IRQ;
    desc->action.irq = irq;
    desc->action.dev = dev;

  }

  desc->num_actions += 1;

  spin_unlock(&desc->lock);
  return 0;
}

int nk_irq_set_handler_early(nk_irq_t irq, nk_irq_handler_t handler, void *state)
{
  struct nk_irq_desc *desc = nk_irq_to_desc(irq);
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
  desc->action.desc = desc;
  desc->action.flags |= NK_IRQ_ACTION_FLAG_NO_IRQ;
  desc->action.irq = irq;

  desc->num_actions += 1;

  spin_unlock(&desc->lock);

  return 0;
}

static inline int flags_compat(uint16_t flags, uint16_t requested, uint16_t banned) 
{
  return ((flags & requested) == requested) & !(flags & banned);
}

nk_irq_t nk_irq_find_range(int num_needed, int flags, uint16_t needed_flags, uint16_t banned_flags, uint16_t set_flags, uint16_t clear_flags) 
{
  nk_irq_t i,j;
  int aligned = flags & NK_IRQ_RANGE_ALIGNED;

  for (i=0;i <= (max_irq()+1)-num_needed;i += aligned ? num_needed : 1) {
    int run_good = 1;
    for(j=0;j<num_needed;j++) {
      
      struct nk_irq_desc *desc = nk_irq_to_desc(i+j);
      if(desc == NULL) {
        if(flags & NK_IRQ_RANGE_ALLOC) {
          run_good = 0;
	  break;
        }
      }
      else if(desc->type == NK_IRQ_DESC_TYPE_INVALID) {
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

  return NK_NULL_IRQ;
}

/*
 * IRQ Action Functions
 */

int nk_handle_irq_actions(struct nk_irq_desc * desc, struct nk_regs *regs) 
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

int nk_map_irq_to_irqdev(nk_irq_t irq, struct nk_irq_dev *dev) 
{
  nk_irq_to_desc(irq)->irq_dev = dev;
  return 0;
}

struct nk_irq_dev * nk_irq_to_irqdev(nk_irq_t irq) 
{
  struct nk_irq_desc *desc = nk_irq_to_desc((nk_irq_t)irq);
  if(desc->flags & NK_IRQ_DESC_FLAG_PERCPU) {
    return per_cpu_get(irq_dev);
  }
  else {
    return desc->irq_dev;
  }
}

int nk_irq_is_assigned_to_irqdev(nk_irq_t irq) 
{
  return nk_irq_to_irqdev(irq) != NULL;
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

void nk_dump_irq_info(void) 
{
  IRQ_PRINT("--- Interrupt Descriptors: (max = %u) ---\n", max_irq()+1);
  for(nk_irq_t i = 0; i < max_irq()+1; i++) 
  {
    struct nk_irq_desc *desc = nk_irq_to_desc(i);
    if(desc == NULL || !(desc->flags & NK_IRQ_DESC_FLAG_ALLOCATED)) 
    {
      continue;
    }

    struct nk_irq_dev *dev = nk_irq_to_irqdev(irq);
    int status = nk_irq_dev_irq_status(dev, irq);
   
    IRQ_PRINT("Interrupt Descriptor %u: type = %s, triggered count = %u, num_actions = %u, irqdev = %s, "
              "%s%s%s%s%s%s%s\n",
		    desc->irq,
                    desc->triggered_count,
		    desc->num_actions,
		    desc->irq_dev != NULL ? desc->irq_dev->dev.name : desc->flags & NK_IRQ_DESC_FLAG_PERCPU ? "PERCPU" : "NULL",
                    status & IRQ_DEV_STATUS_ENABLED ? "[ENABLED] " : "[DISABLED] ",
                    status & IRQ_DEV_STATUS_PENDING ? "[PENDING] " : "",
                    status & IRQ_DEV_STATUS_ACTIVE ? "[ACTIVE] " : "",
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
		action->type == NK_IRQ_ACTION_TYPE_HANDLER ? "handler" : "invalid",
		action->dev == NULL ? "NULL" : action->dev->name,
                action->flags & NK_IRQ_ACTION_FLAG_MASKED ? "[MASKED] " : "",
                );
      switch(action->type) {
        case NK_IRQ_ACTION_TYPE_HANDLER:
          IRQ_PRINT("\t\t{handler = %p, state = %p}\n",
              action->handler,
              action->handler_state
              );
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
