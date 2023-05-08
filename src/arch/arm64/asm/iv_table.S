
.text
.GLOBAL __install_iv_tables
.align 3
__install_iv_tables:
  
  // Check that we are in EL1
  mrs x1, CurrentEL
  lsr x1, x1, 2
  cmp x1, 1
  beq 1f

  // We are in the wrong EL!
  // There's not much we can do so we just hang
  // Store an DEADBEEF in x1 and loop forever

  ldr x1, =0xDEADBEEF
  b .

1:
  adr x1, __el1_iv_table
  msr VBAR_EL1, x1
  ret

.EXTERN el1_to_el1_exception_entry
.EXTERN el0_to_el1_exception_entry
.align 11
__el1_iv_table:
__el0_stack_iv_entrances:
# Uses EL0 stack
.align 7
  b .
.align 7
  b .
.align 7
  b .
.align 7
  b .

# Uses EL1 stack
__el1_stack_iv_entrances:
.align 7
  b el1_to_el1_exception_entry
.align 7
  b el1_to_el1_interrupt_entry
.align 7
  b el1_to_el1_interrupt_entry
.align 7
  b el1_to_el1_exception_entry

# From EL0 (aarch64)
__from_el0_iv_entrances:
.align 7
  b el0_to_el1_exception_entry
.align 7
  b el0_to_el1_interrupt_entry
.align 7
  b el0_to_el1_interrupt_entry
.align 7
  b el0_to_el1_exception_entry

# From EL0 (aarch32)
__from_el0_in_aarch32_iv_entrances:
.align 7
  b .
.align 7
  b .
.align 7
  b .
.align 7
  b .

