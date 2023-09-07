/* 
 * This file is part of the Nautilus AeroKernel developed
 * by the Hobbes and V3VEE Projects with funding from the 
 * United States National  Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  The Hobbes Project is a collaboration
 * led by Sandia National Laboratories that includes several national 
 * laboratories and universities. You can find out more at:
 * http://www.v3vee.org  and
 * http://xtack.sandia.gov/hobbes
 *
 * Copyright (c) 2015, Kyle C. Hale <kh@u.northwestern.edu>
 * Copyright (c) 2015, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Kyle C. Hale <kh@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#include <nautilus/nautilus.h>
#include <nautilus/cpu.h>
#include <nautilus/mm.h>
#include <arch/x64/idt.h>
#include <arch/x64/irq.h>

#ifndef NAUT_CONFIG_DEBUG_INTERRUPT_FRAMEWORK
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

#define PIC_MAX_IRQ_NUM    15     // this is really PIC-specific

#define PIC_MASTER_CMD_PORT  0x20
#define PIC_MASTER_DATA_PORT 0x21
#define PIC_SLAVE_CMD_PORT   0xa0
#define PIC_SLAVE_DATA_PORT  0xa1

/* NOTE: the APIC organizes interrupt priorities as follows:
 * class 0: interrupt vectors 0-15
 * class 1: interrupt vectors 16-31
 * .
 * .
 * .
 * class 15: interrupt vectors 0x1f - 0xff
 *
 * The upper 4 bits indicate the priority class
 * and the lower 4 represent the int number within
 * that class
 *
 * higher class = higher priority
 *
 * higher vector = higher priority within the class
 *
 * Because x86 needs the vectors 0-31, the first
 * two classes are reserved
 *
 * In short, we use vectors above 31, with increasing
 * priority with increasing vector number
 *
 */

void 
disable_8259pic (void)
{
    printk("Disabling legacy 8259 PIC\n");
    outb(0xff, PIC_MASTER_DATA_PORT);
    outb(0xff, PIC_SLAVE_DATA_PORT);
}

void 
imcr_begin_sym_io (void)
{
    /* ICMR */
    outb(0x70, 0x22);
    outb(0x01, 0x23);
}


uint8_t 
nk_int_matches_bus (struct nk_int_entry * entry, const char * bus_type, const uint8_t len)
{
    uint8_t src_bus;
    struct nk_bus_entry * bus;

    /* find the src bus id */
    src_bus = entry->src_bus_id;

    list_for_each_entry(bus, 
            &(nk_get_nautilus_info()->sys.int_info.bus_list),
            elm) {

        if (bus->bus_id == entry->src_bus_id) {
            return !strncmp(bus->bus_type, bus_type, len);
        }

    }

    return 0;
}


void 
nk_add_bus_entry (const uint8_t bus_id, const char * bus_type)
{
    struct nk_bus_entry * bus = NULL;
    struct naut_info * naut = nk_get_nautilus_info();

    bus = mm_boot_alloc(sizeof(struct nk_bus_entry));
    if (!bus) {
        ERROR_PRINT("Could not allocate bus entry\n");
        return;
    }
    memset(bus, 0, sizeof(struct nk_bus_entry));

    bus->bus_id  = bus_id;
    memcpy((void*)bus->bus_type, bus_type, 6);

    list_add(&(bus->elm), &(naut->sys.int_info.bus_list));

}

void
nk_add_int_entry (int_trig_t trig_mode,
                  int_pol_t  polarity,
                  int_type_t type,
                  uint8_t    src_bus_id,
                  uint8_t    src_bus_irq,
                  uint8_t    dst_ioapic_intin,
                  uint8_t    dst_ioapic_id)
{
    struct nk_int_entry * new = NULL;
    struct naut_info * naut = nk_get_nautilus_info();

    new = mm_boot_alloc(sizeof(struct nk_int_entry));
    if (!new) {
        ERROR_PRINT("Could not allocate IRQ entry\n");
        return;
    }
    memset(new, 0, sizeof(struct nk_int_entry));

    new->trig_mode        = trig_mode;
    new->polarity         = polarity;
    new->type             = type;
    new->src_bus_id       = src_bus_id;
    new->src_bus_irq      = src_bus_irq;
    new->dst_ioapic_intin = dst_ioapic_intin;
    new->dst_ioapic_id    = dst_ioapic_id;

    DEBUG_PRINT("nk_add_int_entry(trig=%u,pol=%u,type=%u,src_bus_id=%u,src_bus_irq=%u,dst_ioapic_intin=%u,dst_ioapic_id=%u) ptr=%p\n",
		    trig_mode,
		    polarity,
		    type,
		    src_bus_id,
		    src_bus_irq,
		    dst_ioapic_intin,
		    dst_ioapic_id,
		    new);

    list_add(&(new->elm), &(naut->sys.int_info.int_list));
}

nk_irq_t x86_irq_vector_base = NK_NULL_IRQ;
static struct nk_irq_desc x86_irq_vector_descs[256];

int 
x86_irq_vector_init (struct sys_info * sys)
{
    struct nk_int_info * info = &(sys->int_info);

    if(nk_request_irq_range(256, &x86_irq_vector_base)) {
      // Couldn't get a range of IRQ's
    }

    struct nk_irq_desc *excp_desc = x86_irq_vector_descs;
    struct nk_irq_desc *other_desc = x86_irq_vector_descs + 32;

    int res;
    res = nk_setup_irq_descs_devless(32, excp_desc,
        0, // hwirq
	0, // flags
        IRQ_STATUS_ENABLED
        );
    if(res) {
      return -1;
    }
    res = nk_setup_irq_descs_devless(256-32, other_desc,
        32, 
        NK_IRQ_DESC_FLAG_MSI | NK_IRQ_DESC_FLAG_MSI_X,
        IRQ_STATUS_ENABLED
	);
    if(res) {
      return -1;
    }

    res = nk_assign_irq_descs(256, x86_irq_vector_base, x86_irq_vector_descs);
    if(res) {
      return -1;
    }

    INIT_LIST_HEAD(&(info->int_list));
    INIT_LIST_HEAD(&(info->bus_list));
    return 0;
}

