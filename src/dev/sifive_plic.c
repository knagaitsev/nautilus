
#include <nautilus/irqdev.h>
#include <nautilus/interrupt.h>
#include <nautilus/of/fdt.h>
#include <nautilus/of/dt.h>
#include <nautilus/endian.h>

#ifndef NAUT_CONFIG_DEBUG_SIFIVE_PLIC
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

#define PLIC_INFO(fmt, args...)     INFO_PRINT("[PLIC] " fmt, ##args)
#define PLIC_DEBUG(fmt, args...)    DEBUG_PRINT("[PLIC] " fmt, ##args)
#define PLIC_ERROR(fmt, args...)    ERROR_PRINT("[PLIC] " fmt, ##args)

#define MREG(plic_ptr, x) *((uint32_t *)((plic_ptr)->mmio_base + (uint64_t)(x)))
#define READ_REG(x) *((uint32_t *)((x)))

#define PLIC_PRIORITY_OFFSET 0x000000U
#define PLIC_PENDING_OFFSET 0x1000U
#define PLIC_ENABLE_OFFSET 0x002000U
#define PLIC_ENABLE_STRIDE 0x80U

#define IRQ_ENABLE 1
#define IRQ_DISABLE 0

#define PLIC_CONTEXT_OFFSET 0x200000U
#define PLIC_CONTEXT_STRIDE 0x1000U
#define PLIC_CONTEXT_THRESHOLD 0x0U
#define PLIC_CONTEXT_CLAIM 0x4U

#define PLIC_PRIORITY(plic_ptr, n) (PLIC_PRIORITY_OFFSET + (n) * sizeof(uint32_t))
#define PLIC_ENABLE(plic_ptr, n, h) ((uint64_t)(plic_ptr)->contexts[h].enable_offset + ((n) / 32) * sizeof(uint32_t))
#define PLIC_THRESHOLD(plic_ptr, h) ((uint64_t)(plic_ptr)->contexts[h].context_offset + PLIC_CONTEXT_THRESHOLD)
#define PLIC_CLAIM(plic_ptr, h) ((uint64_t)(plic_ptr)->contexts[h].context_offset + PLIC_CONTEXT_CLAIM)

#define PLIC_BASE_HWIRQ 1

#define MAX_NUM_PLICS 1

struct sifive_plic *global_plic_array[MAX_NUM_PLICS];

struct sifive_plic_context 
{
  nk_irq_t irq;
  void *enable_offset;
  void *context_offset;
};

struct sifive_plic 
{
  void *mmio_base;
  int mmio_size;

  int num_contexts;
  struct sifive_plic_context *contexts;

  uint32_t num_irqs;
  nk_irq_t irq_base;
  struct nk_irq_desc *irq_descs;
};

static int plic_dev_get_characteristics(void *state, struct nk_irq_dev_characteristics *c)
{
  memset(c, 0, sizeof(c));
  return 0;
}

int plic_percpu_init(void) 
{
  for(int i = 0; i < MAX_NUM_PLICS; i++) {
    struct sifive_plic *plic = (struct sifive_plic*)global_plic_array[i];
    if(plic != NULL) {
      MREG(plic, PLIC_THRESHOLD(plic,my_cpu_id())) = 0;
    }
  }
  return 0;
}

static int plic_dev_revmap(void *state, nk_hwirq_t hwirq, nk_irq_t *irq) 
{
  struct sifive_plic *plic = (struct sifive_plic*)state;
  if(hwirq < PLIC_BASE_HWIRQ || hwirq >= PLIC_BASE_HWIRQ + plic->num_irqs) {
    return -1;
  }
  *irq = plic->irq_base + hwirq;
  return 0;
}

static int plic_dev_translate(void *state, nk_dev_info_type_t type, void *raw, nk_hwirq_t *out) 
{
  struct sifive_plic *plic = (struct sifive_plic*)state;
  switch(type) {
    case NK_DEV_INFO_OF:
      uint32_t *raw_cells = (uint32_t*)raw;
      *out = be32toh(raw_cells[0]);
      return 0;
    default:
      PLIC_ERROR("Cannot translate nk_dev_info IRQ's of type other than OF!\n");
      return -1;
  }
}

static int plic_dev_ack_irq(void *state, nk_hwirq_t *hwirq) 
{
  struct sifive_plic *plic = (struct sifive_plic*)state;
  *hwirq = MREG(plic, PLIC_CLAIM(plic,my_cpu_id()));
  return 0;
}

