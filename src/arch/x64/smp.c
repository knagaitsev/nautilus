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

#include <arch/x64/smp.h>
#include <arch/x64/gdt.h>
#include <arch/x64/idt.h>
#include <arch/x64/mtrr.h>

#include <nautilus/nautilus.h>
#include <nautilus/acpi.h>
#include <nautilus/smp.h>
#include <nautilus/sfi.h>
#include <nautilus/irq.h>
#include <nautilus/mm.h>
#include <nautilus/percpu.h>
#include <nautilus/numa.h>
#include <nautilus/cpu.h>
#include <nautilus/fpu.h>

#ifndef NAUT_CONFIG_DEBUG_SMP
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif
#define SMP_PRINT(fmt, args...) printk("SMP: " fmt, ##args)
#define SMP_DEBUG(fmt, args...) DEBUG_PRINT("SMP: " fmt, ##args)
#define SMP_ERROR(fmt, args...) ERROR_PRINT("SMP: " fmt, ##args)

static volatile unsigned smp_core_count = 1; // assume BSP is booted

extern addr_t init_smp_boot;
extern addr_t end_smp_boot;

uint8_t cpu_info_ready = 0;

static uint8_t mp_entry_lengths[5] = {
    MP_TAB_CPU_LEN,
    MP_TAB_BUS_LEN,
    MP_TAB_IOAPIC_LEN,
    MP_TAB_IO_INT_LEN,
    MP_TAB_LINT_LEN,
};


static void
parse_mptable_cpu (struct sys_info * sys, struct mp_table_entry_cpu * cpu)
{
    struct cpu * new_cpu = NULL;

    if (sys->num_cpus == NAUT_CONFIG_MAX_CPUS) {
        panic("CPU count exceeded max (check your .config)\n");
    }

    if(!(new_cpu = mm_boot_alloc(sizeof(struct cpu)))) {
        panic("Couldn't allocate CPU struct\n");
    } 
    memset(new_cpu, 0, sizeof(struct cpu));

    new_cpu->id         = sys->num_cpus;
    new_cpu->lapic_id   = cpu->lapic_id;

    new_cpu->enabled    = cpu->enabled;
    new_cpu->is_bsp     = cpu->is_bsp;
    new_cpu->cpu_sig    = cpu->sig;
    new_cpu->feat_flags = cpu->feat_flags;
    new_cpu->system     = sys;
    new_cpu->cpu_khz    = nk_detect_cpu_freq(new_cpu->id);

    SMP_DEBUG("CPU %u -- LAPIC ID 0x%x -- Features: ", new_cpu->id, new_cpu->lapic_id);
#ifdef NAUT_CONFIG_DEBUG_SMP
    printk("%s", (new_cpu->feat_flags & 1) ? "fpu " : "");
    printk("%s", (new_cpu->feat_flags & (1<<7)) ? "mce " : "");
    printk("%s", (new_cpu->feat_flags & (1<<8)) ? "cx8 " : "");
    printk("%s", (new_cpu->feat_flags & (1<<9)) ? "apic " : "");
    printk("\n");
#endif
    SMP_DEBUG("\tEnabled?=%01d\n", new_cpu->enabled);
    SMP_DEBUG("\tBSP?=%01d\n", new_cpu->is_bsp);
    SMP_DEBUG("\tSignature=0x%x\n", new_cpu->cpu_sig);
    SMP_DEBUG("\tFreq=%lu.%03lu MHz\n", new_cpu->cpu_khz/1000, new_cpu->cpu_khz%1000);

    spinlock_init(&new_cpu->lock);

    sys->cpus[sys->num_cpus] = new_cpu;

    sys->num_cpus++;
}


static void
parse_mptable_ioapic (struct sys_info * sys, struct mp_table_entry_ioapic * ioapic)
{
    struct ioapic * ioa = NULL;
    if (sys->num_ioapics == NAUT_CONFIG_MAX_IOAPICS) {
        panic("IOAPIC count exceeded max (change it in .config)\n");
    }

    if (!(ioa = mm_boot_alloc(sizeof(struct ioapic)))) {
        panic("Couldn't allocate IOAPIC struct\n");
    }
    memset(ioa, 0, sizeof(struct ioapic));

    SMP_DEBUG("IOAPIC entry:\n");
    SMP_DEBUG("\tID=0x%x\n", ioapic->id);
    SMP_DEBUG("\tVersion=0x%x\n", ioapic->version);
    SMP_DEBUG("\tEnabled?=%01d\n", ioapic->enabled);
    SMP_DEBUG("\tBase Addr=0x%lx\n", ioapic->addr);

    ioa->id      = ioapic->id;
    ioa->version = ioapic->version;
    ioa->usable  = ioapic->enabled;
    ioa->base    = (addr_t)ioapic->addr;

    sys->ioapics[sys->num_ioapics] = ioa;
    sys->num_ioapics++; 
}


