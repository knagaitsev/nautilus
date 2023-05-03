
.MACRO PREEMPTIBLE_EXCP_ENTRY
  sub sp, sp, 0xb0
  // Store GP Registers
  stp x0, x1, [sp, 0x00]
  stp x2, x3, [sp, 0x10]
  stp x4, x5, [sp, 0x20]
  stp x6, x7, [sp, 0x30]
  stp x8, x9, [sp, 0x40]
  stp x10, x11, [sp, 0x50]
  stp x12, x13, [sp, 0x60]
  stp x14, x15, [sp, 0x70]
  stp x16, x17, [sp, 0x80]
  stp x18, x29, [sp, 0x90]
  stp x30, xzr, [sp, 0xa0]

  mov x0, sp // Store nk_regs*

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

  // Increment exception depth
  mrs x2, TPIDR_EL1
  ldr x3, [x2, 8]
  add x3, x3, 1
  str x3, [x2, 8]
 
  // Enable interrupts
  msr DAIFClr, 0b1111
.ENDM

.MACRO PREEMPTIBLE_EXCP_EXIT
  // Disable interrupts
  msr DAIFSet, 0b1111

  // Decrement exception depth
  mrs x2, TPIDR_EL1
  ldr x3, [x2, 8]
  sub x3, x3, 1
  str x3, [x2, 8]

  // Restore interrupt registers
  ldp x0, x1, [sp]
  msr SPSR_EL1, x0
  msr ELR_EL1, x1

  add sp, sp, 0x20

  // Restore GP Registers
  ldp x0, x1, [sp, 0x00]
  ldp x2, x3, [sp, 0x10]
  ldp x4, x5, [sp, 0x20]
  ldp x6, x7, [sp, 0x30]
  ldp x8, x9, [sp, 0x40]
  ldp x10, x11, [sp, 0x50]
  ldp x12, x13, [sp, 0x60]
  ldp x14, x15, [sp, 0x70]
  ldp x16, x17, [sp, 0x80]
  ldp x18, x29, [sp, 0x90]
  ldp x30, xzr, [sp, 0xa0]

  add sp, sp, 0xb0
.ENDM

.EXTERN route_interrupt_from_user
.GLOBAL el0_to_el1_interrupt_entry
el0_to_el1_interrupt_entry:
  // Push data onto the stack
  PREEMPTIBLE_EXCP_ENTRY
  
  bl route_interrupt_from_user
  
  // Pop data off of the stack
  PREEMPTIBLE_EXCP_EXIT
  eret

.EXTERN route_interrupt_from_kernel
.GLOBAL el1_to_el1_interrupt_entry
el1_to_el1_interrupt_entry:
  PREEMPTIBLE_EXCP_ENTRY

  bl route_interrupt_from_kernel

  PREEMPTIBLE_EXCP_EXIT
  eret

.EXTERN route_exception_from_user
.GLOBAL el0_to_el1_exception_entry
el0_to_el1_exception_entry:
  PREEMPTIBLE_EXCP_ENTRY

  bl route_exception_from_user
  
  PREEMPTIBLE_EXCP_EXIT
  eret

.EXTERN route_exception_from_kernel
.GLOBAL el1_to_el1_exception_entry
el1_to_el1_exception_entry:
  PREEMPTIBLE_EXCP_ENTRY

  bl route_exception_from_kernel

  PREEMPTIBLE_EXCP_EXIT
  eret

