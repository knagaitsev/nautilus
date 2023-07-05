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
#include <nautilus/numa.h>
#include <nautilus/mm.h>
#include <nautilus/macros.h>
#include <nautilus/errno.h>
#include <nautilus/acpi.h>
#include <nautilus/list.h>
#include <nautilus/multiboot2.h>
#include <nautilus/endian.h>
#include <nautilus/fdt/fdt.h>

#define u8  uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define u64 uint64_t

static int srat_rev;

#ifndef NAUT_CONFIG_DEBUG_NUMA
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif
#define NUMA_PRINT(fmt, args...) printk("NUMA: " fmt, ##args)
#define NUMA_DEBUG(fmt, args...) DEBUG_PRINT("NUMA: " fmt, ##args)
#define NUMA_ERROR(fmt, args...) ERROR_PRINT("NUMA: " fmt, ##args)
#define NUMA_WARN(fmt, args...)  WARN_PRINT("NUMA: " fmt, ##args)

#define NUMA_GLOBAL_DOMAIN 0

static inline int
domain_exists (struct sys_info * sys, unsigned id)
{
    return sys->locality_info.domains[id] != NULL;
}

static inline struct numa_domain *
get_domain (struct sys_info * sys, unsigned id)
{
    return sys->locality_info.domains[id];
}

struct numa_domain *
nk_numa_domain_create (struct sys_info * sys, unsigned id)
{
    struct numa_domain * d = NULL;

    d = (struct numa_domain *)mm_boot_alloc(sizeof(struct numa_domain));
    if (!d) {
        ERROR_PRINT("Could not allocate NUMA domain\n");
        return NULL;
    }
    memset(d, 0, sizeof(struct numa_domain));

    d->id = id;

    INIT_LIST_HEAD(&(d->regions));
    INIT_LIST_HEAD(&(d->adj_list));

    if (id != (sys->locality_info.num_domains + 1)) { 
        NUMA_DEBUG("Memory regions are not in expected domain order, but that should be OK\n");
    }

    sys->locality_info.domains[id] = d;
    sys->locality_info.num_domains++;

    return d;
}

static int
numa_dtb_find_domains(struct sys_info * sys) {

    NUMA_PRINT("Parsing DTB NUMA information...\n");

    uint64_t offset = fdt_node_offset_by_prop_value(sys->dtb, -1, "device_type", "memory", 7);

    while(offset != -FDT_ERR_NOTFOUND) {

      fdt_reg_t reg = { .address = 0, .size = 0 };
      int getreg_result = fdt_getreg(sys->dtb, offset, &reg);

      if(getreg_result != 0) {
        NUMA_WARN("Found \"%s\" node in DTB with missing REG property! Skipping...\n", fdt_get_name(sys->dtb, offset, NULL));
        goto err_continue;
      }

      int lenp;
      uint32_t *numa_id_ptr = fdt_getprop(sys->dtb, offset, "numa-node-id", &lenp);

      if(numa_id_ptr == NULL) {
        NUMA_WARN("Found \"%s\" node in DTB without numa-node-id property! Switching to Identity NUMA Mapping!\n", fdt_get_name(sys->dtb, offset, NULL));
        return -1;
      }

      uint64_t id = *(uint32_t*)(numa_id_ptr);

      if(arch_little_endian()) {
        id = __builtin_bswap32(id);
      }

      struct mem_region *mem = mm_boot_alloc(sizeof(struct mem_region));
      if (!mem) {
        ERROR_PRINT("Could not allocate mem region\n");
        return -1;
      }
      memset(mem, 0, sizeof(struct mem_region));
        
      mem->domain_id     = id;
      mem->base_addr     = reg.address;
      mem->len           = reg.size;
      mem->enabled       = 1; 
   
      struct numa_domain *numa_domain = NULL;
      if(!domain_exists(sys, id)) {
        numa_domain = nk_numa_domain_create(sys, id);
	if(numa_domain == NULL) {
          NUMA_ERROR("Failed to allocate NUMA domain %u structure!\n", id);
	  goto err_continue;
	}
      } else {
        numa_domain = get_domain(sys, id);
      }

      numa_domain->num_regions++;
      numa_domain->addr_space_size += mem->len;
     
      if (list_empty(&numa_domain->regions)) {
          list_add(&mem->entry, &numa_domain->regions);
      } else {
          list_add_tail(&mem->entry, &numa_domain->regions);
      }

      NUMA_DEBUG("Found NUMA region in domain %u: [%p - %p]\n", id, reg.address, reg.address + reg.size);

err_continue:
      offset = fdt_node_offset_by_prop_value(sys->dtb, offset, "device_type", "memory", 7);
    }
  return 0;
}

