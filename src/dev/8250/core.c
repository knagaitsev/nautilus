
#include<dev/8250/core.h>
#include<nautilus/nautilus.h>
#include<nautilus/interrupt.h>
#include<nautilus/irqdev.h>
#include<nautilus/chardev.h>

#ifdef NAUT_CONFIG_HAS_PORT_IO
#include<nautilus/arch.h>
#endif

#ifndef NAUT_CONFIG_DEBUG_GENERIC_8250_UART
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

#define INFO(fmt, args...) INFO_PRINT("[8250] " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("[8250] " fmt, ##args)
#define ERROR(fmt, args...) ERROR_PRINT("[8250] " fmt, ##args)
#define WARN(fmt, args...) WARN_PRINT("[8250] " fmt, ##args)

int uart_8250_struct_init(struct uart_8250_port *port) 
{
  spinlock_init(&port->input_lock);
  spinlock_init(&port->output_lock);

  // Assign default ops where no ops were assigned
  if(port->ops.read_reg == NULL) {
    port->ops.read_reg = uart_8250_default_uart_8250_ops.read_reg;
  }
  if(port->ops.write_reg == NULL) {
    port->ops.write_reg = uart_8250_default_uart_8250_ops.write_reg;
  }
  if(port->ops.xmit_full == NULL) {
    port->ops.xmit_full = uart_8250_default_uart_8250_ops.xmit_full;
  }
  if(port->ops.recv_empty == NULL) {
    port->ops.recv_empty = uart_8250_default_uart_8250_ops.recv_empty;
  }
  if(port->ops.xmit_push == NULL) {
    port->ops.xmit_push = uart_8250_default_uart_8250_ops.xmit_push;
  }
  if(port->ops.recv_pull == NULL) {
    port->ops.recv_pull = uart_8250_default_uart_8250_ops.recv_pull;
  }
  if(port->ops.kick_output == NULL) {
    port->ops.kick_output = uart_8250_default_uart_8250_ops.kick_output;
  }
  if(port->ops.kick_input == NULL) {
    port->ops.kick_input = uart_8250_default_uart_8250_ops.kick_input;
  }
  if(port->ops.handle_irq == NULL) {
    port->ops.handle_irq = uart_8250_default_uart_8250_ops.handle_irq;
  }

  if(port->port.ops.set_baud_rate == NULL) {
    port->port.ops.set_baud_rate = uart_8250_default_uart_ops.set_baud_rate;
  }
  if(port->port.ops.set_word_length == NULL) {
    port->port.ops.set_word_length = uart_8250_default_uart_ops.set_word_length;
  }
  if(port->port.ops.set_parity == NULL) {
    port->port.ops.set_parity = uart_8250_default_uart_ops.set_parity;
  }
  if(port->port.ops.set_stop_bits == NULL) {
    port->port.ops.set_stop_bits = uart_8250_default_uart_ops.set_stop_bits;
  }

  return 0;
}

int uart_8250_interrupt_handler(struct nk_irq_action *action, struct nk_regs *regs, void *state) 
{
  struct uart_8250_port *port = (struct uart_8250_port *)state;
  unsigned int iir = uart_8250_read_reg(port,UART_8250_IIR);
  return uart_8250_handle_irq(port,iir);
}


/*
 * Implementations of "uart_ops"
 */ 

