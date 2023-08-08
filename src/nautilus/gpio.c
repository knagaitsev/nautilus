
#include<nautilus/nautilus.h>
#include<nautilus/naut_types.h>
#include<nautilus/gpio.h>
#include<nautilus/spinlock.h>
#include<nautilus/radix_tree.h>

static spinlock_t gpio_lock;
static struct nk_radix_tree gpio_radix_tree;
static nk_gpio_t next_free_gpio;

int nk_gpio_init(void)
{
  spinlock_init(&gpio_lock);
  return 0;
}
int nk_gpio_deinit(void)
{
  spinlock_deinit(&gpio_lock);
  return 0;
}

int nk_gpio_alloc_block(uint32_t num, struct nk_gpio_dev *dev, nk_gpio_t *base)
{
  int flags = spin_lock_irq_save(&gpio_lock);

  int num_inserted = 0;

  *base = next_free_gpio;

  for(uint32_t i = 0; i<num; i++) 
  {
    struct nk_gpio_desc *desc = malloc(sizeof(struct nk_gpio_desc));
    if(desc == NULL) {
      goto err_exit;
    }
    memset(desc, 0, sizeof(struct nk_gpio_desc));

    desc->num = next_free_gpio+i;
    desc->dev = dev;

    if(nk_radix_tree_insert(&gpio_radix_tree, desc->num, desc)) {
      free(desc);
      goto err_exit;
    }
    num_inserted++;
  }

  next_free_gpio += num;

  spin_unlock_irq_restore(&gpio_lock, flags);
  return 0;

err_exit:

  for(int i = 0; i < num_inserted; i++) 
  {
    struct nk_gpio_desc *desc = nk_radix_tree_remove(next_free_gpio + i);
    free(desc);
  }

  spin_unlock_irq_restore(&gpio_lock, flags);
  return -1;
}

struct nk_gpio_desc *nk_gpio_get_desc(nk_gpio_t num) 
{
  return nk_radix_tree_get(&gpio_radix_tree, (unsigned long)num);
}

int nk_gpio_get_flags(nk_gpio_t num, int *flags)
{
  struct nk_gpio_desc *desc = nk_gpio_get_desc(num);
  if(desc->dev == NULL) {
    // If there's no device, just return what we have
    *flags = desc->flags;
  } else {

  }
  return 0;
}
int nk_gpio_set_flags(nk_gpio_t num, int flags) 
{
  struct nk_gpio_desc *desc = nk_gpio_get_desc(num);
  if(desc->dev == NULL) {
    // If there's no device just overwrite the flags
    desc->flags = flags;
  } else {

  }
  return 0;
}
int nk_gpio_set_output(nk_gpio_t num, int val)
{
  struct nk_gpio_desc *desc = nk_gpio_get_desc(num);
  if(desc->dev == NULL) {
    // No device, just overwrite type
    desc->type = NK_GPIO_OUTPUT;
  } else {

  }
  return 0;
}
int nk_gpio_set_input(nk_gpio_t num) 
{
  struct nk_gpio_desc *desc = nk_gpio_get_desc(num);
  if(desc->dev == NULL) {
    // No device, just overwrite type
    desc->type = NK_GPIO_INPUT;
  } else {

  }
  return 0;
}
int nk_gpio_write_output(nk_gpio_t num, int val)
{
  struct nk_gpio_desc *desc = nk_gpio_get_desc(num);
  if(desc->dev == NULL) {
    if(desc->flags & NK_GPIO_ACTIVE_HIGH) {
      desc->raw_state = val == NK_GPIO_HIGH ? NK_GPIO_ACTIVE : NK_GPIO_INACTIVE;
    } else if(desc->flags & NK_GPIO_ACTIVE_LOW) {
      desc->raw_state = val == NK_GPIO_LOW ? NK_GPIO_ACTIVE : NK_GPIO_INACTIVE;
    } else {
      return -1;
    }
  } else {

  }
  return 0;
}
int nk_gpio_read_input(nk_gpio_t num, int *val)
{
  struct nk_gpio_desc *desc = nk_gpio_get_desc(num);
  if(desc->dev == NULL) {
    if(desc->flags & NK_GPIO_ACTIVE_HIGH) {
      *val = desc->raw_state == NK_GPIO_HIGH ? NK_GPIO_ACTIVE : NK_GPIO_INACTIVE;
    } else if(desc->flags & NK_GPIO_ACTIVE_LOW) {
      *val = desc->raw_state == NK_GPIO_LOW ? NK_GPIO_ACTIVE : NK_GPIO_INACTIVE;
    } else {
      return -1;
    }
  } else {

  }
  return 0;
}
int nk_gpio_write_output_raw(nk_gpio_t num, int val)
{
  struct nk_gpio_desc *desc = nk_gpio_get_desc(num);
  if(desc->dev == NULL) {
    return desc->raw_state;
  } else {

  }
  return 0;
}
int nk_gpio_read_input_raw(nk_gpio_t num, int *val)
{
  struct nk_gpio_desc *desc = nk_gpio_get_desc(num);
  if(desc->dev == NULL) {
    return desc->raw_state;
  } else {

  }
  return 0;
}
