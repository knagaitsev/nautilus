// No include guard on purpose

#include<nautilus/naut_types.h>

// This structure definition can be guarded
#ifndef __ARM64_SMCCC_FUNC_ID_T__
#define __ARM64_SMCCC_FUNC_ID_T__
typedef union smccc_func_id {
  uint32_t raw;
  struct {
    uint_t id : 16;
    uint_t mbz : 8;
    uint_t service_call_range : 6;
    uint_t smccc64 : 1; // 0 -> smccc32
    uint_t fast_call : 1; // 0 -> Yielding call
  };
} smccc_func_id_t;

#define _InnerStringify(M) #M
#define _Stringify(M) _InnerStringify(M)

#endif

#ifndef SMCCC_DECL_NUM_RET
#error "Number of returns not specified in SMCCC function declaration!"
#endif

#ifndef SMCCC_DECL_NUM_ARGS
#error "Number of arguments not specified in SMCCC function declaration!"
#endif

#ifndef SMCCC_INSTRUCTION_USED
#error "SMCCC Instruction was not specified in declaration!"
#endif

#ifdef SMCCC_DECL_64
#define __SMCCC_BITS 64
#define __SMCCC_TYPE uint64_t
#define __SMCCC_REG_PREFIX "x"
#define __SMCCC_STR_POSTFIX ""
#else
#define __SMCCC_BITS 32
#define __SMCCC_TYPE uint32_t
#define __SMCCC_REG_PREFIX "w"
#define __SMCCC_STR_POSTFIX "h"
#endif

#define __SMCCC_CLOBBER_LIST "x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7", "x8", \
                           "x9", "x10", "x11", "x12", "x13", "x14", "x15", "x16", "x17"

#ifndef __SMCCC_CREATE_NAME
#define __SMCCC_CREATE_NAME_EXPAND(instr, bits, inner, ret, args) instr ## bits ## inner ## ret ## _ ## args
#define __SMCCC_CREATE_NAME(instr,inner,bits,ret,args) __SMCCC_CREATE_NAME_EXPAND(instr,inner,bits,ret,args)
#endif


