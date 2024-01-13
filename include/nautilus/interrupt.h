#ifndef __NAUTILUS_INTERRUPT_H__
#define __NAUTILUS_INTERRUPT_H__

#include<nautilus/nautilus.h>
#include<nautilus/cpu.h>
#include<nautilus/naut_types.h>
#include<nautilus/spinlock.h>
#include<nautilus/irqdev.h>

nk_irq_t nk_max_irq(void);

struct nk_regs;
struct nk_irq_action;

// Returns the maximum IRQ no. registered
// (Can be used to (inefficiently) iterate over
// all IRQ descriptors)
nk_irq_t nk_max_irq(void);

/*
 * This defines the signature of all IRQ handler functions
 *
 * nk_irq_action can be used to get the vector number if needed, and all accesses to nk_regs
 * is architecture dependant
 */
typedef int(*nk_irq_handler_t)(struct nk_irq_action *, struct nk_regs *, void *state);

/*
 * When the interrupt is invoked the architecture should call
 * "nk_handle_irq_actions()" with the correct arguments.
 *
 * Right now the only option is setting a handler function.
 *
 */
#define NK_IRQ_ACTION_FLAG_RESERVED (1<<1)
#define NK_IRQ_ACTION_FLAG_MASKED   (1<<2) // This action is masked

#define NK_IRQ_ACTION_TYPE_INVALID       0
#define NK_IRQ_ACTION_TYPE_HANDLER       1
#define NK_IRQ_ACTION_TYPE_LINK          2

struct nk_irq_action {

    struct nk_irq_action *next;
    struct nk_irq_desc *desc;

    union 
    {
        struct { // NK_IRQ_ACTION_TYPE_HANDLER
                 
            nk_irq_handler_t handler;
            void *handler_state;

        };

        struct { // NK_IRQ_ACTION_TYPE_LINK

          nk_irq_t link_irq;

        };

    };

    struct nk_dev *dev;

    nk_irq_t irq;
    uint16_t flags;
    uint16_t type;  

};

int nk_handle_irq_actions(struct nk_irq_desc *desc, struct nk_regs *regs);

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
  struct nk_irq_dev **per_cpu_irq_devs;
  nk_hwirq_t hwirq;

  int devless_status;
  
  // This is a linked list but we need the first
  // entry to be within the descriptor so we
  // can statically allocate atleast 1 action per
  // vector.
  struct nk_irq_action action;
  uint32_t num_actions;

  spinlock_t lock;

  nk_irq_t irq;
  uint16_t flags;

  uint32_t triggered_count;
};

int nk_dump_irq(nk_irq_t i);

// Returns base IRQ number or NK_NULL_IRQ if an error occurred
int nk_request_irq_range(int n, nk_irq_t *out_base);

// Allocates and intializes IRQ descriptors using Kmem
struct nk_irq_desc * nk_alloc_irq_desc(nk_hwirq_t hwirq, int flags, struct nk_irq_dev *);
struct nk_irq_desc * nk_alloc_irq_descs(int n, nk_hwirq_t hwirq, int flags, struct nk_irq_dev *);

// Initializes statically allocated IRQ descriptors
int nk_setup_irq_desc(struct nk_irq_desc *desc, nk_hwirq_t hwirq, int flags, struct nk_irq_dev *dev);
int nk_setup_irq_descs(int n, struct nk_irq_desc *descs, nk_hwirq_t hwirq, int flags, struct nk_irq_dev *dev);

int nk_setup_irq_desc_devless(struct nk_irq_desc *desc, nk_hwirq_t hwirq, int flags, int fixed_status);
int nk_setup_irq_descs_devless(int n, struct nk_irq_desc *descs, nk_hwirq_t hwirq, int flags, int fixed_status);

// Associates an IRQ descriptor with an IRQ number
int nk_assign_irq_desc(nk_irq_t irq, struct nk_irq_desc *desc);
int nk_assign_irq_descs(int n, nk_irq_t irq, struct nk_irq_desc *desc);

// Makes the IRQ PERCPU and sets the irq_dev for a particular CPUID
int nk_set_irq_dev_percpu(cpu_id_t cpuid, nk_irq_t irq, struct nk_irq_dev *dev);
int nk_set_irq_devs_percpu(int n, cpu_id_t cpuid, nk_irq_t irq, struct nk_irq_dev *dev);
int nk_set_all_irq_dev_percpu(nk_irq_t irq, struct nk_irq_dev **devs);
int nk_set_all_irq_devs_percpu(int n, nk_irq_t irq, struct nk_irq_dev **devs);

struct nk_irq_desc * nk_irq_to_desc(nk_irq_t irq);

/*
 * nk_irq_find_range (finds and possibly modifies a range of descriptors with certain flags)
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

// Adds a handler to the irq (can allocate memory using kmem)
int nk_irq_add_handler(nk_irq_t, nk_irq_handler_t, void *state);
int nk_irq_add_handler_dev(nk_irq_t, nk_irq_handler_t, void *state, struct nk_dev *dev);

// Adds a link between the two IRQ's so that if "src" has it's actions handled, so will "dest"
// Common example is linking an x86 vector IRQ to an IOAPIC IRQ
int nk_irq_add_link(nk_irq_t src, nk_irq_t dest);
int nk_irq_add_link_dev(nk_irq_t src, nk_irq_t dest, struct nk_dev *dev);

// Adds a handler to the irq (Should not allocate memory,
// fails if the vector already has an action regardless if it is reserved or not)
int nk_irq_set_handler_early(nk_irq_t, nk_irq_handler_t, void *state);

/*
 * IRQ Functions
 */

int nk_irq_init(void);

int nk_map_irq_to_irqdev(nk_irq_t irq, struct nk_irq_dev *dev);
struct nk_irq_dev * nk_irq_to_irqdev(nk_irq_t);
struct nk_irq_dev * nk_irq_desc_to_irqdev(struct nk_irq_desc *);
int nk_irq_is_assigned_to_irqdev(nk_irq_t);

int nk_mask_irq(nk_irq_t);
int nk_unmask_irq(nk_irq_t);

int nk_send_ipi(nk_irq_t, cpu_id_t);

int nk_handle_interrupt_generic(struct nk_irq_action *null, struct nk_regs *regs, struct nk_irq_dev *dev);

#endif
