#ifndef __NAUT_ARM64_ARCH_H__
#define __NAUT_ARM64_ARCH_H__

#ifndef NAUT_CONFIG_SPARSE_IRQ_VECTORS
#error "ARM64 requires that SPARSE_IRQ_VECTORS is enabled!"
#endif

#ifndef NAUT_CONFIG_IDENTITY_MAP_IRQ_VECTORS
#error "ARM64 requires that IDENTITY_MAP_IRQ_VECTORS is enabled!"
#endif

#define MAX_IRQ_NUM 0
#define MAX_IVEC_NUM 0

#endif
