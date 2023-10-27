
#include<nautilus/nautilus.h>
#include<nautilus/naut_types.h>
#include<dev/8250/core.h>
#include<dev/8250/pc_8250.h>

#ifndef NAUT_CONFIG_DEBUG_PC_8250_UART
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

#define INFO(fmt, args...) INFO_PRINT("[DW_8250] " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("[DW_8250] " fmt, ##args)
#define ERROR(fmt, args...) ERROR_PRINT("[DW_8250] " fmt, ##args)
#define WARN(fmt, args...) WARN_PRINT("[DW_8250] " fmt, ##args)

#define COM1_3_IRQ 4
#define COM2_4_IRQ 3
#define COM1_ADDR 0x3F8
#define COM2_ADDR 0x2F8
#define COM3_ADDR 0x3E8
#define COM4_ADDR 0x2E8

#ifdef NAUT_CONFIG_PC_8250_UART_EARLY_OUTPUT

#if NAUT_CONFIG_PC_8250_UART_EARLY_COM_PORT == 1
#define EARLY_COM_ADDR COM1_ADDR
#elif NAUT_CONFIG_PC_8250_UART_EARLY_COM_PORT == 2
#define EARLY_COM_ADDR COM2_ADDR
#elif NAUT_CONFIG_PC_8250_UART_EARLY_COM_PORT == 3 
#define EARLY_COM_ADDR COM3_ADDR
#elif NAUT_CONFIG_PC_8250_UART_EARLY_COM_PORT == 4
#define EARLY_COM_ADDR COM4_ADDR
#else
#error "Invalid Value of NAUT_CONFIG_PC_8250_UART_EARLY_COM_PORT!"
#endif

const static struct uart_8250_ops pc_8250_ops = {
  .read_reg = generic_8250_read_reg_port8,
  .write_reg = generic_8250_write_reg_port8
};

static struct uart_8250_port pre_vc_pc_8250;

static void pc_8250_early_putchar(char c) 
{
  generic_8250_direct_putchar(&pre_vc_pc_8250, c);
}

int pc_8250_pre_vc_init(void)
{
  memset(&pre_vc_pc_8250, 0, sizeof(struct uart_8250_port));

  // No need to nk_io_map port io
  pre_vc_pc_8250.reg_base = EARLY_COM_ADDR;

  pre_vc_pc_8250.reg_shift = 0;

  pre_vc_pc_8250.ops = pc_8250_ops;
  uart_8250_struct_init(&pre_vc_pc_8250);

  generic_8250_disable_fifos(&pre_vc_pc_8250);
  generic_8250_clear_fifos(&pre_vc_pc_8250);

  uart_port_set_baud_rate(&pre_vc_pc_8250.port, 115200);
  uart_port_set_word_length(&pre_vc_pc_8250.port, 8);
  uart_port_set_parity(&pre_vc_pc_8250.port, UART_PARITY_NONE);
  uart_port_set_stop_bits(&pre_vc_pc_8250.port, 2);

  if(nk_pre_vc_register(pc_8250_early_putchar, NULL)) {
    return -1;
  }

  return 0;
}

#endif

static struct uart_8250_port *com_ports[4];

int pc_8250_init(void) 
{
  int pre_vc_port = 0;
  for(int i = 1; i <= 4; i++) {
#ifdef NAUT_CONFIG_PC_8250_SPEC_EARLY_PORT
    if(i == NAUT_CONFIG_PC_8250_EARLY_PORT) {
      pre_vc_port = i;
      com_ports[i-1] = &pre_vc_pc_8250;
      continue;
    }
#endif
    com_ports[i-1] = malloc(sizeof(struct uart_8250_port));
    if(com_ports[i-1] == NULL) {
      for(int j = 1; j < i; j++) {
        if(com_ports[j-1] != &pre_vc_pc_8250) {
          free(com_ports[j-1]);
        }
      }
      return -1;
    }
    memset(com_ports[i-1], 0, sizeof(struct uart_8250_port));
  }

  nk_irq_t com1_3 = NK_NULL_IRQ;
  nk_irq_t com2_4 = NK_NULL_IRQ;

  struct nk_irq_dev *ioapic = ioapic_get_dev_by_id(0);
  if(ioapic == NULL) {
    ERROR("Could not get IOAPIC device to get PC 8250 IRQ's!\n");
    return -1;
  }

  if(nk_irq_dev_revmap(ioapic, COM1_3_IRQ, &com1_3)) {
    ERROR("Failed to revamp COM1_3_IRQ!\n");
    return -1;
  }

  if(nk_irq_dev_revmap(ioapic, COM2_4_IRQ, &com2_4)) {
    ERROR("Failed to revamp COM2_4_IRQ!\n");
    return -1;
  }


  if(pre_vc_port != 1) {
    com_ports[0]->irq = com1_3;
    com_ports[0]->reg_base = COM1_ADDR;
    com_ports[0]->ops = pc_8250_ops;
    uart_8250_struct_init(com_ports[0]);
  }
  if(pre_vc_port != 2) {
    com_ports[1]->irq = com2_4;
    com_ports[1]->reg_base = COM2_ADDR;
    com_ports[1]->ops = pc_8250_ops;
    uart_8250_struct_init(com_ports[1]);
  }
  if(pre_vc_port != 3) {
    com_ports[2]->irq = com1_3;
    com_ports[2]->reg_base = COM3_ADDR;
    com_ports[2]->ops = pc_8250_ops;
    uart_8250_struct_init(com_ports[2]);
  }
  if(pre_vc_port != 4) {
    com_ports[3]->irq = com2_4;
    com_ports[3]->reg_base = COM4_ADDR;
    com_ports[3]->ops = pc_8250_ops;
    uart_8250_struct_init(com_ports[3]);
  }

  int num_failed = 0;
  for(int i = 0; i < 4; i++) { 

    char name_buf[DEV_NAME_LEN];
    snprintf(name_buf,DEV_NAME_LEN,"serial%u",nk_dev_get_serial_device_number());

    struct nk_char_dev *dev = nk_char_dev_register(name_buf,NK_DEV_FLAG_NO_WAIT,&generic_8250_char_dev_int,(void*)com_ports[i]);

    if(dev == NULL) {
      num_failed += 1;
      continue;
    }
    com_ports[i]->dev = dev;

    generic_8250_disable_fifos(com_ports[i]);
    generic_8250_clear_fifos(com_ports[i]);

    uart_port_set_baud_rate(&com_ports[i]->port, 115200);
    uart_port_set_word_length(&com_ports[i]->port, 8);
    uart_port_set_parity(&com_ports[i]->port, UART_PARITY_ODD);
    uart_port_set_stop_bits(&com_ports[i]->port, 2);

    if(nk_irq_add_handler_dev(com_ports[i]->irq, uart_8250_interrupt_handler, (void*)com_ports[i], (struct nk_dev *)dev)) {
      ERROR("Failed to add IRQ handler for COM Port %u!\n", i + 1);
      nk_char_dev_unregister(dev);
      num_failed += 1;
      continue;
    }

    generic_8250_enable_fifos(com_ports[i]);
    generic_8250_enable_recv_interrupts(com_ports[i]);

    nk_unmask_irq(com_ports[i]->irq);
  }

  return num_failed;
}

