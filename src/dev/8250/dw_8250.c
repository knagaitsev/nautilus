
#include<dev/8250/generic.h>
#include<dev/8250/dw_8250.h>
#include<nautilus/of/fdt.h>
#include<nautilus/of/dt.h>

// Driver for the DesignWare 8250 UART

#define DW_8250_USR 0x1f

#define DW_8250_IIR_BUSY 0x7

struct dw_8250 
{
  struct generic_8250 generic;
};

int dw_8250_interrupt_handler(struct nk_irq_action *action, struct nk_regs *regs, void *state)
{
  struct generic_8250 *uart = (struct generic_8250*)state;

  uint8_t iir;

  do {
      int timeout = 0;
      int flags;
      iir = (uint8_t)generic_8250_read_reg(uart,GENERIC_8250_IIR);

      switch (iir & 0xf)  {
      case GENERIC_8250_IIR_MSR_RESET: // modem status reset + ignore
          (void)generic_8250_read_reg(uart,GENERIC_8250_MSR);
          break;
      case GENERIC_8250_IIR_XMIT_REG_EMPTY: // THR empty (can send more data)
          spin_lock(&uart->output_lock);
          if(uart->ops && uart->ops->kick_output) {
            uart->ops->kick_output(uart);
          }
          spin_unlock(&uart->output_lock);
          break;
      case GENERIC_8250_IIR_RECV_TIMEOUT: // received data available (FIFO timeout)
          // There's a quirk where this interrupt could be asserted when there's no
          // actual data (check for this and do a fake read to resolve)
          timeout = 1;
      case GENERIC_8250_IIR_RECV_DATA_AVAIL:  // received data available 
          flags = spin_lock_irq_save(&uart->input_lock);
          if(timeout) {
            uint8_t lsr = (uint8_t)generic_8250_read_reg(uart,GENERIC_8250_LSR);
            if(!(lsr & (GENERIC_8250_LSR_DATA_READY | GENERIC_8250_LSR_BREAK_INTERRUPT))) {
              // Do a fake read to deassert the wrong timeout
              (void)generic_8250_read_reg(uart, GENERIC_8250_RBR);
            }
          }
          if(uart->ops && uart->ops->kick_input) {
            uart->ops->kick_input(uart);
          }
          spin_unlock_irq_restore(&uart->input_lock, flags);
          break;
      case GENERIC_8250_IIR_LSR_RESET: // line status reset + ignore
          (void)generic_8250_read_reg(uart,GENERIC_8250_LSR);
          break;
      case DW_8250_IIR_BUSY:
          // Design ware specific interrupt
          // Read the USR reg to make it go away 
          (void)generic_8250_read_reg(uart,DW_8250_USR);
          break;
      case GENERIC_8250_IIR_NONE:   // done
          // The serial.c implementation does this but I don't know why?
          spin_lock(&uart->output_lock);
          if(uart->ops && uart->ops->kick_output) {
            uart->ops->kick_output(uart);
          } 
          spin_unlock(&uart->output_lock);
          spin_lock(&uart->input_lock);
          if(uart->ops && uart->ops->kick_input) {
            uart->ops->kick_input(uart);
          }
          spin_unlock(&uart->input_lock);
          break;
      default:  // wtf
          break;
      }

  } while ((iir & 0xf) != 1);
  
  return 0;
}

static struct generic_8250_ops dw_8250_ops = {
  .configure = generic_8250_configure,
  .kick_input = generic_8250_kick_input,
  .kick_output = generic_8250_kick_output,
  .interrupt_handler = dw_8250_interrupt_handler
};

#ifdef NAUT_CONFIG_DW_8250_UART_EARLY_OUTPUT
static int dw_8250_fdt_init(uint64_t dtb, uint64_t offset, struct dw_8250 *dw) {

    fdt_reg_t reg = { .address = 0, .size = 0 };

    if(fdt_getreg((void*)dtb, offset, &reg)) {
      return -1;
    }

    dw->generic.reg_base = reg.address;
    
    int lenp;
    uint32_t *reg_shift_ptr = fdt_getprop((void*)dtb, offset, "reg-shift", &lenp);
    if(reg_shift_ptr == NULL) {
      dw->generic.reg_shift = 0;
    } else {
      dw->generic.reg_shift = *reg_shift_ptr;
    }
    
    uint32_t *reg_width_ptr = fdt_getprop((void*)dtb, offset, "reg-io-width", &lenp);
    if(reg_width_ptr == NULL) {
      dw->generic.reg_width = 1;
    } else {
      dw->generic.reg_width = *reg_width_ptr;
    }

    return 0;
}

static struct dw_8250 pre_vc_dw_8250;
static int pre_vc_dw_8250_dtb_offset = -1;

static void dw_8250_early_putchar(char c) {
  generic_8250_direct_putchar(&pre_vc_dw_8250.generic, c);
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

  pre_vc_dw_8250.generic.flags |= GENERIC_8250_FLAG_NO_INTERRUPTS;
  if(generic_8250_configure(&pre_vc_dw_8250.generic)) {
    return -1;
  }

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
  int did_register = 0;

  struct dw_8250 *dw = NULL;
    
#ifdef NAUT_CONFIG_DW_8250_UART_EARLY_OUTPUT
  if(info->type == NK_DEV_INFO_OF && ((struct dt_node*)info->state)->dtb_offset == pre_vc_dw_8250_dtb_offset) {
     dw = &pre_vc_dw_8250;
  } else {
     dw = malloc(sizeof(struct dw_8250));
     memset(dw, 0, sizeof(struct dw_8250));
     if(dw == NULL) {
       goto err_exit;
     }
     did_alloc = 1;
  }
#else
  dw = malloc(sizeof(struct dw_8250));

  if(dw == NULL) {
    goto err_exit;
  }

  did_alloc = 1; 
#endif

  if(nk_dev_info_read_register_block(info, &dw->generic.reg_base, &dw->generic.reg_size)) {
    goto err_exit;
  }

  if(nk_dev_info_read_u32(info, "reg-io-width", &dw->generic.reg_width)) {
    dw->generic.reg_width = 1;
  }

  if(nk_dev_info_read_u32(info, "reg-shift", &dw->generic.reg_shift)) {
    dw->generic.reg_shift = 0;
  }

  if(nk_dev_info_read_irq(info, &dw->generic.uart_irq)) {
    goto err_exit;
  }

  char name_buf[DEV_NAME_LEN];
  snprintf(name_buf,DEV_NAME_LEN,"serial%u",nk_dev_get_serial_device_number());

  struct nk_char_dev *dev = nk_char_dev_register(name_buf,0,&generic_8250_char_dev_int,(void*)dw);
  dw->generic.dev = dev;

  if(dev == NULL) {
    goto err_exit;
  }
  did_register = 1;

  if(generic_8250_init(&dw->generic)) {
    goto err_exit;
  }

  nk_dev_info_set_device(info, (struct nk_dev*)dev);

  return 0;

err_exit:
  if(did_alloc) {
    free(dw);
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
}

