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
 * http://xstack.sandia.gov/hobbes
 *
 * Copyright (c) 2023, Kevin McAfee <kevinmcafee2022@u.northwestern.edu>
 * Copyright (c) 2023, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Kevin McAfee <kevinmcafee2022@u.northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */
#include <arch/riscv/riscv_idt.h>
#include <nautilus/irq.h>

ulong_t riscv_idt_handler_table[NUM_IDT_ENTRIES];

int
riscv_null_irq_handler (ulong_t irq)
{
    
    panic("received irq: %d\n", irq);
    
    return 0;
}

int
riscv_irq_install(ulong_t irq, int (*handler_addr)(ulong_t))
{
    arch_irq_enable((int)irq);
    return riscv_idt_assign_entry(irq, (ulong_t)handler_addr);
}

int
riscv_idt_assign_entry (ulong_t entry, ulong_t handler_addr)
{

    if (entry >= NUM_IDT_ENTRIES) {
        ERROR_PRINT("Assigning invalid IDT entry\n");
        return -1;
    }

    if (!handler_addr) {
        ERROR_PRINT("attempt to assign null handler\n");
        return -1;
    }

    riscv_idt_handler_table[entry] = handler_addr;

    return 0;
}

int
riscv_idt_get_entry (ulong_t entry, ulong_t *handler_addr)
{

    if (entry >= NUM_IDT_ENTRIES) {
        ERROR_PRINT("Getting invalid IDT entry\n");
        return -1;
    }

    *handler_addr = riscv_idt_handler_table[entry];

    return 0;
}

int
riscv_setup_idt (void)
{
    uint_t i;

    // clear the IDT out
    memset(&riscv_idt_handler_table, 0, sizeof(ulong_t) * NUM_IDT_ENTRIES);

    for (i = 0; i < NUM_IDT_ENTRIES; i++) {
        riscv_idt_assign_entry(i, (ulong_t)riscv_null_irq_handler);
    }

    return 0;
}

int riscv_handle_irq(ulong_t irq)
{
    if (irq) {
        ulong_t irq_handler = 0;
        riscv_idt_get_entry(irq, &irq_handler);
        ((int (*)())irq_handler)(irq);
    }

    return 0;
}

