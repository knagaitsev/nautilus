.section .text

.include "arch/arm64/asm/save_regs.S"

.macro NESTABLE_EXCP_ENTRY

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

.endm

.macro NESTABLE_EXCP_EXIT

  // Restore interrupt registers
  ldp x0, x1, [sp, 0x00]
  msr SPSR_EL1, x0
  msr ELR_EL1, x1

  add sp, sp, 0x20

.endm

.macro DEC_EXCP_DEPTH
  mrs x2, TPIDR_EL1
  cmp x2, 0
  b.eq 1f
  ldrh w3, [x2, 8]
  sub w3, w3, 1
  strh w3, [x2, 8]
1:
.endm

.extern route_interrupt
.extern nk_thread_switch_intr_entry

.global lower_interrupt_entry
.global lower_fast_interrupt_entry
lower_interrupt_entry:
lower_fast_interrupt_entry:
  SAVE_CALLER_GPRS 
  SAVE_CALLER_FPRS
  mov x0, sp // Store nk_regs*

  NESTABLE_EXCP_ENTRY 

  mov x2, 0
  bl route_interrupt

  DEC_EXCP_DEPTH

  cbz x0, 1f
  bl nk_thread_switch_intr_entry
1:

  NESTABLE_EXCP_EXIT
  RESTORE_CALLER_FPRS
  RESTORE_CALLER_GPRS
  eret

.global same_interrupt_entry
.global same_fast_interrupt_entry
same_fast_interrupt_entry:
same_interrupt_entry:
  SAVE_CALLER_GPRS
  SAVE_CALLER_FPRS
  mov x0, sp // Store nk_regs*

  NESTABLE_EXCP_ENTRY

  mov x2, 1
  bl route_interrupt

  DEC_EXCP_DEPTH

  cbz x0, 1f 
  bl nk_thread_switch_intr_entry
1:
  
  NESTABLE_EXCP_EXIT
  RESTORE_CALLER_FPRS
  RESTORE_CALLER_GPRS
same_interrupt_end:
  eret

.extern route_exception

.global lower_serror_entry
.global lower_exception_entry
lower_serror_entry:
  SAVE_CALLER_GPRS
  SAVE_CALLER_FPRS
  mov x0, sp // Store nk_regs*

  NESTABLE_EXCP_ENTRY
  mov x3, 0
  b __lower_excp_entry_common
lower_exception_entry:
  SAVE_CALLER_GPRS
  SAVE_CALLER_FPRS
  mov x0, sp // Store nk_regs*

  NESTABLE_EXCP_ENTRY
  mov x3, 1

__lower_excp_entry_common:

  mov x2, 0
  bl route_exception

  DEC_EXCP_DEPTH

  NESTABLE_EXCP_EXIT
  RESTORE_CALLER_FPRS
  RESTORE_CALLER_GPRS
  eret

.global same_serror_entry
.global same_exception_entry
same_serror_entry:
  SAVE_CALLER_GPRS
  SAVE_CALLER_FPRS
  mov x0, sp // Store nk_regs*

  NESTABLE_EXCP_ENTRY
  mov x3, 0
  b __same_excp_entry_common
same_exception_entry:
  SAVE_CALLER_GPRS
  SAVE_CALLER_FPRS
  mov x0, sp // Store nk_regs*

  NESTABLE_EXCP_ENTRY
  mov x3, 1

__same_excp_entry_common:
  mov x2, 1
  bl route_exception

  DEC_EXCP_DEPTH

  NESTABLE_EXCP_EXIT
  RESTORE_CALLER_FPRS
  RESTORE_CALLER_GPRS
  eret

