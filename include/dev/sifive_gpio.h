#ifndef __SIFIVE_GPIO_H__
#define __SIFIVE_GPIO_H__

void sifive_gpio_init(unsigned long fdt);
int sifive_gpio_set_pin(int is_high);

void sifive_gpio_disable_count();
void sifive_gpio_enable_count();

void sifive_gpio_print_ints_received_and_reset();

#endif
