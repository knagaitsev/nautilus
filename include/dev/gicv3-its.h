#ifndef __GICV3_ITS_H__
#define __GICV3_ITS_H__

int gicv3_its_init(uint64_t dtb, struct gicv3 *gic);
int gicv3_its_ivec_init(struct gicv3 *gic);

#endif
