
#include <nautilus/irqdev.h>
#include <nautilus/interrupt.h>
#include <nautilus/naut_types.h>
#include <nautilus/endian.h>
#include <nautilus/of/dt.h>

#ifndef NAUT_CONFIG_DEBUG_PRINTS
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

#define HLIC_INFO(fmt, args...)     INFO_PRINT("[HLIC] " fmt, ##args)
#define HLIC_DEBUG(fmt, args...)    DEBUG_PRINT("[HLIC] " fmt, ##args)
#define HLIC_ERROR(fmt, args...)    ERROR_PRINT("[HLIC] " fmt, ##args)

#define HLIC_NUM_IRQ 64
#define HLIC_BASE_HWIRQ 0

#define CLINT(hlic) ({uint64_t stvec_reg = read_csr(stvec); (stvec_reg & 0b11) < 2;})

struct hlic 
{
  nk_irq_t irq_base;
  struct nk_irq_desc *irq_descs;
};

static int hlic_dev_get_characteristics(void *state, struct nk_irq_dev_characteristics *c)
{
  memset(c, 0, sizeof(c));
  return 0;
}

static int hlic_dev_revmap(void *state, nk_hwirq_t hwirq, nk_irq_t *irq) 
{
  struct hlic *hlic = (struct hlic*)state;
  if(hwirq <= 0) {
    return -1;
  }
  else if(hwirq < HLIC_NUM_IRQ + HLIC_BASE_HWIRQ) {
    *irq = hlic->irq_base + hwirq;
    return 0;
  }

  return -1;
}

static int hlic_dev_translate(void *state, nk_dev_info_type_t type, void *raw_irq, nk_hwirq_t *out_hwirq) 
{
  struct hlic *hlic = (struct hlic*)state;
  switch(type) {
    case NK_DEV_INFO_OF:
      uint32_t *raw_cells = (uint32_t*)raw_irq;
      *out_hwirq = be32toh(raw_cells[0]);
      if(*out_hwirq <= 0 || *out_hwirq > HLIC_NUM_IRQ + HLIC_BASE_HWIRQ) {
        HLIC_ERROR("Invalid hwirq number when translating raw OF IRQ: (hwirq = 0x%x)!\n", *out_hwirq);
        return -1;
      }
      return 0;
    default:
      HLIC_ERROR("Cannot translate nk_dev_info IRQ's of type other than OF!\n");
      return -1;
  }
}

static int hlic_dev_ack_irq(void *state, nk_hwirq_t *hwirq) 
{
  struct hlic *hlic = (struct hlic*)state;
  *hwirq = (nk_hwirq_t)read_csr(scause);
  return 0;
}

static int hlic_dev_eoi_irq(void *state, nk_hwirq_t hwirq) 
{
  struct hlic *hlic = (struct hlic*)state;
  // Do nothing
  return 0;
}

static int hlic_dev_enable_irq(void *state, nk_hwirq_t hwirq) 
{
  struct hlic *hlic = (struct hlic*)state;

  if(CLINT(hlic)) {
    set_csr(sie, 1ULL<<(uint64_t)hwirq);
  } 

  return 0;
}

static int hlic_dev_disable_irq(void *state, nk_hwirq_t hwirq) 
{
  struct hlic *hlic = (struct hlic*)state;

  if(CLINT(hlic)) {
    clear_csr(sie, 1ULL<<(uint64_t)hwirq);
    return 0;
  } else {
    return -1;
  }
}

static int hlic_dev_irq_status(void *state, nk_hwirq_t hwirq) 
{
  struct hlic *hlic = (struct hlic*)state;

  int status = 0;

  if(CLINT(hlic)) {
    uint64_t sie_reg = read_csr(sie);
    //printk("sie = %p\n", sie_reg);
    status |= sie_reg & (1ULL<<hwirq) ?
      IRQ_STATUS_ENABLED : 0;
  } else {
    status |= IRQ_STATUS_ENABLED;
  }
  
  uint64_t sip_reg = read_csr(sip);

  status |= sip_reg & (1<<hwirq) ?
    IRQ_STATUS_PENDING : 0;

  return status;
}

static struct nk_irq_dev_int hlic_dev_int = {
  .get_characteristics = hlic_dev_get_characteristics,
  .ack_irq = hlic_dev_ack_irq,
  .eoi_irq = hlic_dev_eoi_irq,
  .enable_irq = hlic_dev_enable_irq,
  .disable_irq = hlic_dev_disable_irq,
  .irq_status = hlic_dev_irq_status,
  .revmap = hlic_dev_revmap,
  .translate = hlic_dev_translate
};

