
#include<dev/8250/core.h>
#include<dev/8250/of_8250.h>
#include<nautilus/dev.h>
#include<nautilus/endian.h>
#include<nautilus/of/fdt.h>
#include<nautilus/of/dt.h>
#include<nautilus/iomap.h>

#ifndef NAUT_CONFIG_DEBUG_OF_8250_UART
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

#define INFO(fmt, args...) INFO_PRINT("[8250] " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("[8250] " fmt, ##args)
#define ERROR(fmt, args...) ERROR_PRINT("[8250] " fmt, ##args)
#define WARN(fmt, args...) WARN_PRINT("[8250] " fmt, ##args)

#ifdef NAUT_CONFIG_OF_8250_UART_EARLY_OUTPUT
static int of_8250_fdt_init(uint64_t dtb, uint64_t offset, struct uart_8250_port *port) {

    fdt_reg_t reg = { .address = 0, .size = 0 };

    if(fdt_getreg((void*)dtb, offset, &reg)) {
      return -1;
    }

    port->reg_base = nk_io_map(reg.address, reg.size, 0);
    if((void*)port->reg_base == NULL) {
      return -1;
    }

    // Set default ops
    port->ops = uart_8250_default_uart_8250_ops;
    uart_8250_struct_init(port);
    
    int lenp;
    const uint32_t *reg_shift_ptr = fdt_getprop((void*)dtb, offset, "reg-shift", &lenp);
    port->reg_shift = 0;
    if(reg_shift_ptr != NULL) {
      port->reg_shift = be32toh(*reg_shift_ptr);
    }
    
    const uint32_t *reg_width_ptr = fdt_getprop((void*)dtb, offset, "reg-io-width", &lenp);
    if(reg_width_ptr != NULL) {
      uint32_t reg_width = be32toh(*reg_width_ptr);
      switch(reg_width) {
        case 1:
          port->ops.write_reg = generic_8250_write_reg_mem8;
          break;
        case 4:
          port->ops.write_reg = generic_8250_write_reg_mem32;
          break;
        default:
          ERROR("Could not assign uart_8250_ops.write_reg for UART, using default!\n");
          break;
      }
    }

    return 0;
}

static struct uart_8250_port pre_vc_of_8250;
static int pre_vc_of_8250_dtb_offset = -1;

static void of_8250_early_putchar(char c) {
  generic_8250_direct_putchar(&pre_vc_of_8250, c);
}

#define OF_8250_PRE_VC_COMPAT "ns16550a"

int of_8250_pre_vc_init(uint64_t dtb) 
{
  memset(&pre_vc_of_8250, 0, sizeof(struct uart_8250_port));

  int offset = fdt_node_offset_by_compatible((void*)dtb, -1, OF_8250_PRE_VC_COMPAT);

  if(offset < 0) {
    return -1;
  }

  pre_vc_of_8250_dtb_offset = offset;

  if(of_8250_fdt_init(dtb, offset, &pre_vc_of_8250)) {
    return -1;
  }

  generic_8250_disable_fifos(&pre_vc_of_8250);
  generic_8250_clear_fifos(&pre_vc_of_8250);

  uart_port_set_baud_rate(&pre_vc_of_8250.port, 115200);
  uart_port_set_word_length(&pre_vc_of_8250.port, 8);
  uart_port_set_parity(&pre_vc_of_8250.port, UART_PARITY_NONE);
  uart_port_set_stop_bits(&pre_vc_of_8250.port, 2);

  if(nk_pre_vc_register(of_8250_early_putchar, NULL)) {
    return -1;
  }

  return 0;
}
#endif

