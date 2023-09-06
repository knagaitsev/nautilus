
#include<nautilus/irqdev.h>

#ifdef NAUT_CONFIG_DEBUG_DEV
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) 
#endif

#define ERROR(fmt, args...) ERROR_PRINT("irqdev: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("irqdev: " fmt, ##args)
#define INFO(fmt, args...) INFO_PRINT("irqdev: " fmt, ##args)

int nk_irq_dev_init(void) {
  INFO("init\n");
  return 0;
}
int nk_irq_dev_deinit(void) {
  INFO("deinit\n");
  return 0;
}

struct nk_irq_dev * nk_irq_dev_register(char *name, uint64_t flags, struct nk_irq_dev_int *interface, void *state) 
{
  INFO("register device %s\n", name);
  struct nk_irq_dev *irq_dev = (struct nk_irq_dev *)nk_dev_register(name,NK_DEV_IRQ,flags,(struct nk_dev_int *)interface,state);

  return irq_dev;
}

int nk_irq_dev_unregister(struct nk_irq_dev *d) 
{
  INFO("unregister device: %s\n", d->dev.name);
  return nk_dev_unregister((struct nk_dev*)d);
}

struct nk_irq_dev * nk_irq_dev_find(char *name) 
{

  DEBUG("find %s\n", name);
  struct nk_dev *d = nk_dev_find(name);

  if(d && d->type == NK_DEV_IRQ) {
      DEBUG("%s found\n", name);
      return (struct nk_irq_dev*)d;
  } else {
      DEBUG("%s not found\n", name);
      return NULL;
  }
}

int nk_irq_dev_initialize_cpu(struct nk_irq_dev *dev) {
 
  struct nk_dev *d = (struct nk_dev *)(&(dev->dev));
  struct nk_irq_dev_int *di = (struct nk_irq_dev_int *)(d->interface);

  if(di->initialize_cpu) {
    return di->initialize_cpu(d->state);
  } else {
    // This device doesn't need to initialize CPU's 
    return 0;
  }
}

int nk_irq_dev_get_characteristics(struct nk_irq_dev *dev, struct nk_irq_dev_characteristics *c) {

  struct nk_dev *d = (struct nk_dev*)(&(dev->dev));
  struct nk_irq_dev_int *di = (struct nk_irq_dev_int *)(d->interface);

  if(di->get_characteristics) {
    return di->get_characteristics(d->state, c);
  } else {
    memset(c, 0, sizeof(struct nk_irq_dev_characteristics));
    return 0;
  }
}

int nk_irq_dev_ack(struct nk_irq_dev *dev, nk_irq_t *hwirq) {
 
  struct nk_dev *d = (struct nk_dev *)(&(dev->dev));
  struct nk_irq_dev_int *di = (struct nk_irq_dev_int *)(d->interface);

  // We want this to be as fast as possible
#ifdef NAUT_CONFIG_ENABLE_ASSERTS
  if(di->ack_irq) {
    return di->ack_irq(d->state, hwirq);
  } else {
    // This device doesn't have ACK 
    ERROR("NULL ack_irq in interface of device %s\n", d->name);
    return IRQ_DEV_ACK_UNIMPL;
  }
#else
  return di->ack_irq(d->state, hwirq);
#endif
}

int nk_irq_dev_eoi(struct nk_irq_dev *dev, nk_hwirq_t hwirq) {

  struct nk_dev *d = (struct nk_dev *)(&(dev->dev));
  struct nk_irq_dev_int *di = (struct nk_irq_dev_int *)(d->interface);

#ifdef NAUT_CONFIG_ENABLE_ASSERTS
  if(di->eoi_irq) {
    return di->eoi_irq(d->state, hwirq);
  } else {
    // This device doesn't have EOI!
    return IRQ_DEV_EOI_UNIMPL;
  }
#else
  return di->eoi_irq(d->state, hwirq);
#endif
}

