#ifndef __NAUT_FDT_H__
#define __NAUT_FDT_H__

#include<lib/libfdt.h>

/*
 * Nautilus' extensions to libfdt
 */

typedef struct fdt_reg {
	off_t address;
	size_t size;
} fdt_reg_t;

// returns 0 on success, anything else on failure
int fdt_getreg(const void *fdt, int offset, fdt_reg_t *reg);
int fdt_getreg_array(const void *fdt, int offset, fdt_reg_t *reg, int *num);

// NULL on failure (not ideal for the zero CPU)
off_t fdt_getreg_address(const void *fdt, int offset);

void print_fdt(const void *fdt);

// callback returns 0 if walking should stop, 1 otherwise
void fdt_walk_devices(const void *fdt, int (*callback)(const void *fdt, int offset, int depth));

int fdt_nodename_eq(const void *fdt, int offset, const char *s, int len);

#endif