int generic_8250_set_baud_rate(struct uart_port *port, unsigned int baud_rate) 
{
  struct uart_8250_port *p = (struct uart_8250_port*)port;
  
  // Enable access to dll and dlm
  unsigned long lcr;
  lcr = uart_8250_read_reg(p,UART_8250_LCR);
  uart_8250_write_reg(p,UART_8250_LCR,lcr|UART_8250_LCR_DLAB);

  // TODO: Support Baudrates other than 115200
//#ifdef NAUT_CONFIG_DW_8250_UART_EARLY_OUTPUT
  // KJH - HACK the rockpro clock system is complicated so getting the baudrate right is very difficult
  // luckily, U-Boot can use the UART for output too, and so we can just leave the baud rate alone
//#else
//  if(baud_rate != 115200) {
//    return -1;
//  }
//  uart_8250_write_reg(p, UART_8250_DLL, 1);
//  uart_8250_write_reg(p, UART_8250_DLH, 0);
//#endif

  // Restore LCR
  uart_8250_write_reg(p,UART_8250_LCR,lcr);

  return 0;
}
int generic_8250_set_word_length(struct uart_port *port, unsigned int bits) 
{
  struct uart_8250_port *p = (struct uart_8250_port*)port;

  if(bits >= 5 && bits <= 8) {
    unsigned int lcr;
    lcr = uart_8250_read_reg(p, UART_8250_LCR);
    lcr &= ~UART_8250_LCR_WORD_LENGTH_MASK;
    lcr |= (bits - 5) & UART_8250_LCR_WORD_LENGTH_MASK;
    uart_8250_write_reg(p,UART_8250_LCR,lcr);
    return 0;
  } else {
    return -1;
  } 
}
int generic_8250_set_parity(struct uart_port *port, unsigned int parity)
{
  struct uart_8250_port *p = (struct uart_8250_port*)port;

  unsigned int lcr;
  lcr = uart_8250_read_reg(p,UART_8250_LCR);
  lcr &= ~UART_8250_LCR_PARITY_MASK;
  switch(parity) {
    case UART_PARITY_NONE:
      lcr |= UART_8250_LCR_PARITY_NONE;
      break;
    case UART_PARITY_ODD:
      lcr |= UART_8250_LCR_PARITY_ODD;
      break;
    case UART_PARITY_EVEN:
      lcr |= UART_8250_LCR_PARITY_EVEN;
      break;
    case UART_PARITY_MARK:
      lcr |= UART_8250_LCR_PARITY_MARK;
      break;
    case UART_PARITY_SPACE:
      lcr |= UART_8250_LCR_PARITY_SPACE;
      break;
    default:
      return -1;
  }
  uart_8250_write_reg(p,UART_8250_LCR,lcr);
  return 0;
}
int generic_8250_set_stop_bits(struct uart_port *port, unsigned int bits)
{
  struct uart_8250_port *p = (struct uart_8250_port*)port;

  unsigned int lcr;
  lcr = uart_8250_read_reg(p,UART_8250_LCR);
  switch(bits) {
    case 1:
      lcr &= ~UART_8250_LCR_DUAL_STOP_BIT;
      break;
    case 2:
      lcr |= UART_8250_LCR_DUAL_STOP_BIT;
      break;
    default:
      return -1;
  }
  uart_8250_write_reg(p,UART_8250_LCR,lcr);
  return 0;
}

struct uart_ops uart_8250_default_uart_ops = {
  .set_baud_rate = generic_8250_set_baud_rate,
  .set_word_length = generic_8250_set_word_length,
  .set_parity = generic_8250_set_parity,
  .set_stop_bits = generic_8250_set_stop_bits,
};

/*
 * Default uart_8250_ops implementations
 */

// read_reg and write_reg
#ifdef NAUT_CONFIG_HAS_PORT_IO
unsigned int generic_8250_read_reg_port8(struct uart_8250_port *port, int offset)
{
  return (unsigned int)inb(port->reg_base + (offset << port->reg_shift));
}
void generic_8250_write_reg_port8(struct uart_8250_port *port, int offset, unsigned int val) 
{
  outb((uint8_t)val, port->reg_base + (offset << port->reg_shift));
}
#endif

unsigned int generic_8250_read_reg_mem8(struct uart_8250_port *port, int offset) 
{
  return (unsigned int)*(volatile uint8_t*)(void*)(port->reg_base + (offset << port->reg_shift));
}
void generic_8250_write_reg_mem8(struct uart_8250_port *port, int offset, unsigned int val)
{
  *(volatile uint8_t*)(void*)(port->reg_base + (offset << port->reg_shift)) = (uint8_t)val;
}

unsigned int generic_8250_read_reg_mem32(struct uart_8250_port *port, int offset) 
{
  return (unsigned int)*(volatile uint32_t*)(void*)(port->reg_base + (offset << port->reg_shift));
}
void generic_8250_write_reg_mem32(struct uart_8250_port *port, int offset, unsigned int val) 
{
  *(volatile uint32_t*)(void*)(port->reg_base + (offset << port->reg_shift)) = (uint32_t)val;
}

