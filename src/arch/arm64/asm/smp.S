
.section .text

.global init_smp_boot

init_smp_boot:
  // TODO
  ret

.align 3
.global end_smp_boot
end_smp_boot:


.global smp_ap_stack_switch

// x0 := new stack pointer
// x1 := new base pointer
// x2 := cpu struct ptr
// returns the new cpu struct ptr
// ARM likes to put some strange things on the stack,
// so we're going to put in some extra effort to copy the bootstack
// This means any stack we allocate for a thread, needs to be larger than
// the bootstack (or atleast as large as the space the bootstack uses)
.extern boot_stack_end
.global smp_ap_stack_switch
smp_ap_stack_switch:

  mov x3, sp

  ldr x4, =boot_stack_end

1:
  cmp x4, x3
  ble 2f

  ldrb w5, [x4]
  strb w5, [x0]
  sub x0, x0, 1
  sub x4, x4, 1

  b 1b
2:

  mov sp, x0
  // We have -fomit-frame-pointer on so this is just a GP register
  //mov x29, x1

  mov x0, x2
  ret