static int
numa_dtb_assign_cpus(struct sys_info * sys) {

    NUMA_DEBUG("Assigning CPU's to NUMA Domains...\n");

    uint32_t offset = fdt_node_offset_by_prop_value(sys->dtb, -1, "device_type", "cpu", 4);

    while(offset != -FDT_ERR_NOTFOUND) {

      fdt_reg_t reg = { .address = 0, .size = 0 };
      int getreg_result = fdt_getreg(sys->dtb, offset, &reg);

      if(getreg_result != 0) {
        NUMA_WARN("Found \"%s\" CPU node in DTB with missing REG property!\n", fdt_get_name(sys->dtb, offset, NULL));
	goto cpu_err_continue;
      }

      int lenp;
      void *numa_id_ptr = fdt_getprop(sys->dtb, offset, "numa-node-id", &lenp);

      if(numa_id_ptr == NULL) {
        NUMA_ERROR("Found \"%s\" CPU node in DTB without numa-node-id property!\n", fdt_get_name(sys->dtb, offset, NULL));
        NUMA_ERROR("Assigning CPU %u to domain %u (PROBABLY WON'T WORK BUT THE ALTERNATIVE IS GOING TO CRASH ANYWAYS)\n", reg.address, NUMA_GLOBAL_DOMAIN);
        sys->cpus[reg.address]->domain = NUMA_GLOBAL_DOMAIN;
	goto cpu_err_continue;
      }

      uint64_t id = *(uint32_t*)(numa_id_ptr);

      id = be32toh(id);

      struct numa_domain *domain = get_domain(sys, id);

      if(domain == NULL) {
        NUMA_WARN("CPU %u has NUMA Domain of %u, which doesn't exist!\n", reg.address, id);
        goto cpu_err_continue;
      }

      sys->cpus[reg.address]->domain = domain;

      NUMA_DEBUG("Assigned CPU %u to NUMA Domain %u\n", reg.address, id);

cpu_err_continue:
      offset = fdt_node_offset_by_prop_value(sys->dtb, offset, "device_type", "cpu", 4);
    }
  return 0;
}

static int
numa_init_fake(struct sys_info * sys) {
    
    NUMA_WARN("Faking domain %u\n", NUMA_GLOBAL_DOMAIN);

    struct numa_domain *n = nk_numa_domain_create(sys, NUMA_GLOBAL_DOMAIN);
    if (!n) {
        NUMA_ERROR("Cannot allocate fake domain %u\n", NUMA_GLOBAL_DOMAIN);
        return -1;
    }

    for(uint_t i = 0; i < mm_boot_num_regions(); i++) {
        mem_map_entry_t *mm_entry = mm_boot_get_region(i);
        // If this region in the memory map isn't availible memory skip over it
        if(mm_entry->type != MULTIBOOT_MEMORY_AVAILABLE) {
          continue;
        }

        struct mem_region *mem = mm_boot_alloc(sizeof(struct mem_region));
        if (!mem) {
            NUMA_ERROR("Could not allocate mem region\n");
            return -1;
        }
        memset(mem, 0, sizeof(struct mem_region));
        
        mem->domain_id     = NUMA_GLOBAL_DOMAIN;

        mem->base_addr     = mm_entry->addr;
        mem->len           = mm_entry->len;
        mem->enabled       = 1;
    
    
        n->num_regions++;
        n->addr_space_size += mem->len;
     
        if (list_empty(&n->regions)) {
            list_add(&mem->entry, &n->regions);
        } else {
            list_add_tail(&mem->entry, &n->regions);
        }
    }

    NUMA_PRINT("Assigning all CPU's to Fake Domain %u\n", NUMA_GLOBAL_DOMAIN);
    for (uint_t i = 0; i < sys->num_cpus; i++) {
        sys->cpus[i]->domain = get_domain(sys, NUMA_GLOBAL_DOMAIN);
    }

    return 0;
}

int 
fdt_numa_init (struct sys_info * sys)
{

//#define DEBUG_DO_IDENTITY_NUMA

#ifdef DEBUG_DO_IDENTITY_NUMA
  numa_init_fake(sys);
#else
    if(!numa_dtb_find_domains(sys)) {

      numa_dtb_assign_cpus(sys); 

    } else {
      // Error utilizing the DTB to find Domains, so we fake a Domain 0 containing all of memory
      // (worse performance but should still work)
      numa_init_fake(sys);

    }
#endif

    /* Final Sanity-Check/Debug-Log of the Domains */
    for(uint32_t i = 0; i < sys->locality_info.num_domains; i++) {
      struct numa_domain *numa_domain = get_domain(sys, i);
      if(numa_domain == NULL) {
        NUMA_ERROR("Missing NUMA Domain: %u, (num_domains = %u)\n", i, sys->locality_info.num_domains);
        continue;
      }
      NUMA_DEBUG("Domain %u now has 0x%lx regions and size 0x%lx\n", numa_domain->id, numa_domain->num_regions, numa_domain->addr_space_size);
    }


    NUMA_PRINT("DONE.\n");

    return 0;
}
