#ifndef __OF_8250_H__
#define __OF_8250_H__

#ifdef NAUT_CONFIG_OF_8250_EARLY_OUTPUT
int of_8250_pre_vc_init(void *fdt);
#endif

int of_8250_init(void);

#endif
