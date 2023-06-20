
.GLOBAL nk_thread_entry

nk_thread_entry:

  ldr x19, [sp, 0x0]
  
  blr x19

  ldr x19, [sp, 0x8]

  add sp, sp, 0x10

  br x19

.GLOBAL nk_thread_switch
.GLOBAL nk_thread_switch_intr_entry

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

  // Make sure that lr is zero if we don't enter through intr entry
  mov x30, 0

nk_thread_switch_intr_entry:
  
  // If we enter through interrupts, x30 should now be non-zero.

  // Save the other GP registers
  // That we normally can ignore on exception entry
  sub sp, sp, 0x70
  stp x20, x21, [sp, 0x00]
  stp x22, x23, [sp, 0x10]
  stp x24, x25, [sp, 0x20]
  stp x26, x27, [sp, 0x30]
  stp x28, x29, [sp, 0x40]

  // Save the return address incase we entered through an interrupt
  stp x30, xzr, [sp, 0x50]

  // Save our stack ptr into the thread struct
  mrs x3, TPIDR_EL1
  ldr x3, [x3] // Thread field of CPU struct

#ifdef NAUT_CONFIG_FPU_SAVE
  // Get the offset of the FPU State
  ldr x2, =__nk_thread_fpu_state_offset
  ldr x2, [x2]

  // Get a pointer to the FPU State
  mov x4, x0
  add x0, x3, x2

  // Save the state
  bl nk_fp_save

  // Restore x0 back to pointing at the thread
  mov x0, x4
#endif

  mov x2, sp
  str x2, [x3] // RSP field of thread struct

  // Store flags
  mrs x2, DAIF
  mrs x3, NZCV
  stp x2, x3, [sp, 0x60]

nk_thread_switch_on_exit:
  // Set this CPU's current thread to the function argument
  mrs x3, TPIDR_EL1
  str x0, [x3]

  // Load the stack ptr
  ldr x2, [x0]
  mov sp, x2

#ifdef NAUT_CONFIG_FPU_SAVE
  ldr x2, =__nk_thread_fpu_state_offset
  ldr x2, [x2]

  add x0, x0, x2
  bl nk_fp_restore
  sub x0, x0, x2
#endif

  ldp x20, x21, [sp, 0x00]
  ldp x22, x23, [sp, 0x10]
  ldp x24, x25, [sp, 0x20]
  ldp x26, x27, [sp, 0x30]
  ldp x28, x29, [sp, 0x40]

  // Get whether or not we entered this via interrupt context or not
  ldp x30, xzr, [sp, 0x50]

  // Restore flags
  ldp x2, x3, [sp, 0x60]
  msr DAIF, x2
  msr NZCV, x3
 
  add sp, sp, 0x70

  cbz x30, 1f // If lr is 0 then we shouldn't return early, because we didn't enter 
              // through an interrupt
  ret
1:

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

  mrs x3, TPIDR_EL1

  // Reset the interrupt nesting level
  str wzr, [x3, 8]

  // Reset preemption disable level
  str wzr, [x3, 12]
  
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