static void
parse_mptable_lint (struct sys_info * sys, struct mp_table_entry_lint * lint)
{
    char * type_map[4] = {"[INT]", "[NMI]", "[SMI]", "[ExtINT]"};
    char * po_map[4] = {"[BUS]", "[ActHi]", "[Rsvd]", "[ActLo]"};
    char * el_map[4] = {"[BUS]", "[Edge]", "[Rsvd]", "[Level]"};
    uint8_t polarity = lint->int_flags & 0x3;
    uint8_t trig_mode = (lint->int_flags >> 2) & 0x3;
    SMP_DEBUG("LINT entry\n");
    SMP_DEBUG("\tInt Type=%s\n", type_map[lint->int_type]);
    SMP_DEBUG("\tPolarity=%s\n", po_map[polarity]);
    SMP_DEBUG("\tTrigger Mode=%s\n", el_map[trig_mode]);
    SMP_DEBUG("\tSrc Bus ID=0x%02x\n", lint->src_bus_id);
    SMP_DEBUG("\tSrc Bus IRQ=0x%02x\n", lint->src_bus_irq);
    SMP_DEBUG("\tDst LAPIC ID=0x%02x\n", lint->dst_lapic_id);
    SMP_DEBUG("\tDst LAPIC LINTIN=0x%02x\n", lint->dst_lapic_lintin);
}

static void
parse_mptable_ioint (struct sys_info * sys, struct mp_table_entry_ioint * ioint)
{
    char * type_map[4] = {"[INT]", "[NMI]", "[SMI]", "[ExtINT]"};
    char * po_map[4] = {"[BUS]", "[ActHi]", "[Rsvd]", "[ActLo]"};
    char * el_map[4] = {"[BUS]", "[Edge]", "[Rsvd]", "[Level]"};
    uint8_t polarity = ioint->int_flags & 0x3;
    uint8_t trig_mode = (ioint->int_flags >> 2) & 0x3;
    SMP_DEBUG("IOINT entry\n");
    SMP_DEBUG("\tType=%s\n", type_map[ioint->int_type]);
    SMP_DEBUG("\tPolarity=%s\n", po_map[polarity]);
    SMP_DEBUG("\tTrigger Mode=%s\n", el_map[trig_mode]);
    SMP_DEBUG("\tSrc Bus ID=0x%02x\n", ioint->src_bus_id);
    SMP_DEBUG("\tSrc Bus IRQ=0x%02x\n", ioint->src_bus_irq);
    SMP_DEBUG("\tDst IOAPIC ID=0x%02x\n", ioint->dst_ioapic_id);
    SMP_DEBUG("\tDst IOAPIC INT Pin=0x%02x\n", ioint->dst_ioapic_intin);

    nk_add_int_entry(trig_mode,
                     polarity,
                     ioint->int_type,
                     ioint->src_bus_id,
                     ioint->src_bus_irq,
                     ioint->dst_ioapic_intin,
                     ioint->dst_ioapic_id);
}

static void
parse_mptable_bus (struct sys_info * sys, struct mp_table_entry_bus * bus)
{
    SMP_DEBUG("Bus entry\n");
    SMP_DEBUG("\tBus ID: 0x%02x\n", bus->bus_id);
    SMP_DEBUG("\tType: %.6s\n", bus->bus_type_string);
    nk_add_bus_entry(bus->bus_id, bus->bus_type_string);
}

static uint8_t 
blk_cksum_ok (uint8_t * mp, unsigned len)
{
    unsigned sum = 0;

    while (len--) {
        sum += *mp++;
    }

    return ((sum & 0xff) == 0);
}


static int
parse_mp_table (struct sys_info * sys, struct mp_table * mp)
{
    int count = mp->entry_cnt;
    uint8_t * mp_entry;

    /* make sure everything is as expected */
    if (strncmp((char*)&mp->sig, "PCMP", 4) != 0) {
        SMP_ERROR("MP Table unexpected format\n");
    }

    mp_entry = (uint8_t*)&mp->entries;
    SMP_PRINT("Parsing MP Table (entry count=%u)\n", mp->entry_cnt);


    SMP_PRINT("Verifying MP Table integrity...");
    if (!blk_cksum_ok((uint8_t*)mp, mp->len)) {
        printk("FAIL\n");
        SMP_ERROR("Corrupt MP Table detected\n");
    } else {
        printk("OK\n");
    }

    while (count--) {

        uint8_t type = *mp_entry;

        switch (type) {
            case MP_TAB_TYPE_CPU:
                parse_mptable_cpu(sys, (struct mp_table_entry_cpu*)mp_entry);
                break;
            case MP_TAB_TYPE_IOAPIC:
                parse_mptable_ioapic(sys, (struct mp_table_entry_ioapic*)mp_entry);
                break;
            case MP_TAB_TYPE_IO_INT:
                parse_mptable_ioint(sys, (struct mp_table_entry_ioint*)mp_entry);
                break;
            case MP_TAB_TYPE_BUS:
                parse_mptable_bus(sys, (struct mp_table_entry_bus*)mp_entry);
                break;
            case MP_TAB_TYPE_LINT:
                parse_mptable_lint(sys, (struct mp_table_entry_lint*)mp_entry);
                break;
            default:
                SMP_ERROR("Unexpected MP Table Entry (type=%d)\n", type);
                return -1;
        }

        mp_entry += mp_entry_lengths[type];
    }

    return 0;
}


