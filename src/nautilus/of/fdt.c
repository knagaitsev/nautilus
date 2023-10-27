
#include<nautilus/endian.h>
#include<nautilus/of/fdt.h>
#include<lib/libfdt.h>

/*
 * Nautilus' extensions to libfdt
 */

int fdt_getreg(const void *fdt, int offset, fdt_reg_t *reg) {
	// from the device tree spec:
    // #size-cells and #address-cells are number of 32-bit cells that the
    // address and size data will be stored in for each reg prop
    // if not specified, assume: 2 for #address-cells, 1 for #size-cells

	int reg_lenp = 0;
        const char *reg_prop = fdt_getprop(fdt, offset, "reg", &reg_lenp);

        if(reg_prop == NULL) {
          return -1;
        }

	const void *address_cells_p = NULL;
	const void *size_cells_p = NULL;
	int parent_iter_offset = offset;
	while (!address_cells_p || !size_cells_p) {
		// unclear if we should return here or if we should fall back to the default values
		// listed above (2, 1)
		if (parent_iter_offset < 0) {
			return -1;
		}
		int lenp = 0;

		address_cells_p = fdt_getprop(fdt, parent_iter_offset, "#address-cells", &lenp);
		size_cells_p = fdt_getprop(fdt, parent_iter_offset, "#size-cells", &lenp);

		parent_iter_offset = fdt_parent_offset(fdt, parent_iter_offset);
	}

	uint32_t address_cells = be32toh(*((uint32_t *)address_cells_p));
	uint32_t size_cells = be32toh(*((uint32_t *)size_cells_p));

	// printk("Addr: %d, size: %d\n", address_cells, size_cells);

	int i = 0;
	if (address_cells > 0) {
		if (address_cells == 1) {
                  reg->address = be32toh(*((uint32_t *)reg_prop));
                }
                else if (address_cells == 2) {
                  // These need to be done as 32 bit loads because 64bit alignment isn't gaurenteed
                  uint32_t *addr_ptr = (uint32_t*)reg_prop;
                  reg->address = (uint64_t)be32toh(addr_ptr[1]) | (((uint64_t)be32toh(addr_ptr[0])) << 32);
                } else {
                  // Invalid number of address cells
                }
	}
	reg_prop += 4 * address_cells;
	if (size_cells > 0) {
		if (size_cells == 1) {
                  reg->size = be32toh(*((uint32_t *)reg_prop));
                }
                else if (size_cells == 2) {
                  uint32_t *size_ptr = (uint32_t*)reg_prop;
                  reg->size = (uint64_t)be32toh(size_ptr[1]) | (((uint64_t)be32toh(size_ptr[0])) << 32);
                } else {
                  // Invalid number of size cells
                }
	}

	return 0;
}

int fdt_getreg_array(const void *fdt, int offset, fdt_reg_t *regs, int *num) {
 
	int reg_lenp = 0;
        const char *reg_prop = fdt_getprop(fdt, offset, "reg", &reg_lenp);

        if(reg_prop == NULL) {
          return -1;
        }

        if(regs == NULL) {
          return -1;
        }

	const void *address_cells_p = NULL;
	const void *size_cells_p = NULL;
	int parent_iter_offset = offset;
	while (!address_cells_p || !size_cells_p) {
		// unclear if we should return here or if we should fall back to the default values
		// listed above (2, 1)
		if (parent_iter_offset < 0) {
			return -1;
		}
		int lenp = 0;

		address_cells_p = fdt_getprop(fdt, parent_iter_offset, "#address-cells", &lenp);
		size_cells_p = fdt_getprop(fdt, parent_iter_offset, "#size-cells", &lenp);

		parent_iter_offset = fdt_parent_offset(fdt, parent_iter_offset);
	}

	uint32_t address_cells = be32toh(*((uint32_t *)address_cells_p));
	uint32_t size_cells = be32toh(*((uint32_t *)size_cells_p));

	//printk("Addr: %d, size: %d\n", address_cells, size_cells);

        int num_present = (reg_lenp) / ((4*address_cells) + (4*size_cells));
        *num = num_present > *num ? *num : num_present; // minimum

	for(int i = 0; i < *num; i++) {
	    if (address_cells > 0) {
		if (address_cells == 1) {
                  regs[i].address = be32toh(*((uint32_t *)reg_prop));
                }
                else if (address_cells == 2) {
                  uint32_t *addr_ptr = (uint32_t*)reg_prop;
                  regs[i].address = (uint64_t)be32toh(addr_ptr[1]) | (((uint64_t)be32toh(addr_ptr[0])) << 32);
                } else {
                  // Invalid number of address cells
                }
	    }
	    reg_prop += 4 * address_cells;
	    if (size_cells > 0) {
		if (size_cells == 1) {
                  regs[i].size = be32toh(*((uint32_t *)reg_prop));
                }
                else if (size_cells == 2) {
                  uint32_t *size_ptr = (uint32_t*)reg_prop;
                  regs[i].size = (uint64_t)be32toh(size_ptr[1]) | (((uint64_t)be32toh(size_ptr[0])) << 32);
                } else {
                  // Invalid number of size cells
                }
	    }
            reg_prop += 4 * size_cells;
        }

	return 0; 
}