static int plic_dev_eoi_irq(void *state, nk_hwirq_t hwirq) 
{
  struct sifive_plic *plic = (struct sifive_plic*)state;
  MREG(plic, PLIC_CLAIM(plic,my_cpu_id())) = hwirq;
  return 0;
}

// KJH - Why is priority unused?
static void plic_toggle(struct sifive_plic *plic, int hart, int hwirq, int priority, bool_t enable) 
{
    if (hwirq == 0) return;

    // TODO: this masks the interrupt so we can use it, but it is a bit ugly
    // alternatively what Nick does is iterate through all the interrupts for each
    // PLIC during init and mask them
    // https://github.com/ChariotOS/chariot/blob/7cf70757091b79cbb102a943a963dce516a8c667/arch/riscv/plic.cpp#L85-L88
    MREG(plic, 4 * hwirq) = 7;

    uint32_t mask = (1 << (hwirq % 32));
    uint32_t val = MREG(plic, PLIC_ENABLE(plic, hwirq, hart));

    // printk("Hart: %d, hwirq: %d, v1: %d\n", hart, hwirq, val);
    if (enable)
        val |= mask;
    else
        val &= ~mask;

    //printk("Hart: %d, hwirq: %d, irq: %x, *irq: %x\n", hart, hwirq, &MREG(PLIC_ENABLE(plic, hwirq, hart)), val);

    MREG(plic, PLIC_ENABLE(plic, hwirq, hart)) = val;
}

static int plic_dev_enable_irq(void *state, nk_hwirq_t hwirq) 
{
  struct sifive_plic *plic = (struct sifive_plic*)state;
  plic_toggle(plic, my_cpu_id(), hwirq, 0, 1);
  return 0;
}

static int plic_dev_disable_irq(void *state, nk_hwirq_t hwirq) 
{
  struct sifive_plic *plic = (struct sifive_plic*)state; 
  plic_toggle(plic, my_cpu_id(), hwirq, 0, 0);
  return 0;
}

static int plic_dev_irq_status(void *state, nk_hwirq_t hwirq) 
{
  struct sifive_plic *plic = (struct sifive_plic*)state;

  int status = 0;

  uint32_t enable_val = MREG(plic, PLIC_ENABLE(plic, hwirq, my_cpu_id()));

  status |= enable_val & (1<<(hwirq % 32)) ?
    IRQ_STATUS_ENABLED : 0;

  status |= MREG(plic, PLIC_PENDING_OFFSET) ?
    IRQ_STATUS_PENDING : 0;

  // KJH - No idea how to get this right now (Possible we can't)
  status |= 0/*TODO*/ ?
    IRQ_STATUS_ACTIVE : 0;

  return status;
}

static int plic_interrupt_handler(struct nk_irq_action *action, struct nk_regs *regs, void *state) 
{
  struct sifive_plic *plic = (struct sifive_plic *)state;

  nk_hwirq_t hwirq;
  plic_dev_ack_irq(state, &hwirq);

  nk_irq_t irq = NK_NULL_IRQ;
  if(plic_dev_revmap(state, hwirq, &irq)) {
    return -1;
  }

  struct nk_irq_desc *desc = nk_irq_to_desc(irq);
  if(desc == NULL) {
    return -1;
  }

  nk_handle_irq_actions(desc, regs);

  plic_dev_eoi_irq(state, hwirq);

  return 0;
}

static struct nk_irq_dev_int plic_dev_int = {
  .get_characteristics = plic_dev_get_characteristics,
  .ack_irq = plic_dev_ack_irq,
  .eoi_irq = plic_dev_eoi_irq,
  .enable_irq = plic_dev_enable_irq,
  .disable_irq = plic_dev_disable_irq,
  .irq_status = plic_dev_irq_status,
  .revmap = plic_dev_revmap,
  .translate = plic_dev_translate
};