static struct mp_float_ptr_struct* 
find_mp_pointer (void)
{
    char * cursor = (char*)BASE_MEM_LAST_KILO;

    /* NOTE: these memory regions should already be mapped, 
     * if not, this will fail 
     */

    while (cursor <= (char*)(BASE_MEM_LAST_KILO+PAGE_SIZE)) {

        if (strncmp(cursor, "_MP_", 4) == 0) {
            return (struct mp_float_ptr_struct*)cursor;
        }

        cursor += 4;
    }

    cursor = (char*)BIOS_ROM_BASE;

    while (cursor <= (char*)BIOS_ROM_END) {

        if (strncmp(cursor, "_MP_", 4) == 0) {
            return (struct mp_float_ptr_struct*)cursor;
        }

        cursor += 4;
    }
    return 0;
}

#define PIC_MODE_ON (1 << 7)

static int
__early_init_mp (struct naut_info * naut)
{
    struct mp_float_ptr_struct * mp_ptr;

    mp_ptr = find_mp_pointer();

    if (!mp_ptr) {
        SMP_ERROR("Could not find MP floating pointer struct\n");
        return -1;
    }

    naut->sys.pic_mode_enabled = !!(mp_ptr->mp_feat2 & PIC_MODE_ON);

    SMP_PRINT("Verifying MP Floating Ptr Struct integrity...");
    if (!blk_cksum_ok((uint8_t*)mp_ptr, 16)) {
        printk("FAIL\n");
        SMP_ERROR("Corrupt MP Floating Ptr Struct detected\n");
		return -1;
    } else {
        printk("OK\n");
    }

    parse_mp_table(&(naut->sys), (struct mp_table*)(uint64_t)mp_ptr->mp_cfg_ptr);

    return 0;
}


static int
acpi_parse_lapic (struct acpi_subtable_header * hdr,
				  const unsigned long end)
{
	struct acpi_madt_local_apic *p =
		(struct acpi_madt_local_apic *)hdr;
	struct sys_info * sys = &(nk_get_nautilus_info()->sys);
	struct cpu * new_cpu  = NULL;

	/* is it enabled? */
	if (!(p->lapic_flags & ACPI_MADT_ENABLED)) {
		SMP_DEBUG("[ ACPI Disabled CPU ] (skipping)\n");
		return 0;
	}

#ifdef NAUT_CONFIG_DEBUG_SMP
	acpi_table_print_madt_entry(hdr);
#endif

	if (sys->num_cpus == NAUT_CONFIG_MAX_CPUS) {
		panic("CPU count exceeded max (check your .config)\n");
	}

	if (!(new_cpu = mm_boot_alloc(sizeof(struct cpu)))) {
		panic("Couldn't allocate CPU struct\n");
	}
	memset(new_cpu, 0, sizeof(struct cpu));

	new_cpu->id         = sys->num_cpus;
	new_cpu->lapic_id   = p->id;
	new_cpu->enabled    = 1;
	new_cpu->cpu_sig    = 0;
	new_cpu->feat_flags = 0;
	new_cpu->system     = sys;
	new_cpu->cpu_khz    = nk_detect_cpu_freq(new_cpu->id);
	new_cpu->is_bsp     = (p->id == 0 ? 1 : 0);

	spinlock_init(&new_cpu->lock);
	
	sys->cpus[sys->num_cpus] = new_cpu;
	
	sys->num_cpus++;

	return 0;
}

static int acpi_parse_local_x2apic (struct acpi_subtable_header * hdr,
				    const unsigned long end)
{
    struct acpi_madt_local_x2apic *p =
	(struct acpi_madt_local_x2apic *)hdr;
    struct sys_info * sys = &(nk_get_nautilus_info()->sys);
    struct cpu * new_cpu  = NULL;
    
    /* is it enabled? */
    if (!(p->lapic_flags & ACPI_MADT_ENABLED)) {
	SMP_DEBUG("[ ACPI Disabled CPU (X2APIC) ] (skipping)\n");
	return 0;
    }

#ifdef NAUT_CONFIG_DEBUG_SMP
    acpi_table_print_madt_entry(hdr);
#endif
    
    if (sys->num_cpus == NAUT_CONFIG_MAX_CPUS) {
	panic("CPU count exceeded max (check your .config)\n");
    }

    if (!(new_cpu = mm_boot_alloc(sizeof(struct cpu)))) {
	panic("Couldn't allocate CPU struct\n");
    }

    memset(new_cpu, 0, sizeof(struct cpu));

    new_cpu->id         = sys->num_cpus;
    new_cpu->lapic_id   = p->local_apic_id;
    new_cpu->enabled    = 1;
    new_cpu->cpu_sig    = 0;
    new_cpu->feat_flags = 0;
    new_cpu->system     = sys;
    new_cpu->cpu_khz    = nk_detect_cpu_freq(new_cpu->id);
    new_cpu->is_bsp     = 0; // id > 255 by design

    spinlock_init(&new_cpu->lock);
    
    sys->cpus[sys->num_cpus] = new_cpu;

    sys->num_cpus++;

    return 0;
}

static int acpi_parse_interrupt_override (struct acpi_subtable_header * hdr,
				const unsigned long end)
{
#ifdef NAUT_CONFIG_DEBUG_SMP
    acpi_table_print_madt_entry(hdr);
#endif
    return 0;
}

