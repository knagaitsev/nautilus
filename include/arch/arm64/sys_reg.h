#ifndef __ARM64_CTRL_REG_H__
#define __ARM64_CTRL_REG_H__

#include<nautilus/naut_types.h>
#include<nautilus/printk.h>

#define LOAD_SYS_REG(name, raw) \
  __asm__ __volatile__ (\
      "mrs %0,"#name : "=r" (raw) ::)

#define STORE_SYS_REG(name, raw) \
  __asm__ __volatile__ (\
      "msr "#name", %0" :: "r" (raw) :)

typedef union sys_ctrl_reg {
  uint64_t raw;
  struct {
    uint_t mmu_en : 1;
    uint_t align_check_en : 1;
    uint_t data_cacheability_ctrl : 1;
    uint_t sp_align_check_en : 1;
    uint_t sp_align_check_el0_en : 1;
    uint_t sys_instr_mem_barrier_en : 1;
    uint_t unaligned_acc_en : 1;
    uint_t el0_it_disable : 1;
    uint_t el0_setend_disable_el0 : 1;
    uint_t el0_int_mask_acc_en : 1;
    uint_t el0_cfp_en : 1;
    uint_t excp_exit_is_ctx_sync : 1;
    uint_t instr_cacheability_ctrl : 1;
    uint_t pointer_auth_en : 1;
    uint_t trap_el0_dczva_disable : 1;
    uint_t trap_el0_ctr_disable : 1;
    uint_t trap_el0_wfi_disable : 1;
    uint_t __res0_0 : 1;
    uint_t trap_el0_wfe_disable : 1;
    uint_t write_exec_never : 1;
    uint_t trap_el0_acc_scxtnum : 1;
    uint_t implicit_err_sync_event_en : 1;
    uint_t excp_entry_is_ctx_sync : 1;
    uint_t no_priv_change_on_el1_excp : 1;
    uint_t el0_big_endian : 1;
    uint_t el1_big_endian : 1;
    uint_t trap_el0_cache_maint : 1;
    uint_t el0_and_el1_pointer_auth_en : 1;
    // There are more but this is already more than we need for now
  };
} sys_ctrl_reg_t;

static inline void dump_sys_ctrl_reg(void) {
  sys_ctrl_reg_t ctrl;
  LOAD_SYS_REG(SCTLR_EL1, ctrl.raw);

  printk("\n");

#define PF(x) printk("\tSCTLR.%s = %u\n", #x, ctrl.x)

  PF(mmu_en);
  PF(align_check_en);
  PF(data_cacheability_ctrl);
  PF(sp_align_check_en);
  PF(sp_align_check_el0_en);
  PF(sys_instr_mem_barrier_en);
  PF(unaligned_acc_en);
  PF(el0_it_disable);
  PF(el0_setend_disable_el0);
  PF(el0_int_mask_acc_en);
  PF(el0_cfp_en);
  PF(excp_exit_is_ctx_sync);
  PF(instr_cacheability_ctrl);
  PF(pointer_auth_en);
  PF(trap_el0_dczva_disable);
  PF(trap_el0_ctr_disable);
  PF(trap_el0_wfi_disable);
  PF(__res0_0);
  PF(trap_el0_wfe_disable);
  PF(write_exec_never);
  PF(trap_el0_acc_scxtnum);
  PF(implicit_err_sync_event_en);
  PF(excp_entry_is_ctx_sync);
  PF(no_priv_change_on_el1_excp);
  PF(el0_big_endian);
  PF(el1_big_endian);
  PF(trap_el0_cache_maint);
  PF(el0_and_el1_pointer_auth_en);
#undef PF
  printk("\n");
}

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

typedef union int_mask_reg {
  uint64_t raw;
  struct {
    uint_t __resv1 : 6;
    uint_t fiq_masked : 1;
    uint_t irq_masked : 1;
    uint_t serror_masked : 1;
    uint_t debug_masked : 1;
    // Rest reserved
  };
} int_mask_reg_t;

inline static void load_int_mask_reg(int_mask_reg_t *reg) {
  LOAD_SYS_REG(DAIF, reg->raw);
}
inline static void store_int_mask_reg(int_mask_reg_t *reg) {
  STORE_SYS_REG(DAIF, reg->raw);
}

typedef union mpid_reg {
  uint64_t raw;
  struct {
    uint_t aff0 : 8;
    uint_t aff1 : 8;
    uint_t aff2 : 8;
    uint_t mt : 1;
    uint_t __resv1 : 5;
    uint_t uniproc : 1;
    uint_t __resv2 : 1;
    uint_t aff3 : 8;
    // Rest reserved
  };
} mpid_reg_t;

inline static void load_mpid_reg(mpid_reg_t *reg) {
  LOAD_SYS_REG(MPIDR_EL1, reg->raw);
}
inline static void store_mpid_reg(mpid_reg_t *reg) {
  STORE_SYS_REG(MPIDR_EL1, reg->raw);
}

#endif
