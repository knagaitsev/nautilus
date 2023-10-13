#ifndef __RISCV_ASM_LOWLEVEL_H__
#define __RISCV_ASM_LOWLEVEL_H__

#define ENTRY(x)   \
    .globl x;      \
    .align 4, 0x00,0x01;\
    x:

#define PTRLOG 3
#define SZREG 8
#define REG_S sd
#define REG_L ld
#define REG_SC sc.d
#define ROFF(N, R) N*SZREG(R)

#define GLOBAL(x)  \
    .globl x;      \
    x:

#define END(x) \
    .size x, .-x

#define CALLER_GPR_SAVE_SIZE (18 * 8)

#define SAVE_CALLER_GPRS()\
  addi sp, sp, -CALLER_GPR_SAVE_SIZE;\
  REG_S ra, ROFF(0, sp);\
  REG_S t0, ROFF(1, sp);\
  REG_S t1, ROFF(2, sp);\
  REG_S t2, ROFF(3, sp);\
  REG_S t3, ROFF(4, sp);\
  REG_S t4, ROFF(5, sp);\
  REG_S t5, ROFF(6, sp);\
  REG_S t6, ROFF(7, sp);\
  REG_S a0, ROFF(8, sp);\
  REG_S a1, ROFF(9, sp);\
  REG_S a2, ROFF(10, sp);\
  REG_S a3, ROFF(11, sp);\
  REG_S a4, ROFF(12, sp);\
  REG_S a5, ROFF(13, sp);\
  REG_S a6, ROFF(14, sp);\
  REG_S a7, ROFF(15, sp);\
  addi t0, sp, CALLER_GPR_SAVE_SIZE;\
  REG_S t0, ROFF(16, sp);\
  REG_S x0, ROFF(17, sp);
  // ^^^ Need to maintain stack alignment

#define RESTORE_CALLER_GPRS()\
  REG_L ra, ROFF(0, sp);\
  REG_L t0, ROFF(1, sp);\
  REG_L t1, ROFF(2, sp);\
  REG_L t2, ROFF(3, sp);\
  REG_L t3, ROFF(4, sp);\
  REG_L t4, ROFF(5, sp);\
  REG_L t5, ROFF(6, sp);\
  REG_L t6, ROFF(7, sp);\
  REG_L a0, ROFF(8, sp);\
  REG_L a1, ROFF(9, sp);\
  REG_L a2, ROFF(10, sp);\
  REG_L a3, ROFF(11, sp);\
  REG_L a4, ROFF(12, sp);\
  REG_L a5, ROFF(13, sp);\
  REG_L a6, ROFF(14, sp);\
  REG_L a7, ROFF(15, sp);\
  addi sp, sp, CALLER_GPR_SAVE_SIZE;

#define CALLEE_GPR_SAVE_SIZE (12 * 8)
#define SAVE_CALLEE_GPRS()\
  add sp, sp, -CALLEE_GPR_SAVE_SIZE;\
  REG_S s0, ROFF(0, sp);\
  REG_S s1, ROFF(1, sp);\
  REG_S s2, ROFF(2, sp);\
  REG_S s3, ROFF(3, sp);\
  REG_S s4, ROFF(4, sp);\
  REG_S s5, ROFF(5, sp);\
  REG_S s6, ROFF(6, sp);\
  REG_S s7, ROFF(7, sp);\
  REG_S s8, ROFF(8, sp);\
  REG_S s9, ROFF(9, sp);\
  REG_S s10, ROFF(10, sp);\
  REG_S s11, ROFF(11, sp);

#define RESTORE_CALLEE_GPRS()\
  REG_L s0, ROFF(0, sp);\
  REG_L s1, ROFF(1, sp);\
  REG_L s2, ROFF(2, sp);\
  REG_L s3, ROFF(3, sp);\
  REG_L s4, ROFF(4, sp);\
  REG_L s5, ROFF(5, sp);\
  REG_L s6, ROFF(6, sp);\
  REG_L s7, ROFF(7, sp);\
  REG_L s8, ROFF(8, sp);\
  REG_L s9, ROFF(9, sp);\
  REG_L s10, ROFF(10, sp);\
  REG_L s11, ROFF(11, sp);\
  addi sp, sp, CALLEE_GPR_SAVE_SIZE;

#endif