static int acpi_parse_nmi_source (struct acpi_subtable_header * hdr,
				  const unsigned long end)
{
#ifdef NAUT_CONFIG_DEBUG_SMP
    acpi_table_print_madt_entry(hdr);
#endif
    return 0;
}

static int acpi_parse_local_apic_nmi (struct acpi_subtable_header * hdr,
				      const unsigned long end)
{
#ifdef NAUT_CONFIG_DEBUG_SMP
    acpi_table_print_madt_entry(hdr);
#endif
    return 0;
}

static int acpi_parse_local_apic_override (struct acpi_subtable_header * hdr,
					   const unsigned long end)
{
#ifdef NAUT_CONFIG_DEBUG_SMP
    acpi_table_print_madt_entry(hdr);
#endif
    return 0;
}

static int acpi_parse_io_sapic (struct acpi_subtable_header * hdr,
				const unsigned long end)
{
#ifdef NAUT_CONFIG_DEBUG_SMP
    acpi_table_print_madt_entry(hdr);
#endif
    return 0;
}

static int acpi_parse_local_sapic (struct acpi_subtable_header * hdr,
				   const unsigned long end)
{
#ifdef NAUT_CONFIG_DEBUG_SMP
    acpi_table_print_madt_entry(hdr);
#endif
    return 0;
}

static int acpi_parse_interrupt_source (struct acpi_subtable_header * hdr,
					const unsigned long end)
{
#ifdef NAUT_CONFIG_DEBUG_SMP
    acpi_table_print_madt_entry(hdr);
#endif
    return 0;
}

static int acpi_parse_local_x2apic_nmi (struct acpi_subtable_header * hdr,
					const unsigned long end)
{
#ifdef NAUT_CONFIG_DEBUG_SMP
    acpi_table_print_madt_entry(hdr);
#endif
    return 0;
}

static int acpi_parse_madt_reserved (struct acpi_subtable_header * hdr,
				     const unsigned long end)
{
#ifdef NAUT_CONFIG_DEBUG_SMP
    acpi_table_print_madt_entry(hdr);
#endif
    return 0;
}



#if 0
static int
acpi_parse_lint (struct acpi_subtable_header * hdr,
				   const unsigned long end)
{
	return 0;
}
#endif

static int
acpi_parse_ioapic (struct acpi_subtable_header * hdr,
				   const unsigned long end)
{
	struct acpi_madt_io_apic *p =
		(struct acpi_madt_io_apic *)hdr;
	struct sys_info * sys = &(nk_get_nautilus_info()->sys);
	struct ioapic * ioa = NULL;

#ifdef NAUT_CONFIG_DEBUG_SMP
	acpi_table_print_madt_entry(hdr);
#endif

	if (sys->num_ioapics == NAUT_CONFIG_MAX_IOAPICS) {
		panic("IOAPIC count exceeded max (change it in .config\n");
	}

	if (!(ioa = mm_boot_alloc(sizeof(struct ioapic)))) {
		panic("Couldn't allocate IOAPIC struct\n");
	}
	memset(ioa, 0, sizeof(struct ioapic));

	
	ioa->id      = p->id;
	ioa->version = 0;
	ioa->usable  = 1;
	ioa->base    = p->address;

	sys->ioapics[sys->num_ioapics] = ioa;
	sys->num_ioapics++;

	return 0;
}



static int
acpi_table_parse_madt (enum acpi_madt_type id, 
					   acpi_table_entry_handler handler, 
					   unsigned max_entries) 
{
	return (acpi_table_parse_entries(ACPI_SIG_MADT,
					 sizeof(struct acpi_table_madt), 
					 id, 
					 handler, 
					 max_entries) < 0);
}


static int 
acpi_parse_madt (struct acpi_table_header * hdr, void * arg)
{
    struct acpi_table_madt *p = (struct acpi_table_madt *)hdr;
    struct sys_info * sys = &(nk_get_nautilus_info()->sys);
    int pcat = p->flags & ACPI_MADT_PCAT_COMPAT;

    SMP_DEBUG("Parsing MADT...\n");
    SMP_DEBUG("flags=%x (PCAT_COMPAT=%d (%s))\n",
	      p->flags, pcat, pcat ? "Dual PIC" : "Multiple APIC");

    if (pcat) { 
	sys->flags |= NK_SYS_LEGACY;
    }

    return 0;
}


