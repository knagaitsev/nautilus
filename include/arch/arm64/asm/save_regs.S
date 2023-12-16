
.section .text

.macro SAVE_CALLER_GPRS

  sub sp, sp, 0xb0

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

// Do fancy stuff so sp appears correctly in the nk_regs struct
  add x1, sp, 0xb0
  stp x30, x1, [sp, 0xa0]

.endm

.macro RESTORE_CALLER_GPRS

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

  // We don't actually restore the sp value from the stack
  ldp x30, xzr, [sp, 0xa0]

  add sp, sp, 0xb0

.endm

// Call this after SAVE_CALLER_GPRS (uses x0 and x1)
.macro SAVE_CALLER_FPRS
  sub sp, sp, 0x190
  str q0, [sp, 0x00]
  str q1, [sp, 0x10] 
  str q2, [sp, 0x20] 
  str q3, [sp, 0x30]
  str q4, [sp, 0x40]
  str q5, [sp, 0x50]
  str q6, [sp, 0x60]
  str q7, [sp, 0x70]
  str q16, [sp, 0x80]  
  str q17, [sp, 0x90]
  str q18, [sp, 0xa0]
  str q19, [sp, 0xb0]
  str q20, [sp, 0xc0]
  str q21, [sp, 0xd0]
  str q22, [sp, 0xe0]
  str q23, [sp, 0xf0]
  str q24, [sp, 0x100]
  str q25, [sp, 0x110]
  str q26, [sp, 0x120]
  str q27, [sp, 0x130]
  str q28, [sp, 0x140]
  str q29, [sp, 0x150]
  str q30, [sp, 0x160]
  str q31, [sp, 0x170]
  mrs x3, FPCR
  mrs x4, FPSR
  stp x3, x4, [sp, 0x180]
.endm

.macro RESTORE_CALLER_FPRS
  ldr q0, [sp, 0x00]
  ldr q1, [sp, 0x10] 
  ldr q2, [sp, 0x20] 
  ldr q3, [sp, 0x30]
  ldr q4, [sp, 0x40]
  ldr q5, [sp, 0x50]
  ldr q6, [sp, 0x60]
  ldr q7, [sp, 0x70]
  ldr q16, [sp, 0x80]  
  ldr q17, [sp, 0x90]
  ldr q18, [sp, 0xa0]
  ldr q19, [sp, 0xb0]
  ldr q20, [sp, 0xc0]
  ldr q21, [sp, 0xd0]
  ldr q22, [sp, 0xe0]
  ldr q23, [sp, 0xf0]
  ldr q24, [sp, 0x100]
  ldr q25, [sp, 0x110]
  ldr q26, [sp, 0x120]
  ldr q27, [sp, 0x130]
  ldr q28, [sp, 0x140]
  ldr q29, [sp, 0x150]
  ldr q30, [sp, 0x160]
  ldr q31, [sp, 0x170]
  ldp x3, x4, [sp, 0x180]
  msr FPCR, x3
  msr FPSR, x4
  add sp, sp, 0x190
.endm

