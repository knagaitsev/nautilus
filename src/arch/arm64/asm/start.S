

# start.S - Boot level functionality for ChariotOS on arm64

# AArch64 mode
 
# To keep this in the first portion of the binary.
.section .boot

.EXTERN secondary_init
 
.globl nautilus_entry
# Entry point for the kernel. Registers:
# x0 -> 32 bit pointer to DTB in memory (primary core only) / 0 (secondary cores)
# x1 -> 0
# x2 -> 0
# x3 -> 0
# x4 -> 32 bit kernel entry point, _start location


tmp                     .req x9

nautilus_entry:

    # Check processor ID is zero (executing on main core), else hang
    mrs     x1, mpidr_el1
    and     x1, x1, 3
    cbz     x1, 2f

    /* enable caches so atomics and spinlocks work */
    mrs     tmp, sctlr_el1
    orr     tmp, tmp, (1<<12) /* Enable icache */
    orr     tmp, tmp, (1<<2)  /* Enable dcache/ucache */
    orr     tmp, tmp, (1<<3)  /* Enable Stack Alignment Check EL1 */
    orr     tmp, tmp, (1<<4)  /* Enable Stack Alignment Check EL0 */
    bic     tmp, tmp, (1<<1)  /* Disable Alignment Checking for EL1 EL0 */
    msr     sctlr_el1, tmp

    # We're not on the main core, so hang in an infinite wait loop
1:  wfe
    b secondary_start
    b  1b


2:  # We're on the main core!

    # Configure the processor to use a different stack for each EL
    msr SPsel, 1
    
    # Set stack to start below our code
    ldr     x1, =boot_stack_end
    mov     sp, x1

    # Install the interrupt vector tables (save the dtb)
    mov x20, x0
    bl      __install_iv_tables
    mov x0, x20

    # Jump to our kernel_main routine in C (make sure it doesn't return)
4:  

    bl      init
    # In case it does return, halt the master core too
    # Load a obvious value into x29 so it will be clear to the debugger
    ldr x1, =0xCAFEBABE
    b .


.EXTERN __secondary_stack
.GLOBAL secondary_start
secondary_start:
    
    ldr x1, =__secondary_stack
    ldr x1, [x1]
    mov sp, x1

    bl __install_iv_tables

    bl secondary_init

    ldr x1, =0xCAFEBABE
    b .

.align 16
boot_stack:
.space 4096
boot_stack_end:
