
#include<dev/8250/core.h>
#include<dev/8250/dw_8250.h>
#include<nautilus/endian.h>
#include<nautilus/iomap.h>
#include<nautilus/dev.h>
#include<nautilus/of/fdt.h>
#include<nautilus/of/dt.h>
#include<nautilus/interrupt.h>

// Driver for the DesignWare 8250 UART

#define DW_8250_USR 0x1f

#define DW_8250_IIR_BUSY 0x7

#ifndef NAUT_CONFIG_DEBUG_DW_8250_UART
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

#define INFO(fmt, args...) INFO_PRINT("[DW_8250] " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("[DW_8250] " fmt, ##args)
#define ERROR(fmt, args...) ERROR_PRINT("[DW_8250] " fmt, ##args)
#define WARN(fmt, args...) WARN_PRINT("[DW_8250] " fmt, ##args)


struct dw_8250 
{
  struct uart_8250_port port;
  unsigned int last_lcr;
};

static int dw_8250_handle_irq(struct uart_8250_port *uart, unsigned int iir)
{
  unsigned int iir_reason = iir & 0xF;

  if(iir_reason == UART_8250_IIR_RECV_TIMEOUT) {
    if(uart_8250_recv_empty(uart)) {
      // Timeout with empty recv buffer,
      // This is a possible error state for the DW UART,
      // do a read of RBR to fix it
      uint8_t read_val = uart_8250_read_reg(uart,UART_8250_RBR);
      generic_8250_direct_putchar(uart, read_val);
    }
  }
  else if(iir_reason == DW_8250_IIR_BUSY) {
    // Design ware specific interrupt
    // Rewrite LCR and read the USR reg to make it go away 
    uart_8250_write_reg(uart,UART_8250_LCR,((struct dw_8250*)uart)->last_lcr);
    (void)uart_8250_read_reg(uart,DW_8250_USR);
    return 0;
  } else {
    return generic_8250_handle_irq(uart,iir);
  }
}

static void dw_8250_write_reg8(struct uart_8250_port *uart, int offset, unsigned int val) 
{
  if(offset == UART_8250_LCR) {
    ((struct dw_8250*)uart)->last_lcr = val;
  } 
  generic_8250_write_reg_mem8(uart, offset, val);
}
static void dw_8250_write_reg32(struct uart_8250_port *uart, int offset, unsigned int val) 
{
  if(offset == UART_8250_LCR) {
    ((struct dw_8250*)uart)->last_lcr = val;
  }
  generic_8250_write_reg_mem32(uart, offset, val);
}

const static struct uart_8250_ops dw_8250_ops = {
  .handle_irq = dw_8250_handle_irq,
  .read_reg = generic_8250_read_reg_mem8,
  .write_reg = dw_8250_write_reg8
};

#ifdef NAUT_CONFIG_DW_8250_UART_EARLY_OUTPUT
static int dw_8250_fdt_init(uint64_t dtb, uint64_t offset, struct dw_8250 *dw) {

    fdt_reg_t reg = { .address = 0, .size = 0 };

    if(fdt_getreg((void*)dtb, offset, &reg)) {
      return -1;
    }

    dw->port.reg_base = nk_io_map(reg.address, reg.size, 0);
    if(dw->port.reg_base == NULL) {
      return -1;
    }

    // Set default ops
    dw->port.ops = dw_8250_ops;
    uart_8250_struct_init(&dw->port);
    
    int lenp;
    const uint32_t *reg_shift_ptr = fdt_getprop((void*)dtb, offset, "reg-shift", &lenp);
    dw->port.reg_shift = 0;
    if(reg_shift_ptr != NULL) {
      dw->port.reg_shift = be32toh(*reg_shift_ptr);
    }
    
    const uint32_t *reg_width_ptr = fdt_getprop((void*)dtb, offset, "reg-io-width", &lenp);
    if(reg_width_ptr != NULL) {
      uint32_t reg_width = be32toh(*reg_width_ptr);
      switch(reg_width) {
        case 1:
          dw->port.ops.read_reg = generic_8250_read_reg_mem8;
          dw->port.ops.write_reg = dw_8250_write_reg8;
          break;
        case 4:
          dw->port.ops.read_reg = generic_8250_read_reg_mem32;
          dw->port.ops.write_reg = dw_8250_write_reg32;
          break;
        default:
          ERROR("Could not assign uart_8250_ops.write_reg for DW UART, using default!\n");
          break;
      }
    }

    return 0;
}

static struct dw_8250 pre_vc_dw_8250;
static int pre_vc_dw_8250_dtb_offset = -1;

static void dw_8250_early_putchar(char c) {
  generic_8250_direct_putchar(&pre_vc_dw_8250.port, c);
}

int dw_8250_pre_vc_init(uint64_t dtb) 
{
  memset(&pre_vc_dw_8250, 0, sizeof(struct dw_8250));

  // KJH - HACK for getting to rockpro64 uart2
  int uart_num = 2;
  // UART 0
  int offset = fdt_node_offset_by_compatible((void*)dtb, -1, "snps,dw-apb-uart");
  for(int i = 0; i < uart_num; i++) {
    offset = fdt_node_offset_by_compatible((void*)dtb, offset, "snps,dw-apb-uart");
  }

  if(offset < 0) {
    return -1;
  }

  pre_vc_dw_8250_dtb_offset = offset;

  if(dw_8250_fdt_init(dtb, offset, &pre_vc_dw_8250)) {
    return -1;
  }

  generic_8250_disable_fifos(&pre_vc_dw_8250.port);
  generic_8250_clear_fifos(&pre_vc_dw_8250.port);

  uart_port_set_baud_rate(&pre_vc_dw_8250.port.port, 115200);
  uart_port_set_word_length(&pre_vc_dw_8250.port.port, 8);
  uart_port_set_parity(&pre_vc_dw_8250.port.port, UART_PARITY_NONE);
  uart_port_set_stop_bits(&pre_vc_dw_8250.port.port, 2);

  if(nk_pre_vc_register(dw_8250_early_putchar, NULL)) {
    return -1;
  }

  return 0;
}
#endif

