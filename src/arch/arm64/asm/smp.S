

.GLOBAL init_smp_boot

init_smp_boot:
  // TODO

.ALIGN 3
.GLOBAL end_smp_boot
end_smp_boot:


.GLOBAL smp_ap_stack_switch

// x0 := new stack pointer
// x1 := new base pointer
// x2 := cpu struct ptr
// returns the new cpu struct ptr
smp_ap_stack_switch:
  mov sp, x0
  mov x29, x1
  mov x0, x2
  ret

