
#include<arch/arm64/psci.h>

#include<arch/arm64/smc.h>
#include<nautilus/nautilus.h>

void psci_version(uint16_t *major, uint16_t *minor) {
  uint32_t ver = smc32_call_0_0(0x84000000, SMC_CALL_TYPE_SS_SERV);
  *major = (ver>>16) & 0xFFFF;
  *minor = ver & 0xFFFF;
}

int psci_cpu_on(uint64_t target_mpid, void *entry_point, uint64_t context_id) {
  uint64_t res = smc64_call_0_3(0xC4000003, SMC_CALL_TYPE_SS_SERV, target_mpid, (uint64_t)entry_point, context_id);

  switch(res) {
    case PSCI_SUCCESS:
      printk("psci_cpu_on: SUCCESS\n");
      break;
    case PSCI_INVALID_PARAMETERS:
      printk("psci_cpu_on: invalid parameters!\n");
      break;
    case PSCI_INVALID_ADDRESS:
      printk("psci_cpu_on: invalid address!\n");
      break;
    case PSCI_ALREADY_ON:
      printk("psci_cpu_on: cpu is already on!\n");
      break;
    case PSCI_ON_PENDING:
      printk("psci_cpu_on: cpu is pending!\n");
      break;
    case PSCI_INTERNAL_FAILURE:
      printk("psci_cpu_on: internal failure!\n");
      break;
    case PSCI_DENIED:
      printk("psci_cpu_on: permission denied!\n");
      break;
    default:
      printk("psci_cpu_on: unknown error!\n");
      break;
  }
  return (int)res;
}

int psci_cpu_off(void) {
  uint32_t res = smc32_call_0_0(0x84000002, SMC_CALL_TYPE_SS_SERV);
  return res;
}
void psci_system_off(void) {
  smc32_call_0_0(0x84000008, SMC_CALL_TYPE_SS_SERV);
}
void psci_system_reset(void) {
  smc32_call_0_0(0x84000009, SMC_CALL_TYPE_SS_SERV);
}
