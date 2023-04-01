#include <asm/lowlevel.h>
#include <nautilus/gdt.h>
#include <nautilus/thread.h>

.section .text

.global plic_claim_register
plic_claim_register:
    .quad 0xc202004

ENTRY(poll_irq)
    # TODO: HACKY (56 is hardcoded offset of plic_claim_register in the cpu struct)
    la      a0, plic_claim_register
    bnez    a0, .hit
    ret
.hit:
    tail    poll_handle_irq
