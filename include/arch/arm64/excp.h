#ifndef __ARM64_EXCP_H__
#define __ARM64_EXCP_H__

// These are not all possible valid values of ESR's class field
#define ESR_CLS_UNKNOWN 0x0
#define ESR_CLS_TRAPPED_WF_INSTR 0x1
#define ESR_CLS_BRANCH_TARGET 0xD
#define ESR_CLS_ILLEGAL_EXEC_STATE 0xE
#define ESR_CLS_SYSCALL32 0x11
#define ESR_CLS_SYSCALL64 0x15
#define ESR_CLS_SVE_DISABLED 0x19
#define ESR_CLS_IABORT_FROM_LOWER_EL 0x20
#define ESR_CLS_IABORT_FROM_SAME_EL 0x21
#define ESR_CLS_PC_UNALIGNED 0x22
#define ESR_CLS_DABORT_FROM_LOWER_EL 0x24
#define ESR_CLS_DABORT_FROM_SAME_EL 0x25
#define ESR_CLS_SP_UNALIGNED 0x26
#define ESR_CLS_FP32_TRAP 0x28
#define ESR_CLS_FP64_TRAP 0x2C
#define ESR_CLS_SERROR_INT 0x2F

typedef union esr_el1 {
  uint64_t raw;
  struct {
    uint_t iss : 25;
    uint_t inst_length : 1;
    uint_t syndrome : 6;
    uint_t iss2 : 5;
    // Rest reserved
  };
} esr_el1_t;

struct __attribute__((packed)) excp_entry_info {
  uint64_t status;
  uint64_t elr;
  esr_el1_t esr;
  uint64_t far;
};

typedef int(*irq_handler_t)(excp_entry_t *excp, ulong_t vec, void *state);
typedef int(*excp_handler_t)(struct nk_regs *regs, struct excp_entry_info *info, uint8_t el_from, uint8_t sync, void *state);

typedef struct irq_handler_desc {
  irq_handler_t handler;
  void *state;
} irq_handler_desc_t;
extern irq_handler_desc_t *irq_handler_desc_table;

typedef struct excp_handler_desc {
  excp_handler_t handler;
  void *state;
} excp_handler_desc_t;
extern excp_handler_desc_t *excp_handler_desc_table;

void excp_assign_irq_handler(int irq, irq_handler_t handler, void *state);

// Returns the removed state of the handler if it exists
void excp_assign_excp_handler(uint32_t syndrome, excp_handler_t handler, void *state);
void *excp_remove_excp_handler(uint32_t syndrome);

#endif