static int 
__early_init_madt (struct naut_info * naut)
{
    if (acpi_table_parse(ACPI_SIG_MADT, acpi_parse_madt, NULL) != 0) {
	SMP_ERROR("Could not parse MADT\n");
	return -1;
    } 
    
    /* Find all the LAPICS (and their associated CPUs, of course */
    if (acpi_table_parse_madt(ACPI_MADT_TYPE_LOCAL_APIC,
			      acpi_parse_lapic,
			      NAUT_CONFIG_MAX_CPUS) != 0) {
	SMP_ERROR("Unable to parse MADT LAPIC entries\n");
	return -1;
    }

    // The ACPI model is that all APICs, *whether XAPIC or X2APIC*
    // whose id is <256 ar reported ONLY as lapics.  Those whose ids
    // are 256+ are X2APICS, so we need to now add whatever
    // X2APICs and their processors we can find
    if (acpi_table_parse_madt(ACPI_MADT_TYPE_LOCAL_X2APIC,
			      acpi_parse_local_x2apic,
			      NAUT_CONFIG_MAX_CPUS)) {
	SMP_ERROR("Unable to parse MADT X2APIC entries\n");
	return -1;
    }
    
    /* find all the IOAPICS */
    if (acpi_table_parse_madt(ACPI_MADT_TYPE_IO_APIC,
			      acpi_parse_ioapic,
			      NAUT_CONFIG_MAX_IOAPICS) != 0) {
	SMP_ERROR("Unable to parse MADT IOAPIC entries\n");
	return -1;
    }

#ifdef NAUT_CONFIG_DEBUG_SMP
    // These are included for now just to let us easily
    // see the structure of the machine
    if (acpi_table_parse_madt(ACPI_MADT_TYPE_INTERRUPT_SOURCE,
			      acpi_parse_interrupt_source,
			      NAUT_CONFIG_MAX_CPUS)) { 
	SMP_ERROR("Unable to parse interrupt sources\n");
	return -1;
    }
    
    if (acpi_table_parse_madt(ACPI_MADT_TYPE_INTERRUPT_OVERRIDE,
			      acpi_parse_interrupt_override,
			      NAUT_CONFIG_MAX_CPUS)) { 
	SMP_ERROR("Unable to parse interrupt overrides\n");
	return -1;
    }
    
    if (acpi_table_parse_madt(ACPI_MADT_TYPE_NMI_SOURCE,
			      acpi_parse_interrupt_source,
			      NAUT_CONFIG_MAX_CPUS)) { 
	SMP_ERROR("Unable to parse NMI sources\n");
	return -1;
    }
    
    if (acpi_table_parse_madt(ACPI_MADT_TYPE_LOCAL_APIC_NMI,
			      acpi_parse_local_apic_nmi,
			      NAUT_CONFIG_MAX_CPUS)) { 
	SMP_ERROR("Unable to parse local apic nmi information\n");
	return -1;
    }
    
    if (acpi_table_parse_madt(ACPI_MADT_TYPE_LOCAL_APIC_OVERRIDE,
			      acpi_parse_local_apic_override,
			      NAUT_CONFIG_MAX_CPUS)) { 
	SMP_ERROR("Unable to parse local apic overrides\n");
	return -1;
    }
    
    if (acpi_table_parse_madt(ACPI_MADT_TYPE_IO_SAPIC,
			      acpi_parse_io_sapic,
			      NAUT_CONFIG_MAX_CPUS)) { 
	SMP_ERROR("Unable to parse IO SAPICS\n");
	return -1;
    }
    
    if (acpi_table_parse_madt(ACPI_MADT_TYPE_LOCAL_SAPIC,
			      acpi_parse_local_sapic,
			      NAUT_CONFIG_MAX_CPUS)) { 
	SMP_ERROR("Unable to parse local SAPICS\n");
	return -1;
    }
    
    
    if (acpi_table_parse_madt(ACPI_MADT_TYPE_LOCAL_X2APIC_NMI,
			      acpi_parse_local_x2apic_nmi,
			      NAUT_CONFIG_MAX_CPUS)) { 
	SMP_ERROR("Unable to parse local X2APIC NMIs\n");
	return -1;
    }

#endif
    
    return 0;
}



static int acpi_table_parse_fadt (enum acpi_madt_type id, 
				  acpi_table_entry_handler handler, 
				  unsigned max_entries) 
{
    
    return (acpi_table_parse_entries(ACPI_SIG_FADT,
				     sizeof(struct acpi_table_fadt), 
				     id, 
				     handler, 
				     max_entries) < 0);
}


static int acpi_parse_fadt(struct acpi_table_header * hdr, void * arg)
{
    struct acpi_table_fadt *p = (struct acpi_table_fadt *)hdr;
    struct sys_info * sys = &(nk_get_nautilus_info()->sys);
    int legacy=0;
    int i8042=0;
    int novga=0;
    int nomsi=0;
    
    SMP_DEBUG("Parsing FADT...\n");
    SMP_DEBUG("system interrupt model: %x\n",p->model);
    SMP_DEBUG("boot_flags: %x\n",p->boot_flags);
    
    legacy = p->boot_flags & ACPI_FADT_LEGACY_DEVICES;
    i8042 = p->boot_flags & ACPI_FADT_8042;
    novga = p->boot_flags & ACPI_FADT_NO_VGA;
    nomsi = p->boot_flags & ACPI_FADT_NO_MSI;

    SMP_DEBUG("flags are: legacy=%d i8042=%d novga=%d nomsi=%d\n",
	legacy,i8042,novga,nomsi);

    if (legacy) {
       sys->flags |= NK_SYS_LEGACY;
    }

    return 0;
}

static int __early_init_fadt_legacy(struct naut_info * naut)
{
    if (acpi_table_parse(ACPI_SIG_FADT, acpi_parse_fadt, NULL) != 0) {
	SMP_ERROR("Could not parse FADT\n");
	return -1;
    } else {
        return 0; 
    }
}



