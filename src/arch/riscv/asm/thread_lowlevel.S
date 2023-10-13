
#include <arch/riscv/asm/lowlevel.h>

#include <nautilus/thread.h>

#define CTX_SIZE (32 * SZREG)

/*
 * We come in like this:
 * ----------
 * thread_ptr
 * ----------
 * ret addr
 * ----------
 *
 */
.section .text

#define SSTATUS_SIE (1L << 1)  // Supervisor Interrupt Enable
#define SIE_SEIE (1L << 9)  // external
#define SIE_STIE (1L << 5)  // timer
#define SIE_SSIE (1L << 1)  // software

ENTRY(nk_thread_switch)

        SAVE_CALLER_GPRS()

	lwu t0, 8(tp)               # increment interrupt nesting level
	addi t0, t0, 1              # pretend we are doing an interrupt-driven switch
	sw  t0, 8(tp)               # to make the sums work out

        # Zero ra so we know if we entered through thread_switch_intr_entry or thread_switch
        add ra, x0, x0

ENTRY(nk_thread_switch_intr_entry)
        addi sp, sp, -16
        REG_S ra, ROFF(0, sp)

        SAVE_CALLEE_GPRS()

	# load cur_thread from the cpu structure
	REG_L t0,  ROFF(0,  tp)
	# store the stack pointer into it
	REG_S sp,  ROFF(0,  t0)

# On a thread exit we must avoid saving thread state
# or using the stack at all since at this point the thread and its stack
# might already be deleted.  Interrupts are off, however.
# We also want to save the cost of any state saves
# We should get here via a JUMP
# Note that the first three instructions establish the new stack
# and thread structure
ENTRY(nk_thread_switch_on_exit)

	# set the CPUs cur_thread to the next thread (argument to this function)
	REG_S a0,  ROFF(0, tp)
	# load the stack pointer from the current (new) thread
	REG_L sp,  ROFF(0, a0)

	lwu t0, 8(tp)                // decrement interrupt nesting level
	addi t0, t0, -1              // we may now be leaving interrupt context
	sw  t0, 8(tp)

        RESTORE_CALLEE_GPRS()

        REG_L ra, ROFF(0, sp)
        addi sp, sp, 16

        beqz ra, __non_intr_return

        ret

__non_intr_return:
        // ensure enable supervisor interrupts
        li      t0, SIE_SEIE
	or      t0, t0, SIE_STIE
	or      t0, t0, SIE_SSIE
        csrs    sie, t0

	csrr    t0, sstatus
	li      t1, SSTATUS_SIE
	or      t0, t0, t1
	csrw    sstatus, t0

	# restore registers
        RESTORE_CALLER_GPRS()

	ret                          // and off we go

// rdi = new thread
// rsi = target address to write
// edx = value to write (as 4 bytes)
ENTRY(nk_thread_switch_exit_helper)
	// reset interrupt nesting
	// the context switch will decrement it to zero
	li t0, 1
	sw t0, 8(tp)

	// reset preemption disable level
	li t0, 0
	sw t0, 12(tp)

	// note that interrupts are still off

	// We now mark ourselves as toast for the reaper
	sw a2, 0(a1)

	// At this point, we are racing with the reaper on a different core,
	// which might take away our stack.  However, we have interrupts
	// off locally, so as long as *we* don't use the stack, we should be OK
	j nk_thread_switch_on_exit

	// and this should never happen...
	ret



ENTRY(nk_thread_entry)

	REG_L s0, ROFF(0, sp) // Function
	REG_L s1, ROFF(1, sp) // Thread Cleanup

	addi sp, sp, 2*SZREG

	jalr s0

	jr s1

