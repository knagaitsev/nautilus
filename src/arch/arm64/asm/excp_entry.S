
.EXTERN route_interrupt_from_user
.GLOBAL el0_to_el1_interrupt_entry
el0_to_el1_interrupt_entry:

  sub sp, sp, 0xc0

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

  mrs x0, ELR_EL1
  stp x30, x0, [sp, 0xa0]

  mrs x0, ESR_EL1
  mrs x1, FAR_EL1
  stp x0, x1, [sp, 0xb0]

  mov x0, sp
  bl route_interrupt_from_user

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

  add sp, sp, 0xc0

  eret

.EXTERN route_interrupt_from_kernel
.GLOBAL el1_to_el1_interrupt_entry
el1_to_el1_interrupt_entry:

  sub sp, sp, 0xc0

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

  mrs x0, ELR_EL1
  stp x30, x0, [sp, 0xa0]

  mrs x0, ESR_EL1
  mrs x1, FAR_EL1
  mrs x2, ELR_EL1
  mrs x3, ESR_EL1
  stp x0, x1, [sp, 0xb0]

  mov x0, sp
  bl route_interrupt_from_kernel

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

  add sp, sp, 0xc0

  eret

.EXTERN route_exception_from_user
.GLOBAL el0_to_el1_exception_entry
el0_to_el1_exception_entry:

  sub sp, sp, 0xc0

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

  mrs x0, ELR_EL1
  stp x30, x0, [sp, 0xa0]

  mrs x0, ESR_EL1
  mrs x1, FAR_EL1
  mrs x2, ELR_EL1
  mrs x3, ESR_EL1
  stp x0, x1, [sp, 0xb0]

  mov x0, sp
  bl route_exception_from_user

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

  add sp, sp, 0xc0

  eret

.EXTERN route_exception_from_kernel
.GLOBAL el1_to_el1_exception_entry
el1_to_el1_exception_entry:

  sub sp, sp, 0xc0

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

  mrs x0, ELR_EL1
  stp x30, x0, [sp, 0xa0]

  mrs x0, ESR_EL1
  mrs x1, FAR_EL1
  stp x0, x1, [sp, 0xb0]

  mov x0, sp
  bl route_exception_from_kernel

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

  add sp, sp, 0xc0

  eret