static int of_8250_dev_init_one(struct nk_dev_info *info)
{
  // For each compatible
  int did_alloc = 0;
  int did_map = 0;
  int did_register = 0;

  struct uart_8250_port *port = NULL;
    
#ifdef NAUT_CONFIG_OF_8250_UART_EARLY_OUTPUT
  if(info->type == NK_DEV_INFO_OF && ((struct dt_node*)info->state)->dtb_offset == pre_vc_of_8250_dtb_offset) {
     DEBUG("Reinitializing early inited UART\n");
     port = &pre_vc_of_8250;
  } else {
     DEBUG("Initializing new UART\n");
     port = malloc(sizeof(struct uart_8250_port));
     memset(port, 0, sizeof(struct uart_8250_port));
     if(port == NULL) {
       goto err_exit;
     }
     did_alloc = 1;
  }
#else
  DEBUG("Initializng UART\n");
  port = malloc(sizeof(struct uart_8250_port));

  if(port == NULL) {
    ERROR("Failed to allocate UART!\n");
    goto err_exit;
  }

  did_alloc = 1; 
  memset(port, 0, sizeof(struct uart_8250_port));
#endif

  // Set default ops
#ifdef NAUT_CONFIG_OF_8250_UART_EARLY_OUTPUT
  if(did_alloc) {
    port->ops = uart_8250_default_uart_8250_ops;
    uart_8250_struct_init(port);
  }
#else
  port->ops = uart_8250_default_uart_8250_ops;
  uart_8250_struct_init(port);
#endif

  uint64_t reg_base;
  int reg_size;
  if(nk_dev_info_read_register_block(info, &reg_base, &reg_size)) {
    ERROR("Failed to read register block for UART!\n");
    goto err_exit;
  }

#ifdef NAUT_CONFIG_OF_8250_UART_EARLY_OUTPUT
  if(did_alloc == 0) {
    DEBUG("Skipping mapping I/O registers for early inited UART\n");
  } else {
#endif
    DEBUG("Mapping I/O registers for UART\n");
    port->reg_base = nk_io_map(reg_base, reg_size, 0);
    if((void*)port->reg_base == NULL) {
      ERROR("Failed to map UART registers!\n");
      goto err_exit;
    } else {
      DEBUG("Mapped I/O registers for UART\n");
    }
    did_map = 1;
#ifdef NAUT_CONFIG_OF_8250_UART_EARLY_OUTPUT
  }
#endif

  uint32_t reg_width;
  if(!nk_dev_info_read_u32(info, "reg-io-width", &reg_width)) {
    switch(reg_width) {
      case 1:
        port->ops.write_reg = generic_8250_write_reg_mem8;
        break;
      case 4:
        port->ops.write_reg = generic_8250_write_reg_mem32;
        break;
      default:
        ERROR("Could not assign uart_8250_ops.write_reg for UART, using default!\n");
        break;
    }
  }

  if(nk_dev_info_read_u32(info, "reg-shift", &port->reg_shift)) {
    port->reg_shift = 0;
  }

  port->irq = nk_dev_info_read_irq(info, 0);

  if(port->irq == NK_NULL_IRQ) {
    ERROR("Failed to read UART irq!\n");
    goto err_exit;
  }

  char name_buf[DEV_NAME_LEN];
  snprintf(name_buf,DEV_NAME_LEN,"serial%u",nk_dev_get_serial_device_number());

  struct nk_char_dev *dev = nk_char_dev_register(name_buf,NK_DEV_FLAG_NO_WAIT,&generic_8250_char_dev_int,(void*)port);
  port->dev = dev;

  if(dev == NULL) {
    ERROR("Failed to register UART as a character device!\n");
    goto err_exit;
  }
  did_register = 1;

  // Set up UART
  generic_8250_disable_fifos(port);
  generic_8250_clear_fifos(port);

  uart_port_set_baud_rate(&port->port, 115200);
  uart_port_set_word_length(&port->port, 8);
  uart_port_set_parity(&port->port, UART_PARITY_ODD);
  uart_port_set_stop_bits(&port->port, 2);

  if(nk_irq_add_handler_dev(port->irq, uart_8250_interrupt_handler, (void*)port, (struct nk_dev *)dev)) {
    ERROR("Failed to add IRQ handler for UART!\n");
  }

  generic_8250_enable_fifos(port);
  generic_8250_enable_recv_interrupts(port);

  nk_dev_info_set_device(info, (struct nk_dev*)dev);

  nk_unmask_irq(port->irq);

  return 0;

err_exit:
  if(did_alloc) {
    free(port);
  }
  if(did_map) {
    nk_io_unmap(port->reg_base);
  }
  if(did_register) {
    nk_char_dev_unregister(dev);
  }
  return -1;
}


// KJH - Undoubtedly most of these "matches" will have quirks
// but I'm including all of them for now, because Linux groups them
// together under one driver
static struct of_dev_id of_8250_of_dev_ids[] = {
  {.compatible = "ns8250"},
  {.compatible = "ns16450"},
  {.compatible = "ns16550a"},
  {.compatible = "ns16550"},
  {.compatible = "ns16750"},
  {.compatible = "ns16850"},
  {.compatible = "nvidia,tegra20-uart"},
  {.compatible = "nxp,lpc3220-uart"},
  {.compatible = "ibm,qpace-nwp-serial"},
  {.compatible = "altr,16550-FIFO32"},
  {.compatible = "altr,16550-FIFO64"},
  {.compatible = "altr,16550-FIFO128"},
  {.compatible = "serial"}
};
declare_of_dev_match_id_array(of_8250_of_match, of_8250_of_dev_ids);

int of_8250_init(void)
{
  of_for_each_match(&of_8250_of_match, of_8250_dev_init_one);
  return 0;
}

