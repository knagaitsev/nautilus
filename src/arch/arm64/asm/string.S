
.section .text

.GLOBAL nk_low_level_memset

# x0 = void *dest
# x1 = char data
# x2 = size_t count

.align 3
nk_low_level_memset:

  # If count = 0 return early
0:
  cmp x2, 8
  blo 2f
  tst x0, 0b111
  beq 1f

  strb w1, [x0], 1
  sub x2, x2, 1

  b 0b

1:

  mov x3, x1, lsl 8
  orr x1, x3, x1
  mov x3, x1, lsl 16
  orr x1, x3, x1
  mov x3, x1, lsl 32
  orr x1, x3, x1

  cmp x2, 8
  blo 2f

  str x1, [x0], 8
  sub x2, x2, 8

2:
  cmp x2, 0
  beq 3f

  strb w1, [x0], 1
  sub x2, x2, 1

3:
  ret

.GLOBAL nk_low_level_memcpy

# x0 = void *dest
# x1 = char *src
# x2 = size_t count

.align 3
nk_low_level_memcpy:
  // TODO(arm64)
  b .
  ret
