# lowlevel.S - All low level functionality (boot and util)
# riscv64 bootloader for ChariotOS
# Nick Wanninger
# 29 December, 2020

#include <arch/riscv/asm/lowlevel.h>

/* Status register flags */
#define SR_SIE		(0x00000002UL) /* Supervisor Interrupt Enable */
#define SR_MIE		(0x00000008UL) /* Machine Interrupt Enable */
#define SR_SPIE		(0x00000020UL) /* Previous Supervisor IE */
#define SR_MPIE		(0x00000080UL) /* Previous Machine IE */
#define SR_SPP		(0x00000100UL) /* Previously Supervisor */
#define SR_MPP		(0x00001800UL) /* Previously Machine */
#define SR_SUM		(0x00040000UL) /* Supervisor User Memory Access */

#define CSR_SSTATUS		0x100
#define CSR_SIE			0x104
#define CSR_STVEC		0x105
#define CSR_SCOUNTEREN		0x106
#define CSR_SSCRATCH		0x140
#define CSR_SEPC		0x141
#define CSR_SCAUSE		0x142
#define CSR_STVAL		0x143
#define CSR_SIP			0x144
#define CSR_SATP		0x180

#define MSTATUS_MPP_MASK (3L << 11)  # previous mode.
#define MSTATUS_MPP_M (3L << 11)
#define MSTATUS_MPP_S (1L << 11)
#define MSTATUS_MPP_U (0L << 11)
#define MSTATUS_MIE (1L << 3)  # machine-mode interrupt enable.

#define SIE_SEIE (1L << 9)  # external
#define SIE_STIE (1L << 5)  # timer
#define SIE_SSIE (1L << 1)  # software

#define MIE_MEIE (1L << 11)  # external
#define MIE_MTIE (1L << 7)  # timer
#define MIE_MSIE (1L << 3)  # software


# .option norvc # Disable instruction compression
.section .boot

GLOBAL(nautilus_entry)

    # OpenSBI passes us the hartid through a0.

    # starting stack at boot_stack_start
    la      sp, boot_stack_start_ptr
    REG_L   sp, 0(sp)
    li      t0, 2 * 4096 # 2 page stack
    addi    t1, a0, 1 # t1 = mhartid + 1
    mul     t0, t0, t1 # t1 = SIZE * (mhartid + 1)
    add     sp, sp, t0 # sp = boot_stack_start + (SIZE * (mhartid + 1))

    # set tp to what we want, use t2 as well if needed

    # TODO: HACKY
    li tp, 0xF0000000
    li t2, 0xc202004
    REG_S t2, 56(tp)

    # zero out bss
    la      t0, bss_start_ptr
    REG_L   t0, 0(t0)
    la      t1, bss_end_ptr
    REG_L   t1, 0(t1)
    beq     t0, t1, 1f
0:
    REG_S   zero, (t0)
    add     t0, t0, SZREG
    blt     t1, t0, 0b  # if t1 is less than t0
1:
    # allocate space for the scratch pad
    addi    sp, sp, -32 * SZREG

    # disable supervisor address translation and protection (paging), since we'd identity map anyways
    csrw    satp, 0
    # flush tlb and whatnot
    sfence.vma zero, zero

    # enable supervisor interrupts
    li      t0, SIE_SEIE
    or      t0, t0, SIE_STIE
    or      t0, t0, SIE_SSIE
    csrs    sie, t0

    la      t0, main_ptr
    REG_L   t0, 0(t0)

    jr      t0

.balign 8
boot_stack_start_ptr: .quad boot_stack_start

.balign 8
main_ptr: .quad main

.balign 8
bss_start_ptr: .quad _bssStart

.balign 8
bss_end_ptr: .quad _bssEnd

.global secondary_core_stack
secondary_core_stack:
    .quad 0

.section .text

# When the processor gets an interrupt or traps, it jumps here and sets some CSRs
# like sstatus, sbadaddr, stval etc... The only problem is that we don't switch
# stacks when jumping from userspace. To fix this, we gotta do some extra nonsense
# to avoid losing register states
#define TP_BAK1 0
#define TP_BAK2 1
#define TP_BAK3 2
#define TP_TCA  3
#define TP_INTERVAL  4
#define TP_KERNEL_SP 5
#define TP_USER_SP 6
#define TF_SIZE_ON_STACK (36 * SZREG)