static int __acpi_configure_legacy(struct naut_info * naut)
{
    // Add an ISA bus, assume it is bus zero.  
    // Add IRQ0..15, attaching to first I/O apic in the typical way
    uint32_t ioapic_id;
    if (naut->sys.num_ioapics<1) { 
	SMP_ERROR("No IOAPICs!\n");
	return -1;
    }
    

    ioapic_id = naut->sys.ioapics[0]->id;

    SMP_DEBUG("Configuring legacy ISA bus 0 with legacy ISA device IRQs 0..15 channeled to first ioapic (id 0x%x) in the MPSpec manner\n",ioapic_id);

    // the legacy setup should be 1-1 except for 
    // any interrupt source overrides.   In the following
    // we simple set up the typical overrides.   This should 
    // be FIXED eventually, but this will work for typical machines now 

    nk_add_bus_entry(0,"ISA");
    nk_add_int_entry(0, // [BUS] trigger mode
		     0, // [BUS] polarity
		     0, // [INT] interrupt type
		     0, // Bus 0 (ISA)
		     0, // Bus IRQ
		     2, // pin on destination ioapic
		     ioapic_id // destination ioapic
		     ); // Timer 8254 PIT
    // Note that the PIT is wired strangely
    nk_add_int_entry(0,0,0,0,1,1,ioapic_id); // Keyboard 8042 KBD / PS2 KBD
    // there is no legacy irq 2 due to the 8259 master/slave setup 
    nk_add_int_entry(0,0,0,0,3,3,ioapic_id); // Serial 16550 COM2
    nk_add_int_entry(0,0,0,0,4,4,ioapic_id); // Serial 16550 COM1
    nk_add_int_entry(0,0,0,0,5,5,ioapic_id); // Parallel LPT2+3 or soundblaster
    nk_add_int_entry(0,0,0,0,6,6,ioapic_id); // Floppy controller
    nk_add_int_entry(0,0,0,0,7,7,ioapic_id); // Parallel LPT1
    nk_add_int_entry(0,0,0,0,8,8,ioapic_id); // Clock RTC
    //nk_add_int_entry(0,0,0,0,9,9,ioapic_id); // ACPI - not wired
    nk_add_int_entry(0,0,0,0,10,10,ioapic_id); // expansion
    nk_add_int_entry(0,0,0,0,11,11,ioapic_id); // expansion
    nk_add_int_entry(0,0,0,0,12,12,ioapic_id); // Mouse / PS2 AUX
    nk_add_int_entry(0,0,0,0,13,13,ioapic_id); // Coprocessor / etc
    nk_add_int_entry(0,0,0,0,14,14,ioapic_id); // Primary ATA controller
    nk_add_int_entry(0,0,0,0,15,15,ioapic_id); // Secondary ATA controller

    SMP_DEBUG("Legacy configuration finished\n");

    return 0;
}

static int
init_ap_area (struct ap_init_area * ap_area, 
              struct naut_info * naut,
              int core_num)
{
    memset((void*)ap_area, 0, sizeof(struct ap_init_area));

    /* setup pointer to this CPUs stack */
    uint32_t boot_stack_ptr = AP_BOOT_STACK_ADDR;

    ap_area->stack   = boot_stack_ptr;
    ap_area->cpu_ptr = naut->sys.cpus[core_num];

#ifdef NAUT_CONFIG_ARCH_X86
    /* protected mode temporary GDT */
    ap_area->gdt[2]      = 0x0000ffff;
    ap_area->gdt[3]      = 0x00cf9a00;
    ap_area->gdt[4]      = 0x0000ffff;
    ap_area->gdt[5]      = 0x00cf9200;

    // bases and limits for GDT and GDT64 are computed in the copy
    // that is placed in AP_INFO_AREA, hence not here

    /* long mode temporary GDT */
    ap_area->gdt64[1]    = 0x00af9a000000ffff;
    ap_area->gdt64[2]    = 0x00af92000000ffff;

    /* pointer to BSP's PML4 */
    ap_area->cr3         = read_cr3();
#endif

    /* pointer to our entry routine */
    ap_area->entry       = smp_ap_entry;

    return 0;
}


static int 
smp_wait_for_ap (struct naut_info * naut, unsigned int core_num)
{
    struct cpu * core = naut->sys.cpus[core_num];
#ifdef NAUT_CONFIG_XEON_PHI
    while (!core->booted) {
        udelay(1);
    }
#else
    BARRIER_WHILE(!core->booted);
#endif

    return 0;
}


