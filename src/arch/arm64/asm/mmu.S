
.GLOBAL __mmu_enable

.section .text

// We want this function to fit into one cache line
// (And for instruction caches to be enabled without the MMU)
.align 6
__mmu_enable: 

  isb
  dsb SY
  msr SCTLR_EL1, x0
  isb
  dsb SY
  ret

