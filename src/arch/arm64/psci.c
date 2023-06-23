
#include<arch/arm64/psci.h>

#include<arch/arm64/smc.h>
#include<nautilus/nautilus.h>


#ifndef NAUT_CONFIG_DEBUG_PRINTS
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

#define PSCI_PRINT(fmt, args...) INFO_PRINT("PSCI: " fmt, ##args)
#define PSCI_DEBUG(fmt, args...) DEBUG_PRINT("PSCI: " fmt, ##args)
#define PSCI_ERROR(fmt, args...) ERROR_PRINT("PSCI: " fmt, ##args)
#define PSCI_WARN(fmt, args...) WARN_PRINT("PSCI: " fmt, ##args)

void psci_version(uint16_t *major, uint16_t *minor) {
  uint32_t ver = smc32_call_0_0(0x84000000, SMC_CALL_TYPE_SS_SERV);
  *major = (ver>>16) & 0xFFFF;
  *minor = ver & 0xFFFF;
}

int psci_cpu_on(uint64_t target_mpid, void *entry_point, uint64_t context_id) {
  uint64_t res = smc64_call_0_3(0xC4000003, SMC_CALL_TYPE_SS_SERV, target_mpid, (uint64_t)entry_point, context_id);

  switch(res) {
    case PSCI_SUCCESS:
      PSCI_DEBUG("psci_cpu_on: SUCCESS\n");
      break;
    case PSCI_INVALID_PARAMETERS:
      PSCI_ERROR("psci_cpu_on: invalid parameters!\n");
      break;
    case PSCI_INVALID_ADDRESS:
      PSCI_ERROR("psci_cpu_on: invalid address!\n");
      break;
    case PSCI_ALREADY_ON:
      PSCI_WARN("psci_cpu_on: cpu is already on!\n");
      break;
    case PSCI_ON_PENDING:
      PSCI_ERROR("psci_cpu_on: cpu is pending!\n");
      break;
    case PSCI_INTERNAL_FAILURE:
      PSCI_ERROR("psci_cpu_on: internal failure!\n");
      break;
    case PSCI_DENIED:
      PSCI_ERROR("psci_cpu_on: permission denied!\n");
      break;
    default:
      PSCI_ERROR("psci_cpu_on: unknown error!\n");
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

int psci_init(void) {
  // This function is mostly for future proofing (and testing psci functionality)

  uint16_t psci_major, psci_minor;
  psci_version(&psci_major, &psci_minor);
  PSCI_PRINT("Version = %u.%u\n", psci_major, psci_minor);

  PSCI_PRINT("Initialized PSCI\n");
}

#include<nautilus/shell.h>

static int
reboot_handler(char *buf, void *priv) {
  psci_system_reset();

  // Returning from this is an error
  return 1;
}

static int
off_handler(char *buf, void *priv) {
  psci_system_off();

  // Returning from this is an error
  return 1;
}

static struct shell_cmd_impl reboot_impl = {
    .cmd = "reboot",
    .help_str = "reboot",
    .handler = reboot_handler,
};
nk_register_shell_cmd(reboot_impl);

static struct shell_cmd_impl off_impl = {
    .cmd = "off",
    .help_str = "off",
    .handler = off_handler,
};
nk_register_shell_cmd(off_impl);