static inline __SMCCC_TYPE __SMCCC_CREATE_NAME(SMCCC_INSTRUCTION_USED,__SMCCC_BITS,_call_,SMCCC_DECL_NUM_RET,SMCCC_DECL_NUM_ARGS)(
    uint32_t id,
    uint8_t type
// Returns
#if SMCCC_DECL_NUM_RET >= 1
    , __SMCCC_TYPE *ret0
#endif
#if SMCCC_DECL_NUM_RET >= 2
    , __SMCCC_TYPE *ret1
#endif
#if SMCCC_DECL_NUM_RET >= 3
    , __SMCCC_TYPE *ret2
#endif
#if SMCCC_DECL_NUM_RET >= 4
    , __SMCCC_TYPE *ret3
#endif

// Arguments
#if SMCCC_DECL_NUM_ARGS >= 1
    , __SMCCC_TYPE arg0
#endif
#if SMCCC_DECL_NUM_ARGS >= 2
    , __SMCCC_TYPE arg1
#endif
#if SMCCC_DECL_NUM_ARGS >= 3
    , __SMCCC_TYPE arg2
#endif
#if SMCCC_DECL_NUM_ARGS >= 4
    , __SMCCC_TYPE arg3
#endif
#if SMCCC_DECL_NUM_ARGS >= 5
    , __SMCCC_TYPE arg4
#endif
#if SMCCC_DECL_NUM_ARGS >= 6
    , __SMCCC_TYPE arg5
#endif

) {
#if SMCCC_DECL_NUM_RET == 0
      __SMCCC_TYPE ret;
      __SMCCC_TYPE *ret0 = &ret;
#endif
      smccc_func_id_t _id;
      _id.id = id & 0xFFFF;
      _id.mbz = 0;
      _id.service_call_range = type;
      _id.smccc64 = 
#ifdef SMCCC_DECL_64
        1;
#else
        0;
#endif
      _id.fast_call = 1; // Atomic (out of laziness/speed of development not necessity)


  asm volatile (
      "mov w0, %w[raw_id];"
#if SMCCC_DECL_NUM_ARGS >= 1
      "mov "   __SMCCC_REG_PREFIX   "1, %" __SMCCC_REG_PREFIX "[arg0];"
#endif
#if SMCCC_DECL_NUM_ARGS >= 2
      "mov "   __SMCCC_REG_PREFIX   "2, %" __SMCCC_REG_PREFIX "[arg1];"
#endif
#if SMCCC_DECL_NUM_ARGS >= 3
      "mov "   __SMCCC_REG_PREFIX   "3, %" __SMCCC_REG_PREFIX "[arg2];"
#endif
#if SMCCC_DECL_NUM_ARGS >= 4
      "mov "   __SMCCC_REG_PREFIX   "4, %" __SMCCC_REG_PREFIX "[arg3];"
#endif
#if SMCCC_DECL_NUM_ARGS >= 5
      "mov "   __SMCCC_REG_PREFIX   "5, %" __SMCCC_REG_PREFIX "[arg4];"
#endif
#if SMCCC_DECL_NUM_ARGS >= 6
      "mov "   __SMCCC_REG_PREFIX   "6, %" __SMCCC_REG_PREFIX "[arg5];"
#endif
      "" _Stringify(SMCCC_INSTRUCTION_USED) " 0;"
      "str" __SMCCC_STR_POSTFIX " " __SMCCC_REG_PREFIX "0, %" __SMCCC_REG_PREFIX "[ret0];"
#if SMCCC_DECL_NUM_RET >= 2
      "str" __SMCCC_STR_POSTFIX " " __SMCCC_REG_PREFIX "1, %" __SMCCC_REG_PREFIX "[ret1];"
#endif
#if SMCCC_DECL_NUM_RET >= 3
      "str" __SMCCC_STR_POSTFIX " " __SMCCC_REG_PREFIX "2, %" __SMCCC_REG_PREFIX "[ret2];"
#endif
#if SMCCC_DECL_NUM_RET >= 4
      "str" __SMCCC_STR_POSTFIX " " __SMCCC_REG_PREFIX "3, %" __SMCCC_REG_PREFIX "[ret3];"
#endif
    : // Outputs
    [ret0] "=m" (*ret0)
#if SMCCC_DECL_NUM_RET >= 2
    , [ret1] "=m" (*ret1)
#endif
#if SMCCC_DECL_NUM_RET >= 3
    , [ret2] "=m" (*ret2)
#endif
#if SMCCC_DECL_NUM_RET >= 4
    , [ret3] "=m" (*ret3)
#endif
    : //Inputs 
    [raw_id] "r" (_id.raw)
#if SMCCC_DECL_NUM_ARGS >= 1
    , [arg0] "r" (arg0)
#endif
#if SMCCC_DECL_NUM_ARGS >= 2
    , [arg1] "r" (arg1)
#endif
#if SMCCC_DECL_NUM_ARGS >= 3
    , [arg2] "r" (arg2)
#endif
#if SMCCC_DECL_NUM_ARGS >= 4
    , [arg3] "r" (arg3)
#endif
#if SMCCC_DECL_NUM_ARGS >= 5
    , [arg4] "r" (arg4)
#endif
#if SMCCC_DECL_NUM_ARGS >= 6
    , [arg5] "r" (arg5)
#endif
    : __SMCCC_CLOBBER_LIST);

  return *ret0;
} 

#undef SMCCC_DECL_NAME
#undef __SMCCC_BITS
#undef __SMCCC_TYPE
#undef __SMCCC_CLOBBER_LIST
#undef __SMCCC_STR_POSTFIX
#undef __SMCCC_REG_PREFIX
