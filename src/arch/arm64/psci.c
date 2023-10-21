
#include<arch/arm64/psci.h>

#include<arch/arm64/smccc.h>
#include<nautilus/nautilus.h>
#include<nautilus/shell.h>
#include<nautilus/of/fdt.h>

#ifndef NAUT_CONFIG_DEBUG_PRINTS
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

#define PSCI_PRINT(fmt, args...) INFO_PRINT("PSCI: " fmt, ##args)
#define PSCI_DEBUG(fmt, args...) DEBUG_PRINT("PSCI: " fmt, ##args)
#define PSCI_ERROR(fmt, args...) ERROR_PRINT("PSCI: " fmt, ##args)
#define PSCI_WARN(fmt, args...) WARN_PRINT("PSCI: " fmt, ##args)

static int psci_valid = 0;
static int use_hvc = 0;

#define CHECK_PSCI_VALID(RET) \
  do { \
    if(!psci_valid) { \
      PSCI_ERROR("PSCI Functions cannot be called on a system without PSCI!\n"); \
      return RET; \
    } \
  } while (0)

int psci_version(uint16_t *major, uint16_t *minor) {
  CHECK_PSCI_VALID(-1);
  uint32_t ver;
  if(use_hvc) { 
    uint32_t ver;
    hvc32_call(0x0, SMCCC_CALL_TYPE_SS_SERV, NULL, 0, &ver, 1);
  } else {
    uint32_t ver;
    smc32_call(0x0, SMCCC_CALL_TYPE_SS_SERV, NULL, 0, &ver, 1);
  }
  *major = (ver>>16) & 0xFFFF;
  *minor = ver & 0xFFFF;
  return 0;
}

int psci_cpu_on(uint64_t target_mpid, void *entry_point, uint64_t context_id) { 
  CHECK_PSCI_VALID(-1);
  uint64_t res;
  uint64_t args[3];
  args[0] = target_mpid;
  args[1] = (uint64_t)entry_point;
  args[2] = context_id;
  if(use_hvc) {
    hvc64_call(0x3, SMCCC_CALL_TYPE_SS_SERV, args, 3, &res, 1);
  } else {
    smc64_call(0x3, SMCCC_CALL_TYPE_SS_SERV, args, 3, &res, 1);
  }

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
  CHECK_PSCI_VALID(-1);
  uint32_t res;
  if(use_hvc) {
    hvc32_call(0x2, SMCCC_CALL_TYPE_SS_SERV, NULL, 0, &res, 1);
  } else {
    smc32_call(0x2, SMCCC_CALL_TYPE_SS_SERV, NULL, 0, &res, 1);
  }
  return res;
}
int psci_system_off(void) {
  smc32_call(0x8, SMCCC_CALL_TYPE_SS_SERV, NULL, 0, NULL, 0);
  return -1;
}
int psci_system_reset(void) {
  CHECK_PSCI_VALID(-1);
  if(use_hvc) {
    hvc32_call(0x9, SMCCC_CALL_TYPE_SS_SERV, NULL, 0, NULL, 0);
  } else {
    smc32_call(0x9, SMCCC_CALL_TYPE_SS_SERV, NULL, 0, NULL, 0);
  }
  return -1;
}

int psci_init(void *dtb) {

  int offset = fdt_node_offset_by_compatible(dtb, -1, "arm,psci");
  if(offset < 0) {
    offset = fdt_node_offset_by_compatible(dtb, -1, "arm,psci-0.2");
  }
  if(offset < 0) {
    offset = fdt_node_offset_by_compatible(dtb, -1, "arm,psci-1.0");
  }

  if(offset < 0) {
    PSCI_PRINT("Could not find PSCI support in the device tree!\n");
    psci_valid = 0;
    return -1;
  }

  int lenp;
  char *method_str = (char*)fdt_getprop(dtb, offset, "method", &lenp);
  if(method_str == NULL || lenp < 0) {
    PSCI_WARN("No method field in device tree node! Assuming SMC...\n");
    use_hvc = 0;
  } else if(lenp != 4) { 
    PSCI_WARN("Invalid method field found in device tree node (incorrect length)! Assuming SMC...\n");
    use_hvc = 0;
  } else {
    if(strncmp(method_str, "smc", 3) == 0) {
      PSCI_DEBUG("Method field found: smc\n");
      use_hvc = 0;
    } else if(strncmp(method_str, "hvc", 3) == 0) {
      PSCI_DEBUG("Method field found: hvc\n");
      use_hvc = 1;
    } else {
      PSCI_WARN("Invalid method field found in device tree node (strncmp failed)! Assuming SMC...\n");
      use_hvc = 0;
    }
  }

  psci_valid = 1;
  PSCI_PRINT("Method = %s\n", use_hvc ? "hvc" : "smc");

  uint16_t psci_major, psci_minor;
  psci_version(&psci_major, &psci_minor);
  PSCI_PRINT("Version = %u.%u\n", psci_major, psci_minor);

  PSCI_PRINT("Initialized PSCI\n");
  return 0;
}

static int
reboot_handler(char *buf, void *priv) {
  return psci_system_reset();
}

static int
off_handler(char *buf, void *priv) {
  return psci_system_off();
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
