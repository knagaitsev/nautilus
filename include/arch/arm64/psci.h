#ifndef __ARM64_PSCI_H__
#define __ARM64_PSCI_H__

#include<nautilus/naut_types.h>

#define PSCI_SUCCESS 0
#define PSCI_NOT_SUPPORTED (-1)
#define PSCI_INVALID_PARAMETERS (-2)
#define PSCI_DENIED (-3)
#define PSCI_ALREADY_ON (-4)
#define PSCI_ON_PENDING (-5)
#define PSCI_INTERNAL_FAILURE (-6)
#define PSCI_NOT_PRESENT (-7)
#define PSCI_DISABLED (-8)
#define PSCI_INVALID_ADDRESS (-9)

int psci_init(void *dtb);
int psci_version(uint16_t *major, uint16_t *minor);
int psci_cpu_on(uint64_t target_mpid, void *entry_point, uint64_t context_id);

/*
#define PSCI_AFFINITY_INFO_ON 0
#define PSCI_AFFINITY_INFO_OFF 1
#define PSCI_AFFINITY_INFO_ON_PENDING 2
// INVALID_PARAMETERS and DISABLED are also valid return values from this function
int psci_affinity_info(uint64_t target_mpid);
*/

// These functions can fail, so make sure the programmer doesn't ignore that possibility
int __attribute__((warn_unused_result)) psci_cpu_off(void);
int __attribute__((warn_usused_result)) psci_system_off(void);
int __attribute__((warn_unused_result)) psci_system_reset(void);

#endif
