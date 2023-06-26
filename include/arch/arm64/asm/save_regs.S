
.section .text

.MACRO SAVE_CALLER_GPRS

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

  add x1, sp, 0xb0

  stp x30, x1, [sp, 0xa0]

.ENDM

.MACRO RESTORE_CALLER_GPRS

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

.ENDM

