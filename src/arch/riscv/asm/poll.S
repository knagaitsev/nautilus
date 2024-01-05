#include <asm/lowlevel.h>
#include <nautilus/thread.h>

.section .text

ENTRY(poll_irq)
    # TODO: HACKY (56 is hardcoded offset of plic_claim_register in the cpu struct)
    ld      a0, 56(tp)
    lw      a0, 0(a0)
    bnez    a0, .hit
    ret
.hit:
    tail    poll_handle_irq
