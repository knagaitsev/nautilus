#ifndef __NAUTILUS_INTERRUPT_H__
#define __NAUTILUS_INTERRUPT_H__

#include<nautilus/nautilus.h>
#include<nautilus/naut_types.h>
#include<nautilus/spinlock.h>
#include<nautilus/irqdev.h>

struct nk_regs;
struct nk_irq_action;

/*
 * This defines the signature of all IRQ callback functions
 *
 * nk_irq_action can be used to get the vector number if needed, and all accesses to nk_regs
 * is architecture dependant
 */
typedef int(*nk_irq_callback_t)(struct nk_irq_action *, struct nk_regs *, void *state);

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

#define NK_IVEC_DESC_FLAG_RESERVED  (1<<1)
#define NK_IVEC_DESC_FLAG_MSI       (1<<2)
#define NK_IVEC_DESC_FLAG_PERCPU    (1<<3)
#define NK_IVEC_DESC_FLAG_IPI       (1<<4)

#define NK_IVEC_DESC_TYPE_INVALID        0 // This IVEC # isn't backed by the hardware
#define NK_IVEC_DESC_TYPE_VALID          1

struct nk_ivec_desc {

  struct nk_irq_dev *irq_dev;

  struct nk_irq_action *actions;
  uint32_t num_actions;

  spinlock_t lock;

  nk_ivec_t ivec_num;
  uint16_t flags;
  uint16_t type;

};


int nk_alloc_ivec_desc(nk_ivec_t num, struct nk_irq_dev *dev, uint16_t type, uint16_t flags);
int nk_alloc_ivec_descs(nk_ivec_t base, uint32_t n, struct nk_irq_dev *dev, uint16_t type, uint16_t flags);

struct nk_ivec_desc * nk_ivec_to_desc(nk_ivec_t ivec);

// returns a mask of the previous flag values (atomic)
uint16_t nk_ivec_set_flags(nk_ivec_t ivec, uint16_t mask);
uint16_t nk_ivec_clear_flags(nk_ivec_t ivec, uint16_t mask);
uint16_t nk_ivec_check_flags(nk_ivec_t ivec, uint16_t mask);

int nk_ivec_add_callback(nk_irq_t, nk_irq_callback_t, void *state);

/*
 * irq_actions are different actions which can be registered with
 * each "ivec" and (optionally) associated with an IRQ.
 *
 * When the interrupt vector is invoked the architecture should call
 * "nk_handle_irq_actions()" with the correct arguments.
 *
 * Right now the only option is setting a callback function.
 *
 */
#define NK_IRQ_ACTION_FLAG_RESERVED (1<<1)
#define NK_IRQ_ACTION_FLAG_NO_IRQ   (1<<2) // This action isn't associated with any IRQ

#define NK_IRQ_ACTION_TYPE_INVALID       0
#define NK_IRQ_ACTION_TYPE_CALLBACK      1

struct nk_irq_action {

    struct nk_irq_action *next;
    struct nk_ivec_desc *ivec_desc;

    union 
    {
        struct { // NK_IRQ_ACTION_TYPE_CALLBACK
                 
            nk_irq_callback_t callback;
            void *callback_state;

        };
    };

    nk_irq_t irq;
    nk_ivec_t ivec;
    uint16_t flags;
    uint16_t type;  

};

int nk_handle_irq_actions(struct nk_irq_action *actions, struct nk_regs *regs);

/*
 * IRQ Functions
 */

int nk_map_irq_to_ivec(nk_irq_t irq, nk_ivec_t ivec);
nk_ivec_t nk_irq_to_ivec(nk_irq_t);

int nk_map_irq_to_irqdev(nk_irq_t irq, struct nk_irq_dev *dev);
struct nk_irq_dev * nk_irq_to_irqdev(nk_irq_t);
int nk_irq_is_assigned_to_irqdev(nk_irq_t);

struct nk_ivec_desc * nk_irq_to_desc(nk_irq_t);

int nk_irq_add_callback(nk_irq_t, nk_irq_callback_t, void *state);

int nk_mask_irq(nk_irq_t);
int nk_unmask_irq(nk_irq_t);

#endif
