
.GLOBAL arm64_mmu_enable

.section .text

.align 6
arm64_mmu_enable:  
  dsb SY
  tlbi VMALLE1
  dsb SY
  isb
  mrs x0, SCTLR_EL1
  orr x0, x0, 1 // M = 1
  msr SCTLR_EL1, x0
  dsb SY
  tlbi VMALLE1
  dsb SY
  isb
  ret

