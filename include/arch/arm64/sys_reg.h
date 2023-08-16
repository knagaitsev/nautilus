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

#define ASSERT_SYS_REG_SIZE(type, size) \
      _Static_assert(sizeof(type) == size, "System Register struct: " #type " is not exactly " #size " bytes wide!");

typedef union sctlr_el1 {
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
} sctlr_el1_t;
ASSERT_SYS_REG_SIZE(sctlr_el1_t, 8);

static inline void dump_sctlr_el1(sctlr_el1_t ctrl) {
#define PF(x) printk("\tSCTLR_EL1.%s = %u\n", #x, ctrl.x)
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
}

typedef union cpacr_el1 {
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
} cpacr_el1_t;
ASSERT_SYS_REG_SIZE(cpacr_el1_t, 8);

typedef union fpcr {
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
} fpcr_t;
ASSERT_SYS_REG_SIZE(fpcr_t, 8);

typedef union daif {
  uint64_t raw;
  struct {
    uint_t __resv1 : 6;
    uint_t fiq_masked : 1;
    uint_t irq_masked : 1;
    uint_t serror_masked : 1;
    uint_t debug_masked : 1;
    // Rest reserved
  };
} daif_t;
ASSERT_SYS_REG_SIZE(daif_t, 8);

typedef union mpidr_el1 {
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
} mpidr_el1_t;
ASSERT_SYS_REG_SIZE(mpidr_el1_t, 8);

// Returns -1 on error
cpu_id_t mpidr_el1_to_cpu_id(mpidr_el1_t *mpid);

// Translation Control Registers

typedef union tcr_el1 {
  uint64_t raw;
  struct {
    uint_t t0sz : 6;
    uint_t __res0_0 : 1;
    uint_t ttbr0_do_not_walk : 1;
    uint_t ttbr0_table_inner_cacheability : 2;
    uint_t ttbr0_table_outer_cacheability : 2;
    uint_t ttbr0_table_shareability : 2;
    uint_t ttbr0_granule_size : 2;

    uint_t t1sz : 6;
    uint_t ttbr1_defines_asid : 1;
    uint_t ttbr1_do_not_walk : 1;
    uint_t ttbr1_table_inner_cacheability : 2;
    uint_t ttbr1_table_outer_cacheability : 2;
    uint_t ttbr1_table_shareability : 2;
    uint_t ttbr1_granule_size : 2;

    uint_t ips : 3;
    uint_t __res0_1 : 1;
    uint_t asid_16bit : 1;
    uint_t ttbr0_top_byte_ignored : 1;
    uint_t ttbr1_top_byte_ignored : 1;

    uint_t hw_update_access_flag : 1;
    uint_t hw_update_dirty_bit : 1;

    uint_t ttbr0_disable_hierarchy : 1;
    uint_t ttbr1_disable_hierarchy : 1;

    uint_t ttbr0_desc_bit_59_hw_use_en : 1;
    uint_t ttbr0_desc_bit_60_hw_use_en : 1;
    uint_t ttbr0_desc_bit_61_hw_use_en : 1;
    uint_t ttbr0_desc_bit_62_hw_use_en : 1;

    uint_t ttbr1_desc_bit_59_hw_use_en : 1;
    uint_t ttbr1_desc_bit_60_hw_use_en : 1;
    uint_t ttbr1_desc_bit_61_hw_use_en : 1;
    uint_t ttbr1_desc_bit_62_hw_use_en : 1;

    uint_t ttbr0_top_byte_ignored_data_only : 1;
    uint_t ttbr1_top_byte_ignored_data_only : 1;

    uint_t ttbr0_non_fault_translation_timing_disable : 1;
    uint_t ttbr1_non_fault_translation_timing_disable : 1;

    uint_t ttbr0_unpriv_access_translation_fault : 1;
    uint_t ttbr1_unpriv_access_translation_fault : 1;

    uint_t ttbr0_unchecked_accesses : 1;
    uint_t ttbr1_unchecked_accesses : 1;

    uint_t output_addr_52 : 1;
    
    uint_t ttbr0_ext_mem_tag_check : 1;
    uint_t ttbr1_ext_mem_tag_check : 1;

    uint_t __res0_2 : 2;
  };
} tcr_el1_t;
ASSERT_SYS_REG_SIZE(tcr_el1_t, 8);

static void dump_tcr_el1(tcr_el1_t tc) {
#define PF(field) printk("\tTCR_EL1." #field " = 0x%x\n", tc.field) 

    PF(t0sz);
    PF(ttbr0_do_not_walk);
    PF(ttbr0_table_inner_cacheability);
    PF(ttbr0_table_outer_cacheability);
    PF(ttbr0_table_shareability);
    PF(ttbr0_granule_size);
    PF(t1sz);
    PF(ttbr1_defines_asid);
    PF(ttbr1_do_not_walk);
    PF(ttbr1_table_inner_cacheability);
    PF(ttbr1_table_outer_cacheability);
    PF(ttbr1_table_shareability);
    PF(ttbr1_granule_size);
    PF(ips);
    PF(asid_16bit);
    PF(ttbr0_top_byte_ignored);
    PF(ttbr1_top_byte_ignored);
    PF(hw_update_access_flag);
    PF(hw_update_dirty_bit);
    PF(ttbr0_disable_hierarchy);
    PF(ttbr1_disable_hierarchy);
    PF(ttbr0_desc_bit_59_hw_use_en);
    PF(ttbr0_desc_bit_60_hw_use_en);
    PF(ttbr0_desc_bit_61_hw_use_en);
    PF(ttbr0_desc_bit_62_hw_use_en);
    PF(ttbr1_desc_bit_59_hw_use_en);
    PF(ttbr1_desc_bit_60_hw_use_en);
    PF(ttbr1_desc_bit_61_hw_use_en);
    PF(ttbr1_desc_bit_62_hw_use_en);
    PF(ttbr0_top_byte_ignored_data_only);
    PF(ttbr1_top_byte_ignored_data_only);
    PF(ttbr0_non_fault_translation_timing_disable);
    PF(ttbr1_non_fault_translation_timing_disable);
    PF(ttbr0_unpriv_access_translation_fault);
    PF(ttbr1_unpriv_access_translation_fault);
    PF(ttbr0_unchecked_accesses);
    PF(ttbr1_unchecked_accesses);
    PF(output_addr_52);
    PF(ttbr0_ext_mem_tag_check);
    PF(ttbr1_ext_mem_tag_check);

#undef PF
}

typedef union tcr2_el1 {
  uint64_t raw;
  struct {
    uint_t prot_attr_en : 1;
    uint_t indirect_perm_model : 1;
    uint_t perm_overlay_el0_en : 1;
    uint_t perm_overlay_el1_en : 1;
    uint_t attr_indexing_en : 1;
    uint_t d128_en : 1;
    uint_t __res0_0 : 4;
    uint_t permit_table_walk_incoherence : 1;
    uint_t hw_access_flag_en : 1;
    uint_t ttbr0_contig_disable : 1;
    uint_t ttbr1_contig_disable : 1;
    // Rest res0
  };
} tcr2_el1_t;
ASSERT_SYS_REG_SIZE(tcr2_el1_t, 8);

static void dump_tcr2_el1(tcr2_el1_t tc) {
#define PF(field) printk("\tTCR2_EL1." #field " = 0x%x\n", tc.field) 
    PF(prot_attr_en);
    PF(indirect_perm_model);
    PF(perm_overlay_el0_en);
    PF(perm_overlay_el1_en);
    PF(attr_indexing_en);
    PF(d128_en);
    PF(permit_table_walk_incoherence);
    PF(hw_access_flag_en);
    PF(ttbr0_contig_disable);
    PF(ttbr0_contig_disable);
#undef PF
}

typedef union mair_el1 {
  uint64_t raw;
  uint8_t attrs[8];
} mair_el1_t;
ASSERT_SYS_REG_SIZE(mair_el1_t, 8);

typedef union ttbr0_el1 {
  uint64_t raw;
  struct {
    uint_t common_not_private : 1;
    ulong_t base_addr : 47;
    uint_t asid : 16;
  };
} ttbr0_el1_t;
ASSERT_SYS_REG_SIZE(ttbr0_el1_t, 8);

int ttbr0_el1_set_base_addr(ttbr0_el1_t*, void*);

typedef union ttbr1_el1 {
  uint64_t raw;
  struct {
    uint_t common_not_private : 1;
    ulong_t base_addr : 47;
    uint_t asid : 16;
  };
} ttbr1_el1_t;

int ttbr1_el1_set_base_addr(ttbr1_el1_t*, void*);

// Memory Model Feature Registers
typedef union id_aa64mmfr0_el1 {
  uint64_t raw;
  struct {
    uint_t pa_range : 4;
    uint_t asid_bits : 4;
    uint_t mixed_endian_sup : 4;
    uint_t secure_memory_distinct_sup : 4;
    uint_t mixed_endian_el0_sup : 4;
    uint_t tgran_16kb : 4;
    uint_t tgran_64kb : 4;
    uint_t tgran_4kb  : 4;
    uint_t tgran_2_16kb : 4;
    uint_t tgran_2_64kb : 4;
    uint_t tgran_2_4kb  : 4;
    uint_t non_ctx_sync_excp_sup : 4;
  };
} id_aa64mmfr0_el1_t;
ASSERT_SYS_REG_SIZE(id_aa64mmfr0_el1_t, 8);

typedef union id_aa64mmfr1_el1 {
  uint64_t raw;
  struct {
    uint_t hw_update_acc_dirty_flag_sup : 4;
    uint_t vmid_id_bits : 4;
    uint_t vhe_sup : 4;
    uint_t hier_perm_disable_sup : 4;
    uint_t loregions_sup : 4;
    uint_t priv_acc_never_sup : 4;
    uint_t spec_read_serror : 4;
    uint_t exec_never_stage_2_sup : 4;
    uint_t config_delayed_wfe_trap_sup : 4;
    uint_t enhanced_translation_sync_sup : 4;
    uint_t hcrx_el2_sup : 4;
    uint_t alt_fp_sup : 4;
    uint_t intermediate_tlb_walk_caching_sup : 4;
    uint_t trap_impl_def_el1_sup : 4;
    uint_t cache_maint_perm_sup : 4;
    uint_t restricted_branch_hist_spec_about_excp_sup : 4;
  };
} id_aa64mmfr1_el1_t;
ASSERT_SYS_REG_SIZE(id_aa64mmfr1_el1_t, 8);

typedef union id_aa64mmfr2_el1 {
  uint64_t raw;
  struct {
    uint_t commmon_not_private_sup : 4;
    uint_t user_access_override_sup : 4;
    uint_t lsm_sup : 4;
    uint_t iesb_sup : 4;
    uint_t va_range : 4;
    uint_t revised_ccsidr_sup : 4;
    uint_t nested_virt_sup : 4;
    uint_t small_translation_tables_sup : 4;
    uint_t unaligned_single_copy_atomic_sup : 4;
    uint_t __ids : 4;
    uint_t fwb_sup : 4;
    uint_t __res0_0 : 4;
    uint_t ttl_sup : 4;
    uint_t break_before_make_sup : 4;
    uint_t enhanced_virt_traps_sup : 4;
    uint_t e0pd_sup : 4;
  };
} id_aa64mmfr2_el1_t;
ASSERT_SYS_REG_SIZE(id_aa64mmfr2_el1_t, 8);

typedef union id_aa64mmfr3_el1 {
  uint64_t raw;
  struct {
    uint_t ext_tcr_sup : 4;
    uint_t ext_sctlr_sup : 4;
    uint_t __res0_0 : 4;
    uint_t __res0_1 : 4;
    uint_t __res0_2 : 4;
    uint_t __res0_3 : 4;
    uint_t __res0_4 : 4;
    uint_t mem_encrypt_ctx_sup : 4;
    uint_t __res0_5 : 4;
    uint_t __res0_6 : 4;
    uint_t __res0_7 : 4;
    uint_t __res0_8 : 4;
    uint_t __res0_9 : 4;
    uint_t __res0_10 : 4;
    uint_t __res0_11 : 4;
    uint_t spec_fpacc : 4;
  };
} id_aa64mmfr3_el1_t;
ASSERT_SYS_REG_SIZE(id_aa64mmfr3_el1_t, 8);

#endif
