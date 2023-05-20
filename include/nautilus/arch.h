#pragma once

#include <nautilus/nautilus.h>

typedef struct excp_entry_state excp_entry_t;
typedef struct mmap_info mmap_info_t;
typedef struct mem_map_entry mem_map_entry_t;

struct naut_info;
// #include <nautilus/idt.h>
// #include <nautilus/mm.h>

/* arch specific */
void arch_enable_ints(void);
void arch_disable_ints(void);
int arch_ints_enabled(void);

void arch_irq_enable(int irq);
void arch_irq_disable(int irq);
void arch_irq_install(int irq, int (*handler)(excp_entry_t *excp,
                                              ulong_t vector, void *state),
                      void *state);
void arch_irq_uninstall(int irq);

int arch_irq_find_and_reserve_range(uint64_t num_entries, int aligned, uint64_t *first);

int arch_early_init(struct naut_info *naut);
int arch_numa_init(struct sys_info *sys);

void arch_detect_mem_map(mmap_info_t *mm_info, mem_map_entry_t *memory_map,
                         unsigned long mbd);
void arch_reserve_boot_regions(unsigned long mbd);

uint32_t arch_cycles_to_ticks(uint64_t cycles);
uint32_t arch_realtime_to_ticks(uint64_t ns);
uint64_t arch_realtime_to_cycles(uint64_t ns);
uint64_t arch_cycles_to_realtime(uint64_t cycles);

void arch_update_timer(uint32_t ticks, nk_timer_condition_t cond);
void arch_set_timer(uint32_t ticks);
int arch_read_timer(void);
int arch_timer_handler(excp_entry_t *excp, ulong_t vec, void *state);

uint64_t arch_read_timestamp(void);

void arch_print_regs(struct nk_regs *r);
void *arch_read_sp(void);
void arch_relax(void);
void arch_halt(void);

#ifdef NAUT_CONFIG_ARCH_X86
#include <arch/x64/arch.h>
#elif NAUT_CONFIG_ARCH_RISCV
#include <arch/riscv/arch.h>
#elif NAUT_CONFIG_ARCH_ARM64
#include <arch/arm64/arch.h>
#else
#error "Unsupported architecture"
#endif