int nk_irq_dev_enable_irq(struct nk_irq_dev *dev, nk_hwirq_t irq) {
 
  struct nk_dev *d = (struct nk_dev *)(&(dev->dev));
  struct nk_irq_dev_int *di = (struct nk_irq_dev_int *)(d->interface);

  DEBUG_PRINT("nk_irq_dev_enable_irq(%s, %u)\n", d->name, irq);

#ifdef NAUT_CONFIG_ENABLE_ASSERTS
  if(di->enable_irq) {
    return di->enable_irq(d->state, irq);
  } else {
    // This device doesn't have enable!
    ERROR("NULL enable_irq in interface of device %s\n", d->name);
    return 1;
  }
#else
  return di->enable_irq(d->state, hwirq);
#endif 
}

int nk_irq_dev_disable_irq(struct nk_irq_dev *dev, nk_hwirq_t hwirq) {
 
  struct nk_dev *d = (struct nk_dev *)(&(dev->dev));
  struct nk_irq_dev_int *di = (struct nk_irq_dev_int *)(d->interface);

#ifdef NAUT_CONFIG_ENABLE_ASSERTS
  if(di->disable_irq) {
    return di->disable_irq(d->state, hwirq);
  } else {
    // This device doesn't have disable!
    ERROR("NULL disable_irq in interface of device %s\n", d->name);
    return 1;
  }
#else
  return di->disable_irq(d->state, hwirq);
#endif 
}

int nk_irq_dev_irq_status(struct nk_irq_dev *dev, nk_hwirq_t hwirq) {
 
  struct nk_dev *d = (struct nk_dev *)(&(dev->dev));
  struct nk_irq_dev_int *di = (struct nk_irq_dev_int *)(d->interface);

#ifdef NAUT_CONFIG_ENABLE_ASSERTS
  if(di->irq_status) {
    return di->irq_status(d->state, hwirq);
  } else {
    ERROR("NULL irq_status in interface of device %s\n", d->name);
    return IRQ_DEV_STATUS_ERROR;
  }
#else
  return di->irq_status(d->state, hwirq);
#endif 
}

int nk_irq_dev_translate_irqs(struct nk_irq_dev *dev, nk_dev_info_type_t type, void *raw_irqs, int raw_irqs_len, nk_irq_t *irq_buf, int *irq_buf_count) 
{ 
  struct nk_dev *d = (struct nk_dev *)(&(dev->dev));
  struct nk_irq_dev_int *di = (struct nk_irq_dev_int *)(d->interface);

#ifdef NAUT_CONFIG_ENABLE_ASSERTS
  if(di->translate_irqs) {
    return di->translate_irqs(d->state, type, raw_irqs, raw_irqs_len, irq_buf, irq_buf_count);
  } else {
    // This device doesn't have translation support!
    ERROR("NULL translate_irq in interface of device %s\n", d->name);
    return -1;
  }
#else
  return di->translate_irqs(d->state, type, raw_irqs, raw_irqs_len, irq_buf, irq_buf_count);
#endif
}

int nk_irq_dev_revmap(struct nk_irq_dev *dev, nk_hwirq_t hwirq, nk_irq_t *out_irq) 
{
  struct nk_dev *d = (struct nk_dev *)(&(dev->dev));
  struct nk_irq_dev_int *di = (struct nk_irq_dev_int *)(d->interface);

#ifdef NAUT_CONFIG_ENABLE_ASSERTS
  if(di->revmap) {
    return di->revmap(d->state, hwirq, out_irq);
  } else {
    // This device doesn't have translation support!
    ERROR("NULL revmap in interface of device %s\n", d->name);
    return -1;
  }
#else
  return di->revmap(d->state, hwirq, out_irq);
#endif 
}

int nk_assign_cpu_irq_dev(struct nk_irq_dev* dev, cpu_id_t cpuid) {
  
  struct naut_info *naut = nk_get_nautilus_info();

  if(cpuid >= naut->sys.num_cpus) {
    return -1;
  }

  naut->sys.cpus[cpuid]->irq_dev = dev;

  return 0;
}

int nk_assign_all_cpus_irq_dev(struct nk_irq_dev* dev) {
  int ret = 0;

  struct naut_info *naut = nk_get_nautilus_info();

  for(int i = 0; i < naut->sys.num_cpus; i++) {
    ret |= nk_assign_cpu_irq_dev(dev, i);
  }
  
  return ret;
}

