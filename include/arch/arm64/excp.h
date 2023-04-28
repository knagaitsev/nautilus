#ifndef __ARM64_EXCP_H__
#define __ARM64_EXCP_H__

typedef int(*irq_handler_t)(excp_entry_t *excp, ulong_t vec, void *state);

// Requires boot mem allocator
int excp_init(void);

void excp_assign_irq_handler(int irq, irq_handler_t handler, void *state);

// Returns the removed state of the handler if it exists
void *excp_remove_irq_handler(int irq);

#endif