// recv_empty
int generic_8250_recv_empty(struct uart_8250_port *port) 
{
  unsigned int lsr = uart_8250_read_reg(port, UART_8250_LSR);
  return !(lsr & UART_8250_LSR_DATA_READY);
}
// xmit_full
int generic_8250_xmit_full(struct uart_8250_port *port) 
{
  unsigned int lsr = uart_8250_read_reg(port, UART_8250_LSR);
  return !(lsr & UART_8250_LSR_XMIT_EMPTY);
}
// recv_pull
int generic_8250_recv_pull(struct uart_8250_port *port, uint8_t *dest)
{
  uint8_t c = uart_8250_read_reg(port, UART_8250_RBR);
  
  // Right now the enter key is returning '\r',
  // but chardev consoles ignore that, so we just 
  // force it to '\n'
  if((c&0xFF) == '\r') {
    c = (c & ~0xFF) | '\n';
  }
  // Convert DEL into Backspace
  else if((c&0xFF) == 0x7F) {
    c = (c & ~0xFF) | 0x08;
  }

  *dest = c;
  return 0;
}
// xmit_push
int generic_8250_xmit_push(struct uart_8250_port *port, uint8_t src) 
{
  uart_8250_write_reg(port, UART_8250_THR, (unsigned int)src);
  return 0;
}
int generic_8250_kick_output(struct uart_8250_port *port) 
{
  if(port->dev) {
    nk_dev_signal(port->dev);
  }
  return 0;
}
int generic_8250_kick_input(struct uart_8250_port *port) 
{
  if(port->dev) {
    nk_dev_signal(port->dev);
  }
  return 0;
}

int generic_8250_handle_irq(struct uart_8250_port *port, unsigned int iir) 
{
  switch(iir & 0xF) {
    case UART_8250_IIR_NONE:
      //generic_8250_direct_putchar(port,'N');
      break;
    case UART_8250_IIR_MSR_RESET:
      //generic_8250_direct_putchar(port,'M');
      (void)uart_8250_read_reg(port,UART_8250_MSR);
      break;
    case UART_8250_IIR_XMIT_REG_EMPTY:
      //generic_8250_direct_putchar(port,'X');
      uart_8250_kick_output(port);
      break;
    case UART_8250_IIR_RECV_TIMEOUT:
      //generic_8250_direct_putchar(port,'T');
    case UART_8250_IIR_RECV_DATA_AVAIL:
      //generic_8250_direct_putchar(port,'R');
      uart_8250_kick_input(port);
      break;
    case UART_8250_IIR_LSR_RESET:
      //generic_8250_direct_putchar(port,'L');
      (void)uart_8250_read_reg(port,UART_8250_LSR);
      break;
    default:
      //generic_8250_direct_putchar(port,'U');
      break;
  }

  return 0;
}

struct uart_8250_ops uart_8250_default_uart_8250_ops = {
  .read_reg = generic_8250_read_reg_mem8,
  .write_reg = generic_8250_write_reg_mem8,
  .xmit_full = generic_8250_xmit_full,
  .recv_empty = generic_8250_recv_empty,
  .xmit_push = generic_8250_xmit_push,
  .recv_pull = generic_8250_recv_pull,
  .kick_output = generic_8250_kick_output,
  .kick_input = generic_8250_kick_input,
  .handle_irq = generic_8250_handle_irq
};

/*
 * Common helper functions
 */

int generic_8250_probe_scratch(struct uart_8250_port *uart) 
{
  uart_8250_write_reg(uart,UART_8250_SCR,0xde);
  if(uart_8250_read_reg(uart,UART_8250_SCR) != 0xde) {
    return 0;
  } else {
    return 1;
  }
}

void generic_8250_direct_putchar(struct uart_8250_port *port, uint8_t c) 
{   
  uint8_t ls;

  uart_8250_write_reg(port,UART_8250_THR,c);
  do {
    ls = (uint8_t)uart_8250_read_reg(port,UART_8250_LSR);
  }
  while (!(ls & UART_8250_LSR_XMIT_EMPTY)); 
}

