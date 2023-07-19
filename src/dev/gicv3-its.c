
#include<dev/gic.h>
#include<dev/gicv3.h>
#include<dev/gicv3-its.h>

#define GITS_CTLR_OFFSET  0x0
#define GITS_TYPER_OFFSET 0x8

#define GITS_LOAD_REG(offset, val)
#define GITS_STORE_REG(val, offset

struct gits_typer {
};

int gicv3_its_init(uint64_t dtb, struct gicv3 *gic) 
{

}

int gicv3_its_ivec_init(uint64_t dtb, struct gicv3 *gic) 
{

}