off_t fdt_getreg_address(const void *fdt, int offset) {
	fdt_reg_t reg = { .address = 0, .size = 0 };
    int getreg_result = fdt_getreg(fdt, offset, &reg);

	return reg.address;
}

// TODO: this makes a lot of assumptions about the device:
// it only grabs the first interrupt, and it assumes that
// the #interrupt-cells property is 1 (32 bit)
uint32_t fdt_get_interrupt(const void *fdt, int offset) {
	int lenp = 0;

	const void *ints = fdt_getprop(fdt, offset, "interrupts", &lenp);
	const uint32_t *vals = (uint32_t *)ints;

	return be32toh(vals[0]);
}

int print_device(const void *fdt, int offset, int depth) {
	int lenp = 0;
	const char *name = fdt_get_name(fdt, offset, &lenp);
	const char *compat_prop = fdt_getprop(fdt, offset, "compatible", &lenp);
	const char *status = fdt_getprop(fdt, offset, "status", &lenp);
	// show tree depth with spaces
	for (int i = 0; i < depth; i++) {
		printk("  ");
	}
	printk("%s (%s - %s)\n", name, compat_prop, status);

	const void *ints = fdt_getprop(fdt, offset, "interrupts", &lenp);
	uint32_t *vals = (uint32_t *)ints;
	for (int i = 0; i < lenp / 4; i++) {
		uint32_t val = be32toh(vals[i]);
		printk("\tInt: %d\n", val);
	}

	if (compat_prop && strcmp(compat_prop, "sifive,plic-1.0.0") == 0) {
		const void *ints_extended_prop = fdt_getprop(fdt, offset, "interrupts-extended", &lenp);
		if (ints_extended_prop != NULL) {
			uint32_t *vals = (uint32_t *)ints_extended_prop;
			int context_count = lenp / 8;
			// printk("\tlenp: %d, context count %d\n",
			// lenp, context_count);
			for (int context = 0; context < context_count; context++) {
				int c_off = context * 2;
				int phandle = be32toh(vals[c_off]);
				int nr = be32toh(vals[c_off + 1]);
				if (nr != 9) {
					continue;
				}
				printk("\tcontext %d: (%d, %d)\n", context,
						phandle, nr);

				int intc_offset = fdt_node_offset_by_phandle(fdt, phandle);
				int cpu_offset = fdt_parent_offset(fdt, intc_offset);
				const char *name = fdt_get_name(fdt, cpu_offset, &lenp);
				printk("\tcpu: %s\n", name);
			}
		}
	}

	return 1;
}

void print_fdt(const void *fdt) {
	fdt_walk_devices(fdt, print_device);
}

void fdt_walk_devices(const void *fdt, int (*callback)(const void *fdt, int offset, int depth)) {
	int depth = 0;
	int offset = 0;
	do {
		if (!callback(fdt, offset, depth)) {
			break;
		}

		offset = fdt_next_node(fdt, offset, &depth);
	} while (offset > 0);
}

