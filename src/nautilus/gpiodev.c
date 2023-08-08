
#include<nautilus/gpiodev.h>
#include<nautilus/spinlock.h>
#include<nautilus/nautilus.h>

#ifdef NAUT_CONFIG_DEBUG_DEV
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) 
#endif

#define ERROR(fmt, args...) ERROR_PRINT("gpiodev: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("gpiodev: " fmt, ##args)
#define WARN(fmt, args...) WARN_PRINT("gpiodev: " fmt, ##args)
#define INFO(fmt, args...) INFO_PRINT("gpiodev: " fmt, ##args)

int nk_gpio_dev_init(void) {
  INFO("init\n");
  return 0;
}
int nk_gpio_dev_deinit(void) {
  INFO("deinit\n");
  return 0;
}

struct nk_gpio_dev * nk_gpio_dev_register(char *name, uint64_t flags, struct nk_gpio_dev_int *inter, void *state)
{
  INFO("register device %s\n", name);

  struct nk_dev *gpio_dev = nk_dev_register(name,NK_DEV_IRQ,flags,(struct nk_dev_int*)inter,state);
  return (struct nk_gpio_dev*)gpio_dev;
}

int nk_gpio_dev_unregister(struct nk_gpio_dev *d) 
{
  INFO("unregister device: %s\n", d->dev.name);
  if(nk_dev_unregister_pre_allocated((struct nk_dev *)d)) {
    return -1;
  }
  free(d);
  return 0;
}

struct nk_gpio_dev * nk_gpio_dev_find(char *name)
{
  DEBUG("find %s\n", name);
  struct nk_dev *d = nk_dev_find(name);

  if(d && d->type == NK_DEV_GPIO) {
      DEBUG("%s found\n", name);
      return (struct nk_gpio_dev*)d;
  } else {
      DEBUG("%s not found\n", name);
      return NULL;
  }
}

/*
 * Local Device Functions
 */

int nk_gpio_dev_get_characteristics(struct nk_gpio_dev *dev, struct nk_gpio_dev_characteristics *c) 
{
  struct nk_dev *d = (struct nk_dev *)(&(dev->dev));
  struct nk_gpio_dev_int *di = (struct nk_gpio_dev_int*)(d->interface);

#ifdef NAUT_CONFIG_ENABLE_ASSERTS
  if(di->get_characteristics) {
    return di->get_characteristics(d->state, c);
  } else {
    ERROR("NULL get_characteristics in interface of device %s\n", d->name);
    return -1;
  }
#else
  return di->get_characteristics(d->state, c);
#endif
}

#ifdef NAUT_CONFIG_ENABLE_ASSERTS
#define declare_gpio_dev_method_noargs(func_name, method) \
  int func_name (struct nk_gpio_dev *dev, nk_gpio_t num) { \
    struct nk_dev *d = (struct nk_dev *)(&(dev->dev)); \
    struct nk_gpio_dev_int *di = (struct nk_gpio_dev_int *)(d->interface); \
    if(di->method) { \
      return di->method(d->state, num); \
    } else { \
      return -1; \
    } \
  }

#define declare_gpio_dev_method_int(func_name, method) \
  int func_name (struct nk_gpio_dev *dev, nk_gpio_t num, int val) { \
    struct nk_dev *d = (struct nk_dev *)(&(dev->dev)); \
    struct nk_gpio_dev_int *di = (struct nk_gpio_dev_int *)(d->interface); \
    if(di->method) { \
      return di->method(d->state, num, val); \
    } else { \
      return -1; \
    } \
  }

#define declare_gpio_dev_method_intptr(func_name, method) \
  int func_name (struct nk_gpio_dev *dev, nk_gpio_t num, int *ptr) { \
    struct nk_dev *d = (struct nk_dev *)(&(dev->dev)); \
    struct nk_gpio_dev_int *di = (struct nk_gpio_dev_int *)(d->interface); \
    if(di->method) { \
      return di->method(d->state, num, ptr); \
    } else { \
      return -1; \
    } \
  }
#else
#define declare_gpio_dev_method_noargs(func_name, method) \
  int func_name (struct nk_gpio_dev *dev, nk_gpio_t num) { \
    struct nk_dev *d = (struct nk_dev *)(&(dev->dev)); \
    struct nk_gpio_dev_int *di = (struct nk_gpio_dev_int *)(d->interface); \
    return di->method(d->state, num); \
  }

#define declare_gpio_dev_method_int(func_name, method) \
  int func_name (struct nk_gpio_dev *dev, nk_gpio_t num, int val) { \
    struct nk_dev *d = (struct nk_dev *)(&(dev->dev)); \
    struct nk_gpio_dev_int *di = (struct nk_gpio_dev_int *)(d->interface); \
    return di->method(d->state, num, val); \
  }

#define declare_gpio_dev_method_intptr(func_name, method) \
  int func_name (struct nk_gpio_dev *dev, nk_gpio_t num, int *ptr) { \
    struct nk_dev *d = (struct nk_dev *)(&(dev->dev)); \
    struct nk_gpio_dev_int *di = (struct nk_gpio_dev_int *)(d->interface); \
    return di->method(d->state, num, ptr); \
  }
#endif

declare_gpio_dev_method_int(nk_gpio_dev_write_output, write_output);
declare_gpio_dev_method_intptr(nk_gpio_dev_read_input, read_input);

declare_gpio_dev_method_int(nk_gpio_dev_set_io_type, set_io_type);
declare_gpio_dev_method_intptr(nk_gpio_dev_get_io_type, get_io_type);

declare_gpio_dev_method_int(nk_gpio_dev_set_polarity, set_polarity);
declare_gpio_dev_method_intptr(nk_gpio_dev_get_polarity, get_polarity);

declare_gpio_dev_method_int(nk_gpio_dev_set_open_type, set_open_type);
declare_gpio_dev_method_intptr(nk_gpio_dev_get_open_type, get_open_type);

declare_gpio_dev_method_int(nk_gpio_dev_set_pull_type, set_pull_type);
declare_gpio_dev_method_intptr(nk_gpio_dev_get_pull_type, get_pull_type);