.balign 4
GLOBAL(kernel_vec)

    # make room to save registers.
    #addi sp, sp, -288 # 38 registers

    # save the registers.
    SAVE_CALLER_GPRS()

    # call the C trap handler in idt.c
    move a0, sp /* arg0 <- nk_regs */

    # Save trap registers (so they can be restored if we swap threads)
    addi sp, sp, -(6 * 8)
    csrr t0, sepc
    REG_S t0, ROFF(0, sp)
    csrr t0, sstatus
    REG_S t0, ROFF(1, sp)
    csrr t0, stval
    REG_S t0, ROFF(2, sp)
    csrr t0, scause
    REG_S t0, ROFF(3, sp)
    csrr t0, sscratch
    REG_S t0, ROFF(4, sp)
    csrr t0, sbadaddr
    REG_S t0, ROFF(5, sp)
    move a1, sp /* arg1 <- tregs */

    call kernel_trap

    li t0, 0
    beq t0, a0, after_switch

    # We need to do a thread switch
    call nk_thread_switch

after_switch:
    REG_L t0, ROFF(0, sp)
    csrw sepc, t0
    REG_L t0, ROFF(1, sp)
    csrw sstatus, t0
    REG_L t0, ROFF(2, sp)
    csrw stval, t0
    REG_L t0, ROFF(3, sp)
    csrw scause, t0
    REG_L t0, ROFF(4, sp)
    csrw sscratch, t0
    REG_L t0, ROFF(5, sp)
    csrw sbadaddr, t0
    addi sp, sp, (6 * 8)

    # restore registers.
    RESTORE_CALLER_GPRS()

    # return to whatever we were doing in the kernel.
    sret

END(kernel_vec)

ENTRY(nk_fp_save)
    fsd f0,  8 * 0(a0)
    fsd f1,  8 * 1(a0)
    fsd f2,  8 * 2(a0)
    fsd f3,  8 * 3(a0)
    fsd f4,  8 * 4(a0)
    fsd f5,  8 * 5(a0)
    fsd f6,  8 * 6(a0)
    fsd f7,  8 * 7(a0)
    fsd f8,  8 * 8(a0)
    fsd f9,  8 * 9(a0)
    fsd f10, 8 * 10(a0)
    fsd f11, 8 * 11(a0)
    fsd f12, 8 * 12(a0)
    fsd f13, 8 * 13(a0)
    fsd f14, 8 * 14(a0)
    fsd f15, 8 * 15(a0)
    fsd f16, 8 * 16(a0)
    fsd f17, 8 * 17(a0)
    fsd f18, 8 * 18(a0)
    fsd f19, 8 * 19(a0)
    fsd f20, 8 * 20(a0)
    fsd f21, 8 * 21(a0)
    fsd f22, 8 * 22(a0)
    fsd f23, 8 * 23(a0)
    fsd f24, 8 * 24(a0)
    fsd f25, 8 * 25(a0)
    fsd f26, 8 * 26(a0)
    fsd f27, 8 * 27(a0)
    fsd f28, 8 * 28(a0)
    fsd f29, 8 * 29(a0)
    fsd f30, 8 * 30(a0)
    fsd f31, 8 * 31(a0)
    ret
END(nk_fp_save)

ENTRY(nk_fp_restore)
    fld f0,  8 * 0(a0)
    fld f1,  8 * 1(a0)
    fld f2,  8 * 2(a0)
    fld f3,  8 * 3(a0)
    fld f4,  8 * 4(a0)
    fld f5,  8 * 5(a0)
    fld f6,  8 * 6(a0)
    fld f7,  8 * 7(a0)
    fld f8,  8 * 8(a0)
    fld f9,  8 * 9(a0)
    fld f10, 8 * 10(a0)
    fld f11, 8 * 11(a0)
    fld f12, 8 * 12(a0)
    fld f13, 8 * 13(a0)
    fld f14, 8 * 14(a0)
    fld f15, 8 * 15(a0)
    fld f16, 8 * 16(a0)
    fld f17, 8 * 17(a0)
    fld f18, 8 * 18(a0)
    fld f19, 8 * 19(a0)
    fld f20, 8 * 20(a0)
    fld f21, 8 * 21(a0)
    fld f22, 8 * 22(a0)
    fld f23, 8 * 23(a0)
    fld f24, 8 * 24(a0)
    fld f25, 8 * 25(a0)
    fld f26, 8 * 26(a0)
    fld f27, 8 * 27(a0)
    fld f28, 8 * 28(a0)
    fld f29, 8 * 29(a0)
    fld f30, 8 * 30(a0)
    fld f31, 8 * 31(a0)
    ret
END(nk_fp_restore)