static int plic_init_dev_info(struct nk_dev_info *info) 
{
  int did_alloc = 0;
  int did_register = 0;
  int did_context_alloc = 0;

  if(info->type != NK_DEV_INFO_OF) {
    PLIC_ERROR("Currently only support device tree initialization!\n");
    goto err_exit;
  }

  struct sifive_plic *plic = malloc(sizeof(struct sifive_plic));

  if(plic == NULL) 
  {
    PLIC_ERROR("Failed to allocate PLIC struct!\n");
    goto err_exit;
  }
  did_alloc = 1;

  memset(plic, 0, sizeof(struct sifive_plic));

  if(nk_dev_info_read_register_block(info, &plic->mmio_base, &plic->mmio_size)) {
    PLIC_ERROR("Failed to read register block!\n");
    goto err_exit;
  }

  if(nk_dev_info_read_u32(info, "riscv,ndev", &plic->num_irqs)) {
    PLIC_ERROR("Failed to read number of IRQ's!\n");
    goto err_exit;
  }

  PLIC_DEBUG("mmio_base = %p, mmio_size = %p, num_irqs = %u\n",
      plic->mmio_base, plic->mmio_size, plic->num_irqs);

  struct nk_irq_dev *dev = nk_irq_dev_register("plic", 0, &plic_dev_int, (void*)plic);

  if(dev == NULL) {
    PLIC_ERROR("Failed to register IRQ device!\n");
    goto err_exit;
  } else {
    did_register = 1;
  }

  nk_dev_info_set_device(info, (struct nk_dev*)dev);

  if(nk_request_irq_range(plic->num_irqs, &plic->irq_base)) {
    PLIC_ERROR("Failed to get IRQ numbers!\n");
    goto err_exit;
  }

  PLIC_DEBUG("plic->irq_base = %u\n", plic->irq_base);

  plic->irq_descs = nk_alloc_irq_descs(plic->num_irqs, PLIC_BASE_HWIRQ, NK_IRQ_DESC_FLAG_PERCPU, dev);
  if(plic->irq_descs == NULL) {
    PLIC_ERROR("Failed to allocate IRQ descriptors!\n");
    goto err_exit;
  }

  if(nk_assign_irq_descs(plic->num_irqs, plic->irq_base, plic->irq_descs)) {
    PLIC_ERROR("Failed to assign IRQ descriptors!\n");
    goto err_exit;
  }

  PLIC_DEBUG("Created and Assigned IRQ Descriptors\n");

  plic->num_contexts = nk_dev_info_num_irq(info);
  PLIC_DEBUG("num_contexts = %u\n", plic->num_contexts);

  plic->contexts = malloc(sizeof(struct sifive_plic_context) * plic->num_contexts);
  if(plic->contexts == NULL) {
    PLIC_ERROR("Failed to allocate PLIC contexts!\n");
    goto err_exit;
  }
  did_context_alloc = 1;
  
  for(int i = 0; i < plic->num_contexts; i++) {
    plic->contexts[i].irq = nk_dev_info_read_irq(info, i);
    if(plic->contexts[i].irq == NK_NULL_IRQ) {
      PLIC_DEBUG("Context (%u) does not exist\n");
      continue;
    }
    plic->contexts[i].enable_offset = (void*)(uint64_t)(PLIC_ENABLE_OFFSET + (i * PLIC_ENABLE_STRIDE)); 
    plic->contexts[i].context_offset = (void*)(uint64_t)(PLIC_CONTEXT_OFFSET + (i * PLIC_CONTEXT_STRIDE));
    if(nk_irq_add_handler_dev(plic->contexts[i].irq, plic_interrupt_handler, (void*)plic, (struct nk_dev*)dev)) {
      PLIC_ERROR("Failed to assign IRQ handler to context (%u)!\n");
      goto err_exit;
    }
    nk_unmask_irq(plic->contexts[i].irq);
  }

  PLIC_DEBUG("initialized globally!\n");

  return 0;

err_exit:
  if(did_context_alloc) {
    free(plic->contexts);
  }
  if(did_alloc) {
    free(plic);
  }
  if(did_register) {
    nk_irq_dev_unregister(dev);
  }
  return -1;
}

static const char * plic_properties_names[] = {
  "interrupt-controller"
};

static const struct of_dev_properties plic_of_properties = {
  .names = plic_properties_names,
  .count = 1
};

static const struct of_dev_id plic_of_dev_ids[] = {
  { .properties = &plic_of_properties, .compatible = "sifive,plic-1.0.0" },
  { .properties = &plic_of_properties, .compatible = "riscv,plic0" }
};

static const struct of_dev_match plic_of_dev_match = {
  .ids = plic_of_dev_ids,
  .num_ids = sizeof(plic_of_dev_ids)/sizeof(struct of_dev_id),
  .max_num_matches = 1
};

int plic_init(void) 
{ 
  PLIC_DEBUG("init\n");
  return of_for_each_match(&plic_of_dev_match, plic_init_dev_info);
}

