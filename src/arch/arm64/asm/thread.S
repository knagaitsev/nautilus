
.GLOBAL nk_thread_entry

nk_thread_entry:

  ldr lr, [sp, 0x8]
  add sp, sp, 0x10
  
  blr lr

  bl nk_thread_exit

.GLOBAL nk_thread_switch

nk_thread_switch:

  sub sp, sp, 0xb0

  // Store the registers which would have 
  // been saved on the stack by an interrupt

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

  // Fake the excp_entry_info structure on the stack
  sub sp, sp, 0x20

  // increment the interrupt nesting level
  mrs x1, TPIDR_EL1
  ldr x2, [x1, 8]
  add x2, x2, 1
  str x2, [x1, 8]

nk_thread_switch_intr_entry:

  // Throw away the excp_entry_info struct
  add sp, sp, 0x20

  // Save the other GP registers
  // That we normally can ignore on exception entry
  sub sp, sp, 0x60
  stp x20, x21, [sp, 0x00]
  stp x22, x23, [sp, 0x10]
  stp x24, x25, [sp, 0x20]
  stp x26, x27, [sp, 0x30]
  stp x28, x29, [sp, 0x40]


  // Store flags
  mrs x2, NZCV
  stp xzr, x2, [sp, 0x50]


  // Save our stack ptr into the thread struct
  ldr x1, [x1]
  mov x2, sp
  str x2, [x1]

nk_thread_switch_on_exit:

  // Set this CPU's current thread to the function argument
  mrs x1, TPIDR_EL1
  str x0, [x1]

  // Load the stack ptr
  ldr x2, [x0]
  mov sp, x2

  // Decrement the interrupt nesting level
  ldr x2, [x1, 8]
  sub x2, x2, 1
  str x2, [x1, 8]
 
  // Restore registers
  // Incrementing the stack in steps isn't needed but for now
  // when correctness is a worry it's simpler to understand

  ldp x20, x21, [sp, 0x00]
  ldp x22, x23, [sp, 0x10]
  ldp x24, x25, [sp, 0x20]
  ldp x26, x27, [sp, 0x30]
  ldp x28, x29, [sp, 0x40]

  // Restore flags
  ldp xzr, x2, [sp, 0x50]
  msr NZCV, x2
 
  add sp, sp, 0x60
 
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

  ret

.GLOBAL nk_thread_switch_exit_helper
nk_thread_switch_exit_helper:

  // Set interrupt nesting to 1 (will be decremented to zero later)
  mrs x3, TPIDR_EL1
  mov x4, 1
  str x4, [x3]

  // Reset preemption disable level
  mov x4, 0
  str x4, [x3, 12]

  // Change our status as the arguments tell us to
  str x2, [x1]

  b nk_thread_switch_on_exit

  // BAD
  ret

.GLOBAL nk_fp_save

nk_fp_save:

  str q0, [x0, 0x00]
  str q1, [x0, 0x10]
  str q2, [x0, 0x20]
  str q3, [x0, 0x30]
  str q4, [x0, 0x40]
  str q5, [x0, 0x50]
  str q6, [x0, 0x60]
  str q7, [x0, 0x70]
  str q8, [x0, 0x80]
  str q9, [x0, 0x90]
  str q10, [x0, 0xa0]
  str q11, [x0, 0xb0]
  str q12, [x0, 0xc0]
  str q13, [x0, 0xd0]
  str q14, [x0, 0xe0]
  str q15, [x0, 0xf0]
  str q16, [x0, 0x100]
  str q17, [x0, 0x110]
  str q18, [x0, 0x120]
  str q19, [x0, 0x130]
  str q20, [x0, 0x140]
  str q21, [x0, 0x150]
  str q22, [x0, 0x160]
  str q23, [x0, 0x170]
  str q24, [x0, 0x180]
  str q25, [x0, 0x190]
  str q26, [x0, 0x1a0]
  str q27, [x0, 0x1b0]
  str q28, [x0, 0x1c0]
  str q29, [x0, 0x1d0]
  str q30, [x0, 0x1e0]
  str q31, [x0, 0x1f0]

  ret

.GLOBAL nk_fp_restore

nk_fp_restore:

  ldr q0, [x0, 0x00]
  ldr q1, [x0, 0x10]
  ldr q2, [x0, 0x20]
  ldr q3, [x0, 0x30]
  ldr q4, [x0, 0x40]
  ldr q5, [x0, 0x50]
  ldr q6, [x0, 0x60]
  ldr q7, [x0, 0x70]
  ldr q8, [x0, 0x80]
  ldr q9, [x0, 0x90]
  ldr q10, [x0, 0xa0]
  ldr q11, [x0, 0xb0]
  ldr q12, [x0, 0xc0]
  ldr q13, [x0, 0xd0]
  ldr q14, [x0, 0xe0]
  ldr q15, [x0, 0xf0]
  ldr q16, [x0, 0x100]
  ldr q17, [x0, 0x110]
  ldr q18, [x0, 0x120]
  ldr q19, [x0, 0x130]
  ldr q20, [x0, 0x140]
  ldr q21, [x0, 0x150]
  ldr q22, [x0, 0x160]
  ldr q23, [x0, 0x170]
  ldr q24, [x0, 0x180]
  ldr q25, [x0, 0x190]
  ldr q26, [x0, 0x1a0]
  ldr q27, [x0, 0x1b0]
  ldr q28, [x0, 0x1c0]
  ldr q29, [x0, 0x1d0]
  ldr q30, [x0, 0x1e0]
  ldr q31, [x0, 0x1f0]

  ret
