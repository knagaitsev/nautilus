
.section .text

.global nk_low_level_memset

# x0 = void *dest
# x1 = char data
# x2 = size_t count

.align 3
nk_low_level_memset:

# Calculate end
  add x2, x2, x0 

1:
  cmp x2, x0
  b.eq 2f

  strb w1, [x0]
  add x0, x0, 1

  b 1b
2:

  ret

.global nk_low_level_memcpy

# x0 = void *dest
# x1 = char *src
# x2 = size_t count

.align 3
nk_low_level_memcpy:
  // TODO(arm64)
  b .
  ret
