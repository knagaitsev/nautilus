// No include guard on purpose

#include<nautilus/naut_types.h>

// This structure definition can be guarded
#ifndef __ARM64_SMC_FUNC_ID_T__
#define __ARM64_SMC_FUNC_ID_T__
typedef union smc_func_id {
  uint32_t raw;
  struct {
    uint_t id : 16;
    uint_t mbz : 8;
    uint_t service_call_range : 6;
    uint_t smc64 : 1; // 0 -> smc32
    uint_t fast_call : 1; // 0 -> Yielding call
  };
} smc_func_id_t;
#endif

#ifndef SMC_DECL_NAME
#error "No function name provided to smc function declaration!"
#endif

#ifndef SMC_DECL_NUM_RET
#error "Number of returns not specified in smc function declaration!"
#endif

#ifndef SMC_DECL_NUM_ARGS
#error "Number of arguments not specified in smc function declaration!"
#endif

#ifdef SMC_DECL_64
#define __SMC_TYPE uint64_t
#define __SMC_REG_PREFIX "x"
#define __SMC_STR_POSTFIX ""
#else
#define __SMC_TYPE uint32_t
#define __SMC_REG_PREFIX "w"
#define __SMC_STR_POSTFIX "h"
#endif

#ifdef NAUT_CONFIG_ARM64_HAS_SECURE_MONITOR
#define __SMC_INSTRUCTION_USED "smc"
#else
#define __SMC_INSTRUCTION_USED "hvc"
#endif

#define __SMC_CLOBBER_LIST "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", \
                           "x9", "x10", "x11", "x12", "x13", "x14", "x15", "x16", "x17"

static inline __SMC_TYPE SMC_DECL_NAME(
    uint32_t id,
    uint8_t type
// Returns
#if SMC_DECL_NUM_RET >= 1
    , __SMC_TYPE *ret0
#endif
#if SMC_DECL_NUM_RET >= 2
    , __SMC_TYPE *ret1
#endif
#if SMC_DECL_NUM_RET >= 3
    , __SMC_TYPE *ret2
#endif
#if SMC_DECL_NUM_RET >= 4
    , __SMC_TYPE *ret3
#endif

// Arguments
#if SMC_DECL_NUM_ARGS >= 1
    , __SMC_TYPE arg0
#endif
#if SMC_DECL_NUM_ARGS >= 2
    , __SMC_TYPE arg1
#endif
#if SMC_DECL_NUM_ARGS >= 3
    , __SMC_TYPE arg2
#endif
#if SMC_DECL_NUM_ARGS >= 4
    , __SMC_TYPE arg3
#endif
#if SMC_DECL_NUM_ARGS >= 5
    , __SMC_TYPE arg4
#endif
#if SMC_DECL_NUM_ARGS >= 6
    , __SMC_TYPE arg5
#endif

) {
#if SMC_DECL_NUM_RET == 0
      __SMC_TYPE ret;
      __SMC_TYPE *ret0 = &ret;
#endif
      smc_func_id_t _id;
      _id.id = id & 0xFFFF;
      _id.mbz = 0;
      _id.service_call_range = type;
      _id.smc64 = 
#ifdef SMC_DECL_64
        1;
#else
        0;
#endif
      _id.fast_call = 1; // Atomic (out of laziness/speed of development not necessity)


  asm volatile (
      "mov w0, %w[raw_id];"
#if SMC_DECL_NUM_ARGS >= 1
      "mov "   __SMC_REG_PREFIX   "1, %" __SMC_REG_PREFIX "[arg0];"
#endif
#if SMC_DECL_NUM_ARGS >= 2
      "mov "   __SMC_REG_PREFIX   "2, %" __SMC_REG_PREFIX "[arg1];"
#endif
#if SMC_DECL_NUM_ARGS >= 3
      "mov "   __SMC_REG_PREFIX   "3, %" __SMC_REG_PREFIX "[arg2];"
#endif
#if SMC_DECL_NUM_ARGS >= 4
      "mov "   __SMC_REG_PREFIX   "4, %" __SMC_REG_PREFIX "[arg3];"
#endif
#if SMC_DECL_NUM_ARGS >= 5
      "mov "   __SMC_REG_PREFIX   "5, %" __SMC_REG_PREFIX "[arg4];"
#endif
#if SMC_DECL_NUM_ARGS >= 6
      "mov "   __SMC_REG_PREFIX   "6, %" __SMC_REG_PREFIX "[arg5];"
#endif
      "" __SMC_INSTRUCTION_USED " 0;"
      "str" __SMC_STR_POSTFIX " " __SMC_REG_PREFIX "0, %" __SMC_REG_PREFIX "[ret0];"
#if SMC_DECL_NUM_RET >= 2
      "str" __SMC_STR_POSTFIX " " __SMC_REG_PREFIX "1, %" __SMC_REG_PREFIX "[ret1];"
#endif
#if SMC_DECL_NUM_RET >= 3
      "str" __SMC_STR_POSTFIX " " __SMC_REG_PREFIX "2, %" __SMC_REG_PREFIX "[ret2];"
#endif
#if SMC_DECL_NUM_RET >= 4
      "str" __SMC_STR_POSTFIX " " __SMC_REG_PREFIX "3, %" __SMC_REG_PREFIX "[ret3];"
#endif
    : // Outputs
    [ret0] "=m" (*ret0)
#if SMC_DECL_NUM_RET >= 2
    , [ret1] "=m" (*ret1)
#endif
#if SMC_DECL_NUM_RET >= 3
    , [ret2] "=m" (*ret2)
#endif
#if SMC_DECL_NUM_RET >= 4
    , [ret3] "=m" (*ret3)
#endif
    : //Inputs 
    [raw_id] "r" (_id.raw)
#if SMC_DECL_NUM_ARGS >= 1
    , [arg0] "r" (arg0)
#endif
#if SMC_DECL_NUM_ARGS >= 2
    , [arg1] "r" (arg1)
#endif
#if SMC_DECL_NUM_ARGS >= 3
    , [arg2] "r" (arg2)
#endif
#if SMC_DECL_NUM_ARGS >= 4
    , [arg3] "r" (arg3)
#endif
#if SMC_DECL_NUM_ARGS >= 5
    , [arg4] "r" (arg4)
#endif
#if SMC_DECL_NUM_ARGS >= 6
    , [arg5] "r" (arg5)
#endif
    : __SMC_CLOBBER_LIST);

  return *ret0;
} 

#undef SMC_DECL_NAME