int generic_8250_enable_fifos(struct uart_8250_port *port) 
{
  unsigned int fcr = uart_8250_read_reg(port,UART_8250_FCR);

  uart_8250_write_reg(port,UART_8250_FCR,
      UART_8250_FCR_ENABLE_FIFO|
      UART_8250_FCR_ENABLE_64BYTE_FIFO|
      UART_8250_FCR_64BYTE_TRIGGER_LEVEL_56BYTE);

  unsigned int iir = uart_8250_read_reg(port,UART_8250_IIR);
  unsigned int fifo_mask = iir & UART_8250_IIR_FIFO_MASK;

  switch(fifo_mask) {
    case UART_8250_IIR_FIFO_ENABLED:
      if(iir & UART_8250_IIR_64BYTE_FIFO_ENABLED) {
        return 0;
      } else {
        return 0;
      }
    default:
    case UART_8250_IIR_FIFO_NON_FUNC:
    case UART_8250_IIR_FIFO_NONE:
      return -1;
  }
}
int generic_8250_disable_fifos(struct uart_8250_port *port) 
{
  uart_8250_write_reg(port,UART_8250_FCR,0);
  return 0;
}
int generic_8250_clear_fifos(struct uart_8250_port *port) 
{
  unsigned int fcr = uart_8250_read_reg(port, UART_8250_FCR);
  uart_8250_write_reg(port, UART_8250_FCR,
      fcr|
      UART_8250_FCR_CLEAR_RECV_FIFO|
      UART_8250_FCR_CLEAR_XMIT_FIFO);
  return 0;
}

int generic_8250_enable_recv_interrupts(struct uart_8250_port *port) 
{
  unsigned int ier = uart_8250_read_reg(port, UART_8250_IER);
  ier |= UART_8250_IER_RECV_DATA_AVAIL | UART_8250_IER_RECV_LINE_STATUS;
  uart_8250_write_reg(port, UART_8250_IER, ier);
  return 0;
}

/*
 * Implementations of the 8250 nk_char_dev_int ops
 */

int generic_8250_char_dev_get_characteristics(void *state, struct nk_char_dev_characteristics *c) 
{
    memset(c,0,sizeof(*c));
    return 0;
}

int generic_8250_char_dev_read(void *state, uint8_t *dest) 
{
  struct uart_8250_port *uart = (struct uart_8250_port*)state;

  //generic_8250_direct_putchar(uart,'{');

  int rc = -1;
  int flags;

  flags = spin_lock_irq_save(&uart->input_lock);

  if(uart_8250_recv_empty(uart)) {
    //generic_8250_direct_putchar(uart,'E');
    rc = 0;
  } else {
    if(uart_8250_recv_pull(uart, dest)) {
      //generic_8250_direct_putchar(uart,'$');
      rc = 0;
    } else {
      //generic_8250_direct_putchar(uart,'#');
      rc = 1;
    }
  }

  spin_unlock_irq_restore(&uart->input_lock, flags);

  //generic_8250_direct_putchar(uart,'}');
  return rc;
}

int generic_8250_char_dev_write(void *state, uint8_t *src)
{ 
  struct uart_8250_port *uart = (struct uart_8250_port*)state;

  int wc = -1;
  int flags;

  flags = spin_lock_irq_save(&uart->output_lock);

  if(uart_8250_xmit_full(uart)) {
    wc = 0;
  } else {
    if(uart_8250_xmit_push(uart, *src)) {
      wc = 0;
    } else {
      wc = 1;
    }
  }

  spin_unlock_irq_restore(&uart->output_lock, flags);
  return wc;
}

int generic_8250_char_dev_status(void *state) 
{
  struct uart_8250_port *uart = (struct uart_8250_port*)state;

  int status = 0;
  int flags;

  flags = spin_lock_irq_save(&uart->input_lock);
  if(!uart->ops.recv_empty(uart)) {
    status |= NK_CHARDEV_READABLE;
  } 
  spin_unlock_irq_restore(&uart->input_lock, flags);

  flags = spin_lock_irq_save(&uart->output_lock);
  if(!uart->ops.xmit_full(uart)) {
    status |= NK_CHARDEV_WRITEABLE;
  } 
  spin_unlock_irq_restore(&uart->output_lock, flags);

  return status;
}

struct nk_char_dev_int generic_8250_char_dev_int = {
    .get_characteristics = generic_8250_char_dev_get_characteristics,
    .read = generic_8250_char_dev_read,
    .write = generic_8250_char_dev_write,
    .status = generic_8250_char_dev_status
};

