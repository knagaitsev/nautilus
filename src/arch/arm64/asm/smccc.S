
.global smc64_call
smc64_call:
sub sp, sp, 0x10
stp x4,x5,[sp, 0x0]

and x0, x0, 0xFFFF // Only 16 bit id

and x1, x1, 0x3F // Only 6 bit type
lsl x1, x1, 24

orr x0, x0, x1

orr x0, x0, (1<<30) // Set 64-bit
orr x0, x0, (1<<31) // Make it a fast-call

mov x9, x2

cbz x3, _smc64_0_arg
sub x3, x3, 1
cbz x3, _smc64_1_arg
sub x3, x3, 1
cbz x3, _smc64_2_arg
sub x3, x3, 1
cbz x3, _smc64_3_arg
sub x3, x3, 1
cbz x3, _smc64_4_arg
sub x3, x3, 1
cbz x3, _smc64_5_arg
_smc64_6_arg:
ldr x6, [x9, 0x28]
_smc64_5_arg:
ldr x5, [x9, 0x20]
_smc64_4_arg:
ldr x4, [x9, 0x18]
_smc64_3_arg:
ldr x3, [x9, 0x10]
_smc64_2_arg:
ldr x2, [x9, 0x8]
_smc64_1_arg:
ldr x1, [x9, 0x0]
_smc64_0_arg:

smc 0;

ldp x4,x5, [sp, 0x0]
add sp, sp, 0x10

cbz x5, _end_smc64_call
sub x5, x5, 1
cbz x5, _smc64_1_ret
sub x5, x5, 1
cbz x5, _smc64_2_ret
sub x5, x5, 1
cbz x5, _smc64_3_ret
_smc64_4_ret:
str x3, [x4, 0x18]
_smc64_3_ret:
str x2, [x4, 0x10]
_smc64_2_ret:
str x1, [x4, 0x8]
_smc64_1_ret:
str x0, [x4, 0x0]
_end_smc64_call:
ret

.global hvc64_call
hvc64_call:
sub sp, sp, 0x10
stp x4,x5,[sp, 0x0]

and x0, x0, 0xFFFF // Only 16 bit id

and x1, x1, 0x3F // Only 6 bit type
lsl x1, x1, 24

orr x0, x0, x1

orr x0, x0, (1<<30) // Set 64-bit
orr x0, x0, (1<<31) // Make it a fast-call

cbz x3, _hvc64_0_arg
sub x3, x3, 1
cbz x3, _hvc64_1_arg
sub x3, x3, 1
cbz x3, _hvc64_2_arg
sub x3, x3, 1
cbz x3, _hvc64_3_arg
sub x3, x3, 1
cbz x3, _hvc64_4_arg
sub x3, x3, 1
cbz x3, _hvc64_5_arg
_hvc64_6_arg:
ldr x6, [x2, 0x28]
_hvc64_5_arg:
ldr x5, [x2, 0x20]
_hvc64_4_arg:
ldr x4, [x2, 0x18]
_hvc64_3_arg:
ldr x3, [x2, 0x10]
_hvc64_2_arg:
ldr x2, [x2, 0x8]
_hvc64_1_arg:
ldr x1, [x2, 0x0]
_hvc64_0_arg:

hvc 0;

ldp x4,x5, [sp, 0x0]
add sp, sp, 0x10

cbz x5, _end_hvc64_call
sub x5, x5, 1
cbz x5, _hvc64_1_ret
sub x5, x5, 1
cbz x5, _hvc64_2_ret
sub x5, x5, 1
cbz x5, _hvc64_3_ret
_hvc64_4_ret:
str x3, [x4, 0x18]
_hvc64_3_ret:
str x2, [x4, 0x10]
_hvc64_2_ret:
str x1, [x4, 0x8]
_hvc64_1_ret:
str x0, [x4, 0x0]
_end_hvc64_call:
ret

.global smc32_call
smc32_call:
sub sp, sp, 0x10
stp x4,x5,[sp, 0x0]

and x0, x0, 0xFFFF // Only 16 bit id

and x1, x1, 0x3F // Only 6 bit type
lsl x1, x1, 24

orr x0, x0, x1

bic x0, x0, (1<<30) // Set 32-bit
orr x0, x0, (1<<31) // Make it a fast-call

cbz x3, _smc32_0_arg
sub x3, x3, 1
cbz x3, _smc32_1_arg
sub x3, x3, 1
cbz x3, _smc32_2_arg
sub x3, x3, 1
cbz x3, _smc32_3_arg
sub x3, x3, 1
cbz x3, _smc32_4_arg
sub x3, x3, 1
cbz x3, _smc32_5_arg
_smc32_6_arg:
ldr w6, [x2, 0x14]
_smc32_5_arg:
ldr w5, [x2, 0x10]
_smc32_4_arg:
ldr w4, [x2, 0xC]
_smc32_3_arg:
ldr w3, [x2, 0x8]
_smc32_2_arg:
ldr w2, [x2, 0x4]
_smc32_1_arg:
ldr w1, [x2, 0x0]
_smc32_0_arg:

smc 0;

ldp x4,x5, [sp, 0x0]
add sp, sp, 0x10

cbz x5, _end_smc32_call
sub x5, x5, 1
cbz x5, _smc32_1_ret
sub x5, x5, 1
cbz x5, _smc32_2_ret
sub x5, x5, 1
cbz x5, _smc32_3_ret
_smc32_4_ret:
str w3, [x4, 0xC]
_smc32_3_ret:
str w2, [x4, 0x8]
_smc32_2_ret:
str w1, [x4, 0x4]
_smc32_1_ret:
str w0, [x4, 0x0]
_end_smc32_call:
ret

.global hvc32_call
hvc32_call:
sub sp, sp, 0x10
stp x4,x5,[sp, 0x0]

and x0, x0, 0xFFFF // Only 16 bit id

and x1, x1, 0x3F // Only 6 bit type
lsl x1, x1, 24

orr x0, x0, x1

bic x0, x0, (1<<30) // Set 32-bit
orr x0, x0, (1<<31) // Make it a fast-call

cbz x3, _hvc32_0_arg
sub x3, x3, 1
cbz x3, _hvc32_1_arg
sub x3, x3, 1
cbz x3, _hvc32_2_arg
sub x3, x3, 1
cbz x3, _hvc32_3_arg
sub x3, x3, 1
cbz x3, _hvc32_4_arg
sub x3, x3, 1
cbz x3, _hvc32_5_arg
_hvc32_6_arg:
ldr w6, [x2, 0x14]
_hvc32_5_arg:
ldr w5, [x2, 0x10]
_hvc32_4_arg:
ldr w4, [x2, 0xC]
_hvc32_3_arg:
ldr w3, [x2, 0x8]
_hvc32_2_arg:
ldr w2, [x2, 0x4]
_hvc32_1_arg:
ldr w1, [x2, 0x0]
_hvc32_0_arg:

hvc 0;

ldp x4,x5, [sp, 0x0]
add sp, sp, 0x10

cbz x5, _end_hvc32_call
sub x5, x5, 1
cbz x5, _hvc32_1_ret
sub x5, x5, 1
cbz x5, _hvc32_2_ret
sub x5, x5, 1
cbz x5, _hvc32_3_ret
_hvc32_4_ret:
str w3, [x4, 0xC]
_hvc32_3_ret:
str w2, [x4, 0x8]
_hvc32_2_ret:
str w1, [x4, 0x4]
_hvc32_1_ret:
str w0, [x4, 0x0]
_end_hvc32_call:
ret

