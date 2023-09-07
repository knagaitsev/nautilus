
static int hlic_dev_get_characteristics(void *state, struct nk_irq_dev_characteristics *c)
{
  memset(c, 0, sizeof(c));
  return 0;
}

static int hlic_dev_initialize_cpu(void *state) 
{
  struct hlic *hlic = (struct hlic*)hlic;
  MREG(HLIC_THRESHOLD(my_cpu_id())) = 0;
  return 0;
}

static int hlic_dev_translate_irqs(void *state, nk_dev_info_type_t type, void *raw_irqs, int raw_irqs_len, nk_irq_t *buffer, int *buf_count) 
{
  struct hlic *hlic = (struct hlic*)state;
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
    HLIC_ERROR("Cannot translate nk_dev_info IRQ's of type other than OF!\n");
    return -1;
  }
  return 0;
}

static int hlic_dev_ack_irq(void *state, nk_irq_t *irq) 
{
  struct hlic *hlic = (struct hlic*)state;
  // TODO
  return 0;
}

static int hlic_dev_eoi_irq(void *state, nk_irq_t irq) 
{
  struct hlic *hlic = (struct hlic*)state;
  // TODO
  return 0;
}

static int hlic_dev_enable_irq(void *state, nk_irq_t irq) 
{
  struct hlic *hlic = (struct hlic*)state;
  // TODO
  return 0;
}

static int hlic_dev_disable_irq(void *state, nk_irq_t irq) 
{
  struct hlic *hlic = (struct hlic*)state;
  // TODO
  return 0;
}

static int hlic_dev_irq_status(void *state, nk_irq_t irq) 
{
  struct hlic *hlic = (struct hlic*)state;

  int status = 0;

  status |= 0/*TODO*/ ?
    IRQ_STATUS_ENABLED : 0;

  status |= 0/*TODO*/ ?
    IRQ_STATUS_PENDING : 0;

  status |= 0/*TODO*/ ?
    IRQ_STATUS_ACTIVE : 0;

  return status;
}

static struct nk_irq_dev_int hlic_dev_int = {
  .get_characteristics = hlic_dev_get_characteristics,
  .initialize_cpu = hlic_dev_initialize_cpu, 
  .ack_irq = hlic_dev_ack_irq,
  .eoi_irq = hlic_dev_eoi_irq,
  .enable_irq = hlic_dev_enable_irq,
  .disable_irq = hlic_dev_disable_irq,
  .irq_status = hlic_dev_irq_status,
  .translate_irqs = hlic_dev_translate_irqs
};

static int hlic_init_dev_info(struct nk_dev_info *info) 
{
  int did_alloc = 0;
  int did_register = 0;

  if(info->type != NK_DEV_INFO_OF) {
    HLIC_ERROR("Currently only support device tree initialization!\n");
    goto err_exit;
  }

  struct hlic *hlic = malloc(sizeof(struct hlic));

  if(hlic == NULL) 
  {
    HLIC_ERROR("Failed to allocate HLIC struct!\n");
    goto err_exit;
  }
  did_alloc = 1;

  memset(hlic, 0, sizeof(struct hlic));

  if(nk_dev_info_read_register_block(info, &hlic->mmio_base, &hlic->mmio_size)) {
    HLIC_ERROR("Failed to read register block!\n");
    goto err_exit;
  }

  if(nk_dev_info_read_u32(info, "riscv,ndev", &hlic->num_irqs)) {
    HLIC_ERROR("Failed to read number of IRQ's!\n");
    goto err_exit;
  }

  HLIC_DEBUG("mmio_base = %p, mmio_size = %p, num_irqs = %u\n",
      hlic->mmio_base, hlic->mmio_size, hlic->num_irqs);

  struct nk_irq_dev *dev = nk_irq_dev_register("hlic", 0, &hlic_dev_int, (void*)hlic);

  if(dev == NULL) {
    HLIC_ERROR("Failed to register IRQ device!\n");
    goto err_exit;
  } else {
    did_register = 1;
  }

  nk_dev_info_set_device(info, (struct nk_dev*)dev);

  nk_alloc_ivec_descs(0, hlic->num_irqs, dev, NK_IVEC_DESC_TYPE_DEFAULT, 0);
  HLIC_DEBUG("Allocated Vectors!\n");

  if(nk_assign_all_cpus_irq_dev(dev)) {
    HLIC_ERROR("Failed to assign the HLIC to all CPU's in the system!\n");
    goto err_exit;
  }

  HLIC_DEBUG("initialized globally!\n");

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
  .max_num_matches = 1
};

int hlic_init(void) 
{ 
  HLIC_DEBUG("init\n");
  return of_for_each_match(&hlic_of_dev_match, hlic_init_dev_info);
  HLIC_DEBUG("initialized\n");
}


