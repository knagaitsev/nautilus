#ifndef __ARCH64_SMCCC_H__
#define __ARCH64_SMCCC_H__

#define SMCCC_CALL_TYPE_ARCH 0

#define SMCCC_ARCH_RET_SUCCESS       ( 0)
#define SMCCC_ARCH_RET_NOT_SUPPORTED (-1)
#define SMCCC_ARCH_RET_NOT_REQUIRED  (-2)
#define SMCCC_ARCH_RET_INVALID_PARAM (-3)

#define SMCCC_CALL_TYPE_CPU_SERV 1
#define SMCCC_CALL_TYPE_SIP_SERV 2
#define SMCCC_CALL_TYPE_OEM_SERV 3
#define SMCCC_CALL_TYPE_SS_SERV 4
#define SMCCC_CALL_TYPE_SH_SERV 5
#define SMCCC_CALL_TYPE_VSH_SERV 6

void smc64_call(
    uint16_t id,
    uint8_t type,
    uint64_t *args,
    uint64_t arg_count;
    uint64_t *rets, 
    uint64_t ret_count;
    );

void hvc64_call(
    uint16_t id,
    uint8_t type,
    uint64_t *args,
    uint64_t arg_count;
    uint64_t *rets, 
    uint64_t ret_count;
    );

void smc32_call(
    uint16_t id,
    uint8_t type,
    uint32_t *args,
    uint64_t arg_count;
    uint32_t *rets, 
    uint64_t ret_count;
    );

void hvc32_call(
    uint16_t id,
    uint8_t type,
    uint32_t *args,
    uint64_t arg_count;
    uint32_t *rets, 
    uint64_t ret_count;
    );

#endif