static struct nk_dev *hlic_init_dev_info_dev_ptr;
static int hlic_init_dev_info(struct nk_dev_info *info) 
{
  if(info->type != NK_DEV_INFO_OF) {
    HLIC_ERROR("Currently only support device tree initialization!\n");
    return -1;
  }

  return nk_dev_info_set_device(info, hlic_init_dev_info_dev_ptr);
}

static const char * hlic_properties_names[] = {
  "interrupt-controller"
};

static const struct of_dev_properties hlic_of_properties = {
  .names = hlic_properties_names,
  .count = 1
};

static const struct of_dev_id hlic_of_dev_ids[] = {
  { .properties = &hlic_of_properties, .compatible = "riscv,cpu-intc" },
};

static const struct of_dev_match hlic_of_dev_match = {
  .ids = hlic_of_dev_ids,
  .num_ids = sizeof(hlic_of_dev_ids)/sizeof(struct of_dev_id),
};

static struct nk_dev_int timer_ops = {
  .open = 0,
  .close = 0
};

int hlic_init(void) 
{ 
  int did_alloc = 0;
  int did_register = 0;

  HLIC_DEBUG("init\n");
  struct hlic *hlic = malloc(sizeof(struct hlic));

  if(hlic == NULL) 
  {
    HLIC_ERROR("Failed to allocate HLIC struct!\n");
    goto err_exit;
  }
  did_alloc = 1;

  memset(hlic, 0, sizeof(struct hlic));

  struct nk_irq_dev *dev = nk_irq_dev_register("hlic", 0, &hlic_dev_int, (void*)hlic);

  if(dev == NULL) {
    HLIC_ERROR("Failed to register IRQ device!\n");
    goto err_exit;
  } else {
    did_register = 1;
  }

  if(nk_request_irq_range(HLIC_NUM_IRQ, &hlic->irq_base)) {
    HLIC_ERROR("Failed to get IRQ numbers for interrupts!\n");
    goto err_exit;
  }

  hlic->irq_descs = nk_alloc_irq_descs(HLIC_NUM_IRQ, HLIC_BASE_HWIRQ, NK_IRQ_DESC_FLAG_PERCPU, dev);
  if(hlic->irq_descs == NULL) {
    HLIC_ERROR("Failed to allocate IRQ descriptors!\n");
    goto err_exit;
  }

  if(nk_assign_irq_descs(HLIC_NUM_IRQ, hlic->irq_base, hlic->irq_descs)) {
    HLIC_ERROR("Failed to assign IRQ descriptors for interrupts!\n");
    goto err_exit;
  }

  if(riscv_set_root_irq_dev(dev)) {
    HLIC_ERROR("Failed to set the HLIC as the root IRQ device!\n");
    goto err_exit;
  }

  HLIC_DEBUG("initialized globally!\n");

  hlic_init_dev_info_dev_ptr = (struct nk_dev*)dev;

  if(of_for_each_match(&hlic_of_dev_match, hlic_init_dev_info)) {
    HLIC_ERROR("Failed to assign HLIC to all device tree entries!\n");
    goto err_exit;
  }

  // Set up the timer interrupt
  if(CLINT(hlic)) {
    nk_irq_t timer_irq = NK_NULL_IRQ;
    if(hlic_dev_revmap(hlic, 5, &timer_irq)) {
      HLIC_ERROR("Could not revmap the riscv timer IRQ!\n");
      goto err_exit;
    }
    struct nk_dev *timer_dev = nk_dev_register(
        "riscv-timer",
        NK_DEV_TIMER,
        0,&timer_ops,0);
    if(timer_dev == NULL) {
      HLIC_ERROR("Failed to register timer dev!\n");
      goto err_exit;
    }
    if(nk_irq_add_handler_dev(timer_irq, arch_timer_handler, NULL, (struct nk_dev*)timer_dev)) {
      HLIC_ERROR("Failed to set arch timer handler!\n");
      goto err_exit;
    }
  }

  HLIC_DEBUG("initialized %s\n",
      CLINT(hlic) ? "(in CLINT mode)" : "(in CLIC mode)");

  return 0;

err_exit:
  if(did_alloc) {
    free(hlic);
  }
  if(did_register) {
    nk_irq_dev_unregister(dev);
  }
  return -1;
}


