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

#define SMCCC_INSTRUCTION_USED smc
#define SMCCC_DECL_64
// smc64_call_n_n()
#include "./smccc/permute.h"
#undef SMCCC_DECL_64
// smc32_call_n_n()
#include "./smccc/permute.h"

#undef SMCCC_INSTRUCTION_USED
#define SMCCC_INSTRUCTION_USED hvc
#define SMCCC_DECL_64
// hvc64_call_n_n()
#include "./smccc/permute.h"
#undef SMCCC_DECL_64
// hvc32_call_n_n()
#include "./smccc/permute.h"

#endif
