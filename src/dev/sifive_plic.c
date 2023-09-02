
#include <nautilus/irqdev.h>
#include <nautilus/interrupt.h>
#include <nautilus/of/fdt.h>
#include <nautilus/of/dt.h>

#ifndef NAUT_CONFIG_DEBUG_PLIC
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

#define PLIC_INFO(fmt, args...)     INFO_PRINT("[PLIC] " fmt, ##args)
#define PLIC_DEBUG(fmt, args...)    DEBUG_PRINT("[PLIC] " fmt, ##args)
#define PLIC_ERROR(fmt, args...)    ERROR_PRINT("[PLIC] " fmt, ##args)

#define MREG(x) *((uint32_t *)(PLIC + (x)))
#define READ_REG(x) *((uint32_t *)((x)))

#define PLIC_PRIORITY_BASE 0x000000U

#define PLIC_PENDING_BASE 0x1000U

#define PLIC_ENABLE_BASE 0x002000U
#define PLIC_ENABLE_STRIDE 0x80U
#define IRQ_ENABLE 1
#define IRQ_DISABLE 0

#define PLIC_CONTEXT_BASE 0x200000U
#define PLIC_CONTEXT_STRIDE 0x1000U
#define PLIC_CONTEXT_THRESHOLD 0x0U
#define PLIC_CONTEXT_CLAIM 0x4U

#define PLIC_PRIORITY(n) (PLIC_PRIORITY_BASE + (n) * sizeof(uint32_t))
#define PLIC_ENABLE(n, h) (contexts[h].enable_offset + ((n) / 32) * sizeof(uint32_t))
#define PLIC_THRESHOLD(h) (contexts[h].context_offset + PLIC_CONTEXT_THRESHOLD)
#define PLIC_CLAIM(h) (contexts[h].context_offset + PLIC_CONTEXT_CLAIM)

struct sifive_plic_context 
{
  void *enable_offset;
  void *context_offset;
};

struct sifive_plic 
{
  void *mmio_base;
  int mmio_size;

  struct sifive_plic_context *contexts;

  uint32_t num_irqs;
};

static int plic_dev_get_characteristics(void *state, struct nk_irq_dev_characteristics *c)
{
  memset(c, 0, sizeof(c));
  return 0;
}

static int plic_dev_initialize_cpu(void *state) 
{
  struct sifive_plic *plic = (struct sifive_plic*)plic;
  MREG(PLIC_THRESHOLD(my_cpu_id())) = 0;
  return 0;
}

static int plic_dev_translate_irqs(void *state, nk_dev_info_type_t type, void *raw_irqs, int raw_irqs_len, nk_irq_t *buffer, int *buf_count) 
{
  struct sifive_plic *plic = (struct sifive_plic*)state;
  if(type == NK_DEV_INFO_TYPE_OF) {
    // KJH - I'm assuming it's just 1 cell representing the IRQ number
    uint32_t *raw_cells = (uint32_t*)raw_irqs;
    uint32_t num_cells = raw_irqs_len / 4;
    num_cells = num_cells < *buf_count ? num_cells : *buf_count;
    for(int i = 0; i < num_cells; i++) {
      buffer[i] = be32toh(raw_cells[i]);
    }
    *buf_count = num_cells;
  } else {
    PLIC_ERROR("Cannot translate nk_dev_info IRQ's of type other than OF!\n");
    return -1;
  }
  return 0;
}

static int plic_dev_ack_irq(void *state, nk_irq_t *irq) 
{
  struct sifive_plic *plic = (struct sifive_plic*)state;
  // TODO
  return 0;
}

static int plic_dev_eoi_irq(void *state, nk_irq_t irq) 
{
  struct sifive_plic *plic = (struct sifive_plic*)state;
  // TODO
  return 0;
}

static int plic_dev_enable_irq(void *state, nk_irq_t irq) 
{
  struct sifive_plic *plic = (struct sifive_plic*)state;
  // TODO
  return 0;
}

static int plic_dev_disable_irq(void *state, nk_irq_t irq) 
{
  struct sifive_plic *plic = (struct sifive_plic*)state;
  // TODO
  return 0;
}

static int plic_dev_irq_status(void *state, nk_irq_t irq) 
{
  struct sifive_plic *plic = (struct sifive_plic*)state;

  int status = 0;

  status |= 0/*TODO*/ ?
    IRQ_DEV_STATUS_ENABLED : 0;

  status |= 0/*TODO*/ ?
    IRQ_DEV_STATUS_PENDING : 0;

  status |= 0/*TODO*/ ?
    IRQ_DEV_STATUS_ACTIVE : 0;

  return status;
}

static struct nk_irq_dev_int plic_dev_int = {
  .get_characteristics = plic_dev_get_characteristics,
  .initialize_cpu = plic_dev_initialize_cpu, 
  .ack_irq = plic_dev_ack_irq,
  .eoi_irq = plic_dev_eoi_irq,
  .enable_irq = plic_dev_enable_irq,
  .disable_irq = plic_dev_disable_irq,
  .irq_status = plic_dev_irq_status,
  .translate_irqs = plic_dev_translate_irqs
};

static int plic_init_dev_info(struct nk_dev_info *info) 
{
  int did_alloc = 0;
  int did_register = 0;

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

  nk_alloc_ivec_descs(0, plic->num_irqs, dev, NK_IVEC_DESC_TYPE_DEFAULT, 0);
  PLIC_DEBUG("Allocated Vectors!\n");

  if(nk_assign_all_cpus_irq_dev(dev)) {
    PLIC_ERROR("Failed to assign the PLIC to all CPU's in the system!\n");
    goto err_exit;
  }

  PLIC_DEBUG("initialized globally!\n");

  return 0;

err_exit:
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
  PLIC_DEBUG("initialized\n");
}

