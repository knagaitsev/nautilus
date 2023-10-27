#ifndef __NAUT_ARM64_ARCH_H__
#define __NAUT_ARM64_ARCH_H__

#ifndef NAUT_CONFIG_SPARSE_IRQ
#error "ARM64 requires that NAUT_CONFIG_SPARSE_IRQ is enabled!"
#endif

#define MAX_IRQ_NUM 0
#define MAX_IVEC_NUM 0

#endif
