#ifndef __NAUTILUS_INTERRUPT_H__
#define __NAUTILUS_INTERRUPT_H__

#include<nautilus/nautilus.h>
#include<nautilus/cpu.h>
#include<nautilus/naut_types.h>
#include<nautilus/spinlock.h>
#include<nautilus/irqdev.h>

#define NK_NULL_IRQ (nk_irq_t)(-1)

nk_irq_t nk_max_irq(void);

struct nk_regs;
struct nk_irq_action;

/*
 * This defines the signature of all IRQ handler functions
 *
 * nk_irq_action can be used to get the vector number if needed, and all accesses to nk_regs
 * is architecture dependant
 */
typedef int(*nk_irq_handler_t)(struct nk_irq_action *, struct nk_regs *, void *state);

/*
 * irq_actions are different actions which can be registered with
 * each "ivec" and (optionally) associated with an IRQ.
 *
 * When the interrupt vector is invoked the architecture should call
 * "nk_handle_irq_actions()" with the correct arguments.
 *
 * Right now the only option is setting a handler function.
 *
 */
#define NK_IRQ_ACTION_FLAG_RESERVED (1<<1)
#define NK_IRQ_ACTION_FLAG_MASKED   (1<<2) // This action is masked

#define NK_IRQ_ACTION_TYPE_INVALID       0
#define NK_IRQ_ACTION_TYPE_HANDLER       1

struct nk_irq_action {

    struct nk_irq_action *next;
    struct nk_irq_desc *desc;

    union 
    {
        struct { // NK_IRQ_ACTION_TYPE_HANDLER
                 
            nk_irq_handler_t handler;
            void *handler_state;

        };
    };

    struct nk_dev *dev;

    nk_irq_t irq;
    uint16_t flags;
    uint16_t type;  

};

int nk_handle_irq_actions(struct nk_ivec_desc *desc, struct nk_regs *regs);

/*
 * "ivec" -> "Interrupt Vector"
 * 
 * "ivec"s represent the granule at which the hardware/architecture
 * can differentiate between different interrupt sources.
 *
 * As opposed to an "irq" which is more conceptual, and multiple
 * "irq"s could share the same "ivec" and the architecture won't
 * be able to tell which is which and should call both "irq"s handlers
 *
 */

#define NK_IRQ_DESC_FLAG_ALLOCATED    (1<<0)
#define NK_IRQ_DESC_FLAG_MSI          (1<<1)
#define NK_IRQ_DESC_FLAG_MSI_X        (1<<2)
#define NK_IRQ_DESC_FLAG_PERCPU       (1<<3)
#define NK_IRQ_DESC_FLAG_IPI          (1<<4)
#define NK_IRQ_DESC_FLAG_NMI          (1<<5)
#define NK_IRQ_DESC_FLAG_EDGE         (1<<6)
#define NK_IRQ_DESC_FLAG_EDGE_RISING  (1<<7)
#define NK_IRQ_DESC_FLAG_EDGE_FALLING (1<<8)
#define NK_IRQ_DESC_FLAG_LEVEL        (1<<9)
#define NK_IRQ_DESC_FLAG_LEVEL_LOW    (1<<10)
#define NK_IRQ_DESC_FLAG_LEVEL_HIGH   (1<<11)

struct nk_irq_desc {

  struct nk_irq_dev *irq_dev;
  nk_hwirq_t hwirq;

  // This is a linked list but we need the first
  // entry to be within the descriptor so we
  // can statically allocate atleast 1 action per
  // vector.
  struct nk_irq_action action;
  uint32_t num_actions;
  uint32_t num_unmasked_actions;

  spinlock_t lock;

  nk_irq_t irq;
  uint16_t flags;
  uint16_t type;

  uint32_t triggered_count;
};

void nk_dump_irq_info(void);

// Returns base IRQ number or NK_NULL_IRQ if an error occurred
nk_irq_t nk_alloc_irq_descs(int n, struct nk_irq_dev *dev, nk_hwirq_t hwirq, uint16_t type, uint16_t flags);
int nk_free_irq_descs(nk_irq_t base, int n);

struct nk_irq_desc * nk_irq_to_desc(nk_irq_t irq);

/*
 * nk_irq_find_range (finds and possibly modifies a range of NK_IVEC_DESC_TYPE_DEFAULT descriptors with certain flags)
 *
 * num: size of the range to find
 * flags: flags for the search, not the descriptor
 * needed_flags: every descriptor in the range must have all these flags set
 * banned_flags: no descriptor in the range can have any of these flags set
 *
 * set_flags: these flags will be set for all descriptors in the found range
 * clear_flags: these flags will be cleared for all descriptors in the found range
 *
 * first: will be set to the start of the range if one is found
 *
 * RETURNS: Either the base IRQ num, or NK_NULL_IRQ if no range could be found 
 */

#define NK_IRQ_RANGE_FLAG_ALIGNED    (1<<0)
nk_irq_t nk_irq_find_range(int num, int flags, uint16_t needed_flags, uint16_t banned_flags, uint16_t set_flags, uint16_t clear_flags);

// Adds a handler to the ivec (can allocate memory using kmem)
// Can fail if the vector is reserved
int nk_irq_add_handler(nk_ivec_t, nk_irq_handler_t, void *state);
int nk_irq_add_handler_dev(nk_ivec_t, nk_irq_handler_t, void *state, struct nk_dev *dev);

// Adds a handler to the ivec (Should not allocate memory,
// fails if the vector already has an action regardless if it is reserved or not)
int nk_irq_set_handler_early(nk_ivec_t, nk_irq_handler_t, void *state);

/*
 * IRQ Functions
 */

int nk_irq_init(void);

int nk_map_irq_to_irqdev(nk_irq_t irq, struct nk_irq_dev *dev);
struct nk_irq_dev * nk_irq_to_irqdev(nk_irq_t);
int nk_irq_is_assigned_to_irqdev(nk_irq_t);

int nk_mask_irq(nk_irq_t);
int nk_unmask_irq(nk_irq_t);

#endif
