
.text
.GLOBAL install_iv_tables_el1
.align 3
install_iv_tables_el1:

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
  adr x1, __iv_table
  msr VBAR_EL1, x1
  ret

.GLOBAL install_iv_tables_el2
.align 3
install_iv_tables_el2:

  // Check that we are in EL1
  mrs x1, CurrentEL
  lsr x1, x1, 2
  cmp x1, 2
  beq 1f

  // We are in the wrong EL!
  // There's not much we can do so we just hang
  // Store an DEADBEEF in x1 and loop forever
  ldr x1, =0xDEADBEEF
  b .

1:
  adr x1, __iv_table
  msr VBAR_EL2, x1
  ret


.text
.EXTERN same_exception_entry
.EXTERN lower_exception_entry
.align 11
__iv_table:
__el0_stack_iv_entrances:
# Uses EL0 stack
.align 7
  b same_exception_entry
.align 7
  b same_interrupt_entry
.align 7
  b same_fast_interrupt_entry
.align 7
  b same_serror_entry

# Uses ELn stack
__n_stack_iv_entrances:
.align 7
  b same_exception_entry
.align 7
  b same_interrupt_entry
.align 7
  b same_fast_interrupt_entry
.align 7
  b same_serror_entry

# From Lower EL (aarch64)
__from_lower_iv_entrances:
.align 7
  b lower_exception_entry
.align 7
  b lower_interrupt_entry
.align 7
  b lower_fast_interrupt_entry
.align 7
  b lower_serror_entry

# From Lower EL (aarch32)
__from_lower_in_aarch32_iv_entrances:
.align 7
  b .
.align 7
  b .
.align 7
  b .
.align 7
  b .