static int dw_8250_dev_init_one(struct nk_dev_info *info)
{
  // For each compatible
  int did_alloc = 0;
  int did_map = 0;
  int did_register = 0;

  struct dw_8250 *dw = NULL;
    
#ifdef NAUT_CONFIG_DW_8250_UART_EARLY_OUTPUT
  if(info->type == NK_DEV_INFO_OF && ((struct dt_node*)info->state)->dtb_offset == pre_vc_dw_8250_dtb_offset) {
     DEBUG("Reinitializing early inited DW 8250 UART\n");
     dw = &pre_vc_dw_8250;
  } else {
     DEBUG("Initializing new DW 8250 UART\n");
     dw = malloc(sizeof(struct dw_8250));
     memset(dw, 0, sizeof(struct dw_8250));
     if(dw == NULL) {
       goto err_exit;
     }
     did_alloc = 1;
  }
#else
  DEBUG("Initializng DW 8250 UART\n");
  dw = malloc(sizeof(struct dw_8250));

  if(dw == NULL) {
    ERROR("Failed to allocate DW UART!\n");
    goto err_exit;
  }

  did_alloc = 1; 
  memset(dw, 0, sizeof(struct dw_8250));
#endif

  // Set default ops
#ifdef NAUT_CONFIG_DW_8250_UART_EARLY_OUTPUT
  if(did_alloc) {
    dw->port.ops = dw_8250_ops;
    uart_8250_struct_init(&dw->port);
  }
#else
  dw->port.ops = dw_8250_ops;
  uart_8250_struct_init(&dw->port);
#endif

  uint64_t reg_base;
  int reg_size;
  if(nk_dev_info_read_register_block(info, &reg_base, &reg_size)) {
    ERROR("Failed to read register block for DW UART!\n");
    goto err_exit;
  }

#ifdef NAUT_CONFIG_DW_8250_UART_EARLY_OUTPUT
  if(did_alloc == 0) {
    DEBUG("Skipping mapping I/O registers for early inited DW UART\n");
  } else {
#endif
    DEBUG("Mapping I/O registers for DW UART\n");
    dw->port.reg_base = nk_io_map(reg_base, reg_size, 0);
    if(dw->port.reg_base == NULL) {
      ERROR("Failed to map DW UART registers!\n");
      goto err_exit;
    } else {
      DEBUG("Mapped I/O registers for DW UART\n");
    }
    did_map = 1;
#ifdef NAUT_CONFIG_DW_8250_UART_EARLY_OUTPUT
  }
#endif

  uint32_t reg_width;
  if(!nk_dev_info_read_u32(info, "reg-io-width", &reg_width)) {
    switch(reg_width) {
      case 1:
        dw->port.ops.write_reg = dw_8250_write_reg8;
        break;
      case 4:
        dw->port.ops.write_reg = dw_8250_write_reg32;
        break;
      default:
        ERROR("Could not assign uart_8250_ops.write_reg for DW UART, using default!\n");
        break;
    }
  }

  if(nk_dev_info_read_u32(info, "reg-shift", &dw->port.reg_shift)) {
    dw->port.reg_shift = 0;
  }

  dw->port.irq = nk_dev_info_read_irq(info, 0);

  if(dw->port.irq == NK_NULL_IRQ) {
    ERROR("Failed to read DW UART irq!\n");
    goto err_exit;
  }

  char name_buf[DEV_NAME_LEN];
  snprintf(name_buf,DEV_NAME_LEN,"serial%u",nk_dev_get_serial_device_number());

  struct nk_char_dev *dev = nk_char_dev_register(name_buf,NK_DEV_FLAG_NO_WAIT,&generic_8250_char_dev_int,(void*)&dw->port);
  dw->port.dev = dev;

  if(dev == NULL) {
    ERROR("Failed to register DW UART as a character device!\n");
    goto err_exit;
  }
  did_register = 1;

  // Set up UART
  generic_8250_disable_fifos(&dw->port);
  generic_8250_clear_fifos(&dw->port);

  uart_port_set_baud_rate(&dw->port.port, 115200);
  uart_port_set_word_length(&dw->port.port, 8);
  uart_port_set_parity(&dw->port.port, UART_PARITY_ODD);
  uart_port_set_stop_bits(&dw->port.port, 2);

  if(nk_irq_add_handler_dev(dw->port.irq, uart_8250_interrupt_handler, (void*)dw, (struct nk_dev *)dev)) {
    ERROR("Failed to add IRQ handler for DW UART!\n");
  }

  generic_8250_enable_fifos(&dw->port);
  generic_8250_enable_recv_interrupts(&dw->port);

  nk_dev_info_set_device(info, (struct nk_dev*)dev);

  nk_unmask_irq(dw->port.irq);

  return 0;

err_exit:
  if(did_alloc) {
    free(dw);
  }
  if(did_map) {
    nk_io_unmap(dw->port.reg_base);
  }
  if(did_register) {
    nk_char_dev_unregister(dev);
  }
  return -1;
}

static struct of_dev_id dw_8250_of_dev_ids[] = {
  {.compatible = "snps,dw-apb-uart"}
};
declare_of_dev_match_id_array(dw_8250_of_match, dw_8250_of_dev_ids);

int dw_8250_init(void)
{
  of_for_each_match(&dw_8250_of_match, dw_8250_dev_init_one);
  return 0;
}