int
smp_bringup_aps (struct naut_info * naut)
{
    struct ap_init_area * ap_area;

    addr_t boot_target     = (addr_t)&init_smp_boot;
    size_t smp_code_sz     = (addr_t)&end_smp_boot - boot_target;
    addr_t ap_trampoline   = (addr_t)AP_TRAMPOLINE_ADDR;
    uint8_t target_vec     = ap_trampoline >> 12U;
    struct apic_dev * apic = naut->sys.cpus[naut->sys.bsp_id]->apic;

    int status = 0; 
    int err = 0;
    int i, j, maxlvt;

    if (naut->sys.num_cpus == 1) {
        return 0;
    }

    #ifdef NAUT_CONFIG_ARCH_X86
    maxlvt = apic_get_maxlvt(apic);

    SMP_DEBUG("Passing target page num %x to SIPI\n", target_vec);

    /* clear APIC errors */
    if (maxlvt > 3) {
        apic_write(apic, APIC_REG_ESR, 0);
    }
    apic_read(apic, APIC_REG_ESR);
    #endif

    SMP_DEBUG("Copying in page for SMP boot code at (%p)...\n", (void*)ap_trampoline);
    memcpy((void*)ap_trampoline, (void*)boot_target, smp_code_sz);

    /* create an info area for APs */
    /* initialize AP info area (stack pointer, GDT info, etc) */
    ap_area = (struct ap_init_area*)AP_INFO_AREA;

    SMP_DEBUG("Passing AP area at %p\n", (void*)ap_area);

    /* START BOOTING AP CORES */
    
    /* we, of course, skip the BSP (NOTE: assuming it's 0...) */
    for (i = 0; i < naut->sys.num_cpus; i++) {
        int ret;

        /* skip the BSP */
        if (naut->sys.cpus[i]->is_bsp) {
            SMP_DEBUG("Skipping BSP (core id=%u, apicid=%u\n", i, naut->sys.cpus[i]->lapic_id);
            continue;
        }

        SMP_DEBUG("Booting secondary CPU %u\n", i);

        ret = init_ap_area(ap_area, naut, i);
        if (ret == -1) {
            ERROR_PRINT("Error initializing ap area\n");
            return -1;
        }


        #ifdef NAUT_CONFIG_ARCH_X86
        /* Send the INIT sequence */
        SMP_DEBUG("sending INIT to remote APIC (0x%x)\n", naut->sys.cpus[i]->lapic_id);
        apic_send_iipi(apic, naut->sys.cpus[i]->lapic_id);

        /* wait for status to update */
        status = apic_wait_for_send(apic);
        #endif

        mbarrier();

        /* 10ms delay */
        udelay(10000);

        #ifdef NAUT_CONFIG_ARCH_X86
        /* deassert INIT IPI (level-triggered) */
        apic_deinit_iipi(apic, naut->sys.cpus[i]->lapic_id);

        for (j = 1; j <= 2; j++) {
            if (maxlvt > 3) {
                apic_write(apic, APIC_REG_ESR, 0);
            }
            apic_read(apic, APIC_REG_ESR);

            SMP_DEBUG("Sending SIPI %u to core %u (vec=%x)\n", j, i, target_vec);

            /* send the startup signal */
            apic_send_sipi(apic, naut->sys.cpus[i]->lapic_id, target_vec);

            udelay(300);

            status = apic_wait_for_send(apic);

            udelay(200);

            err = apic_read(apic, APIC_REG_ESR) & 0xef;

            if (status || err) {
                break;
            }

            /* if it already booted up, we don't need to send the 2nd SIPI */
            if (naut->sys.cpus[i]->booted == 1) {
                break;
            }

        }
        #endif

        if (status) {
            ERROR_PRINT("APIC wasn't delivered!\n");
        }

        if (err) {
            ERROR_PRINT("ERROR delivering SIPI\n");
        }

        /* wait for AP to set its boot flag */
        smp_wait_for_ap(naut, i);

        SMP_DEBUG("Bringup for core %u done.\n", i);
    }

    BARRIER_WHILE(smp_core_count != naut->sys.num_cpus);

    SMP_DEBUG("ALL CPUS BOOTED\n");

    /* we can now use gs-based percpu data */
    cpu_info_ready = 1;

    return (status|err);
}

extern struct cpu * smp_ap_stack_switch(uint64_t, uint64_t, struct cpu*);

extern struct idt_desc idt_descriptor;
extern struct gdt_desc64 gdtr64;

int 
smp_early_init (struct naut_info * naut)
{
	int ret;

	SMP_DEBUG("Checking for MADT\n");

    /* try to use ACPI if possible. Newer machines may have strange
     * MP Table corruption 
     */
	if (__early_init_madt(naut) == 0) {
	    SMP_DEBUG("MADT init succeeded - now also checking for legacy system via FADT\n");
	    __early_init_fadt_legacy(naut);
	    // At this point we have determined if this is a legacy system
	    if (naut->sys.flags & NK_SYS_LEGACY) { 
		SMP_DEBUG("ACPI system with legacy - configuring\n");
		if (__acpi_configure_legacy(naut)) {
		    panic("Cannot configure ACPI system with legacy\n");
		}
	    }
	    goto out_ok;
	} else {
		SMP_DEBUG("MADT not present or working. Falling back on MP Table\n");
		if (__early_init_mp(naut) == 0) {
			goto out_ok;
		} else {
			panic("Neither ACPI MADT nor MP Table present/working! Cannot detect CPUs. Giving up.\n");
		}
	}
			
out_ok:
    SMP_PRINT("Detected %u CPUs\n", naut->sys.num_cpus);
	return 0;
}

static int
smp_xcall_init_queue (struct cpu * core)
{
    core->xcall_q = nk_queue_create();
    if (!core->xcall_q) {
        ERROR_PRINT("Could not allocate xcall queue on cpu %u\n", core->id);
        return -1;
    }

    return 0;
}


int
smp_setup_xcall_bsp (struct cpu * core)
{
    SMP_PRINT("Setting up cross-core IPI event queue\n");
    smp_xcall_init_queue(core);

    if (nk_ivec_add_handler(IPI_VEC_XCALL, xcall_handler, NULL) != 0) {
        ERROR_PRINT("Could not assign interrupt handler for XCALL on core %u\n", core->id);
        return -1;
    }

    return 0;
}

