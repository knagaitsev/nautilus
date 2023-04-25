#ifndef __ARM64_CTRL_REG_H__
#define __ARM64_CTRL_REG_H__

#include<nautilus/naut_types.h>

#define LOAD_SYS_REG(name, raw) \
  __asm__ __volatile__ (\
      "mrs %0,"#name : "=r" (raw) ::)

#define STORE_SYS_REG(name, raw) \
  __asm__ __volatile__ (\
      "msr "#name", %0" :: "r" (raw) :)

typedef union feat_acc_ctrl_reg {
  uint64_t raw;
  struct {
    uint_t __resv1 : 16;
    uint_t sve_enabled : 2;
    uint_t __resv2 : 2;
    uint_t fp_enabled : 2;
    uint_t __resv3 : 6;
    uint_t tta : 1;
    // Rest reserved
  };
} feat_acc_ctrl_reg_t;

inline static void load_feat_acc_ctrl_reg(feat_acc_ctrl_reg_t *reg) {
  LOAD_SYS_REG(CPACR_EL1, reg->raw);  
}
inline static void store_feat_acc_ctrl_reg(feat_acc_ctrl_reg_t *reg) {
  STORE_SYS_REG(CPACR_EL1, reg->raw);
}

typedef union fp_ctrl_reg {
  uint64_t raw;
  struct {
    uint_t flush_inp_zero : 1;
    uint_t alt_handling : 1;
    uint_t nep : 1;
    uint_t __resv1 : 5;
    uint_t inv_op_excp_enabled : 1;
    uint_t div_zero_excp_enabled : 1;
    uint_t overflow_excp_enabled : 1;
    uint_t underflow_excp_enabled : 1;
    uint_t inexact_excp_enabled : 1;
    uint_t __resv2 : 2;
    uint_t inp_denorm_excp_enabled : 1;
    uint_t __len : 3;
    uint_t flush_denorm_zero_16 : 1;
    uint_t __stride : 2;
    uint_t rounding_mode : 2;
    uint_t flush_denorm_zero : 1;
    uint_t prop_default_nan : 1;
    uint_t alt_handling_16 : 1;
    // Rest reserved
  };
} fp_ctrl_reg_t;

inline static void load_fp_ctrl_reg(fp_ctrl_reg_t *reg) {
  LOAD_SYS_REG(FPCR, reg->raw);
}
inline static void store_fp_ctrl_reg(fp_ctrl_reg_t *reg) {
  STORE_SYS_REG(FPCR, reg->raw);
}

#endif
