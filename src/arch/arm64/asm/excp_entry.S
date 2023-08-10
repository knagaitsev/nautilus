.section .text

.include "arch/arm64/asm/save_regs.S"

.MACRO NESTABLE_EXCP_ENTRY

  sub sp, sp, 0x20

  // Store interrupt corruptible registers
  mrs x1, SPSR_EL1
  mrs x2, ELR_EL1
  stp x1, x2, [sp, 0x00]

  // Get extra info about the interrupt
  mrs x1, ESR_EL1
  mrs x2, FAR_EL1
  stp x1, x2, [sp, 0x10]

  mov x1, sp

  // Increment exception depth (if non-null)
  mrs x2, TPIDR_EL1
  cmp x2, 0
  b.eq 1f
  ldrh w3, [x2, 8]
  add w3, w3, 1
  strh w3, [x2, 8]
1: 

.ENDM

.MACRO NESTABLE_EXCP_EXIT

  // Restore interrupt registers
  ldp x0, x1, [sp, 0x00]
  msr SPSR_EL1, x0
  msr ELR_EL1, x1

  add sp, sp, 0x20

.ENDM

.MACRO DEC_EXCP_DEPTH
  mrs x2, TPIDR_EL1
  cmp x2, 0
  b.eq 1f
  ldrh w3, [x2, 8]
  sub w3, w3, 1
  strh w3, [x2, 8]
1:
.ENDM

.EXTERN route_interrupt
.EXTERN nk_thread_switch_intr_entry

.GLOBAL el0_to_el1_interrupt_entry
.GLOBAL el0_to_el1_fast_interrupt_entry
el0_to_el1_interrupt_entry:
el0_to_el1_fast_interrupt_entry:
  SAVE_CALLER_GPRS 
  mov x0, sp // Store nk_regs*

  NESTABLE_EXCP_ENTRY 

  mov x2, 0
  bl route_interrupt

  DEC_EXCP_DEPTH

  cbz x0, 1f
  bl nk_thread_switch_intr_entry
1:

  NESTABLE_EXCP_EXIT
  RESTORE_CALLER_GPRS
  eret

.GLOBAL el1_to_el1_interrupt_entry
.GLOBAL el1_to_el1_fast_interrupt_entry
el1_to_el1_fast_interrupt_entry:
el1_to_el1_interrupt_entry:
  SAVE_CALLER_GPRS
  mov x0, sp // Store nk_regs*

  NESTABLE_EXCP_ENTRY

  mov x2, 1
  bl route_interrupt

  DEC_EXCP_DEPTH

  cbz x0, 1f 
  bl nk_thread_switch_intr_entry
1:
  
  NESTABLE_EXCP_EXIT
  RESTORE_CALLER_GPRS
el1_to_el1_interrupt_end:
  eret

.EXTERN route_exception

.GLOBAL el0_to_el1_serror_entry
.GLOBAL el0_to_el1_exception_entry
el0_to_el1_serror_entry:
  SAVE_CALLER_GPRS
  mov x0, sp // Store nk_regs*

  NESTABLE_EXCP_ENTRY
  mov x3, 0
  b __el0_to_el1_excp_entry_common
el0_to_el1_exception_entry:
  SAVE_CALLER_GPRS
  mov x0, sp // Store nk_regs*

  NESTABLE_EXCP_ENTRY
  mov x3, 1

__el0_to_el1_excp_entry_common:

  mov x2, 0
  bl route_exception

  DEC_EXCP_DEPTH

  NESTABLE_EXCP_EXIT
  RESTORE_CALLER_GPRS
  eret

.GLOBAL el1_to_el1_serror_entry
.GLOBAL el1_to_el1_exception_entry
el1_to_el1_serror_entry:
  SAVE_CALLER_GPRS
  mov x0, sp // Store nk_regs*

  NESTABLE_EXCP_ENTRY
  mov x3, 0
  b __el1_to_el1_excp_entry_common
el1_to_el1_exception_entry:
  SAVE_CALLER_GPRS
  mov x0, sp // Store nk_regs*

  NESTABLE_EXCP_ENTRY
  mov x3, 1

__el1_to_el1_excp_entry_common:
  mov x2, 1
  bl route_exception

  DEC_EXCP_DEPTH

  NESTABLE_EXCP_EXIT
  RESTORE_CALLER_GPRS
  eret