static int
smp_ap_setup (struct cpu * core)
{
    // Note that any use of SSE/AVX, for example produced by
    // clang/llvm optimation, that happens before fpu_init will
    // cause a panic.  Initialize FPU ASAP.
    
    // setup IDT
    lidt(&idt_descriptor);

    // setup GDT
    lgdt64(&gdtr64);

    uint64_t core_addr = (uint64_t) core->system->cpus[core->id];

    // set GS base (for per-cpu state)
    msr_write(MSR_GS_BASE, (uint64_t)core_addr);

    fpu_init(nk_get_nautilus_info(), FPU_AP_INIT);
    
    if (nk_mtrr_init_ap()) {
	ERROR_PRINT("Could not initialize MTRRs for core %u\n",core->id);
	return -1;
    }

#ifdef NAUT_CONFIG_ENABLE_REMOTE_DEBUGGING 
    if (nk_gdb_init_ap() != 0) {
        ERROR_PRINT("Could not initialize remote debugging for core %u\n", core->id);
	return -1;
    }
#endif

#ifdef NAUT_CONFIG_ALLOCS
    if (nk_alloc_init_ap()) { 
	ERROR_PRINT("Could not set up allocators for core %u\n",core->id);
    }
#endif

#ifdef NAUT_CONFIG_USE_IST
    nk_gdt_init_ap(core);
#endif

#ifdef NAUT_CONFIG_ASPACES
    if (nk_aspace_init_ap()) {
        ERROR_PRINT("Could not set up aspaces for core %u\n",core->id);
    }
#endif
    
    apic_init(core);

    if (smp_xcall_init_queue(core) != 0) {
        ERROR_PRINT("Could not setup xcall for core %u\n", core->id);
        return -1;
    }
    
    extern struct nk_sched_config sched_cfg;

    if (nk_sched_init_ap(&sched_cfg) != 0) {
        ERROR_PRINT("Could not setup scheduling for core %u\n", core->id);
        return -1;
    }

#ifdef NAUT_CONFIG_FIBER_ENABLE
    if (nk_fiber_init_ap() != 0) {
        ERROR_PRINT("Could not setup fiber thread for core %u\n", core->id);
        return -1;
    }
#endif

#ifdef NAUT_CONFIG_CACHEPART
    if (nk_cache_part_init_ap() != 0) {
        ERROR_PRINT("Could not setup cache partitioning for core %u\n", core->id);
        return -1;
    }
#endif

    return 0;
}


extern void nk_rand_init(struct cpu*);

static void
smp_ap_finish (struct cpu * core)
{
#ifdef NAUT_CONFIG_ARCH_X86
    nk_rand_init(core);

    nk_cpu_topo_discover(core);

    PAUSE_WHILE(atomic_cmpswap(core->booted, 0, 1) != 0);

#ifndef NAUT_CONFIG_HVM_HRT
    atomic_inc(smp_core_count);

    /* wait on all the other cores to boot up */
    BARRIER_WHILE(smp_core_count != core->system->num_cpus);
#endif

    nk_sched_start();

#ifdef NAUT_CONFIG_FIBER_ENABLE
    nk_fiber_startup();
#endif

#ifdef NAUT_CONFIG_CACHEPART
    if (nk_cache_part_start_ap() != 0) {
        ERROR_PRINT("Could not start cache partitioning for core %u\n", core->id);
    }
#endif

#ifdef NAUT_CONFIG_LINUX_SYSCALLS
    nk_syscall_init_ap();
#endif

    SMP_DEBUG("Core %u ready - enabling interrupts\n", core->id);

    arch_enable_ints();

#ifdef NAUT_CONFIG_PROFILE
    nk_instrument_calibrate(INSTR_CAL_LOOPS);
#endif
#endif
}


extern void idle(void* in, void**out);

void 
smp_ap_entry (struct cpu * core) 
{ 
    struct cpu * my_cpu;
    SMP_DEBUG("Core %u starting up\n", core->id);
    if (smp_ap_setup(core) < 0) {
        panic("Error setting up AP!\n");
    }

    /* we should now be able to pull our CPU pointer out of GS
     * This is important, because the stack will be clobbered
     * for the next CPU boot! 
     */
    my_cpu = get_cpu();
    SMP_DEBUG("CPU (AP) %u operational\n", my_cpu->id);

    // switch from boot stack to my new stack (allocated in thread_init)
    nk_thread_t * cur = get_cur_thread();

    /* 
     * we have to call into assembly since GCC 
     * wont let us clobber rbp. Note how we reassign
     * my_cpu. This is so we don't lose it in the
     * switch (it's sitting on the stack!)
     */
    my_cpu = smp_ap_stack_switch(cur->rsp, cur->rsp, my_cpu);

    // wait for the other cores and turn on interrupts
    smp_ap_finish(my_cpu);
    
    ASSERT(irqs_enabled());

    arch_enable_ints();

    idle(NULL, NULL);
}

int 
arch_smp_early_init (struct naut_info * naut)
{
  smp_early_init(naut);
}
