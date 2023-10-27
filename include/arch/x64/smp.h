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

#ifndef __ARCH_X86_SMP_H__
#define __ARCH_X86_SMP_H__

#ifndef __ASSEMBLER__ 

#ifdef __cplusplus
extern "C" {
#endif

#include<nautilus/nautilus.h>

#include <arch/x64/msr.h>
#include <arch/x64/gdt.h>


/******* EXTERNAL INTERFACE *********/

uint8_t nk_get_cpu_by_lapicid (uint8_t lapicid);

/******* !EXTERNAL INTERFACE! *********/

struct ap_init_area {
    uint32_t stack;  // 0
    uint32_t rsvd; // to align the GDT on 8-byte boundary // 4
    uint32_t gdt[6] ; // 8
    uint16_t gdt_limit; // 32
    uint32_t gdt_base; // 34
    uint16_t rsvd1; // 38
    uint64_t gdt64[3]; // 40
    uint16_t gdt64_limit; // 64
    uint64_t gdt64_base; // 66
    uint64_t cr3; // 74
    struct cpu * cpu_ptr; // 82

    void (*entry)(struct cpu * core); // 90

} __packed;

int smp_early_init(struct naut_info * naut);
int smp_bringup_aps(struct naut_info * naut);
void smp_ap_entry (struct cpu * core);
int smp_setup_xcall_bsp (struct cpu * core);

#ifdef __cplusplus
}
#endif

#endif /* !__ASSEMBLER__ */
 
#define AP_TRAMPOLINE_ADDR 0xf000 
#define AP_BOOT_STACK_ADDR 0x1000
#define AP_INFO_AREA       0x2000

#define BASE_MEM_LAST_KILO 0x9fc00
#define BIOS_ROM_BASE      0xf0000
#define BIOS_ROM_END       0xfffff

#endif
