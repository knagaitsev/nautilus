
.GLOBAL nk_thread_entry
.section .text

.include "arch/arm64/asm/save_regs.S"

nk_thread_entry:

  msr DAIF, xzr

  ldr x18, [sp, 0x0]
  
  blr x18

  ldr x30, [sp, 0x8]

  add sp, sp, 0x10

  ret

.GLOBAL nk_thread_switch
.GLOBAL nk_thread_switch_intr_entry

nk_thread_switch:

  // Store the registers which would have 
  // been saved on the stack by an interrupt
  SAVE_CALLER_GPRS

  // Make sure that lr is zero if we don't enter through intr entry
  mov x30, 0

nk_thread_switch_intr_entry:
  
  // If we enter through interrupts, x30 should now be non-zero.

  // Save the other GP registers
  // That we normally can ignore on exception entry
  sub sp, sp, 0x70
  stp x19, x20, [sp, 0x00]
  stp x21, x22, [sp, 0x10]
  stp x23, x24, [sp, 0x20]
  stp x25, x26, [sp, 0x30]
  stp x27, x28, [sp, 0x40]

  // Save the return address incase we entered through an interrupt
  stp x30, xzr, [sp, 0x50]

  // Save our stack ptr into the thread struct
  mrs x16, TPIDR_EL1
  ldr x16, [x16] // Thread field of CPU struct

#ifdef NAUT_CONFIG_FPU_SAVE

  // Get the offset of the FPU State
  ldr x18, =__nk_thread_fpu_state_offset
  ldr x18, [x18]

  // Get a pointer to the FPU State
  mov x17, x0
  add x0, x16, x18

  // Save the state
  bl nk_fp_save

  // Restore x0 back to pointing at the thread
  mov x0, x17

#endif

  mov x2, sp
  str x2, [x16] // RSP field of thread struct

  // Store flags
  mrs x2, DAIF
  mrs x3, NZCV
  stp x2, x3, [sp, 0x60]

nk_thread_switch_on_exit:
  // Set this CPU's current thread to the function argument
  mrs x16, TPIDR_EL1
  str x0, [x16]

  // Load the stack ptr
  ldr x2, [x0]
  mov sp, x2

#ifdef NAUT_CONFIG_FPU_SAVE
  ldr x18, =__nk_thread_fpu_state_offset
  ldr x18, [x18]

  add x0, x0, x18
  bl nk_fp_restore
  sub x0, x0, x18
#endif

  ldp x19, x20, [sp, 0x00]
  ldp x21, x22, [sp, 0x10]
  ldp x23, x24, [sp, 0x20]
  ldp x25, x26, [sp, 0x30]
  ldp x27, x28, [sp, 0x40]

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

  RESTORE_CALLER_GPRS

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
  mrs x1, FPSR
  mrs x2, FPCR
 
  // Out of range for dereference offset
  add x0, x0, 0x200
  stp x1, x2, [x0, 0x0]

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

  add x0, x0, 0x200
  ldp x1, x2, [x0, 0x0]
  msr FPSR, x1
  msr FPCR, x2

  ret
