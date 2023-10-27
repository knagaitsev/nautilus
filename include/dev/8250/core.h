#ifndef __8250_H__
#define __8250_H__

#include<nautilus/spinlock.h>
#include<nautilus/chardev.h>
#include<dev/uart.h>

#define UART_8250_RBR 0
#define UART_8250_THR 0
#define UART_8250_DLL 0
#define UART_8250_DLH 1
#define UART_8250_IER 1
#define UART_8250_IIR 2
#define UART_8250_FCR 2
#define UART_8250_LCR 3
#define UART_8250_MCR 4
#define UART_8250_LSR 5
#define UART_8250_MSR 6
#define UART_8250_SCR 7

#define UART_8250_IER_RECV_DATA_AVAIL  (1<<0)
#define UART_8250_IER_XMIT_REG_EMPTY   (1<<1)
#define UART_8250_IER_RECV_LINE_STATUS (1<<2)
#define UART_8250_IER_MODEM_STATUS     (1<<3)
#define UART_8250_IER_SLEEP_MODE       (1<<4)
#define UART_8250_IER_LOW_POWER_MODE   (1<<5)

#define UART_8250_IIR_NONE              0b0001
#define UART_8250_IIR_MSR_RESET         0b0000
#define UART_8250_IIR_XMIT_REG_EMPTY    0b0010
#define UART_8250_IIR_RECV_DATA_AVAIL   0b0100
#define UART_8250_IIR_LSR_RESET         0b0110
#define UART_8250_IIR_RECV_TIMEOUT      0b1100

#define UART_8250_IIR_64BYTE_FIFO_ENABLED (1<<5)
#define UART_8250_IIR_FIFO_MASK     0b11000000
#define UART_8250_IIR_FIFO_NONE     0b00000000
#define UART_8250_IIR_FIFO_NON_FUNC 0b10000000
#define UART_8250_IIR_FIFO_ENABLED  0b11000000

#define UART_8250_FCR_ENABLE_FIFO (1<<0)
#define UART_8250_FCR_CLEAR_RECV_FIFO (1<<1)
#define UART_8250_FCR_CLEAR_XMIT_FIFO (1<<2)
#define UART_8250_FCR_DMA_MODE_SELECT (1<<3)
#define UART_8250_FCR_ENABLE_64BYTE_FIFO (1<<5)
#define UART_8250_FCR_TRIGGER_LEVEL_MASK   0b11000000
#define UART_8250_FCR_TRIGGER_LEVEL_1BYTE  0b00000000
#define UART_8250_FCR_TRIGGER_LEVEL_4BYTE  0b01000000
#define UART_8250_FCR_TRIGGER_LEVEL_8BYTE  0b10000000
#define UART_8250_FCR_TRIGGER_LEVEL_14BYTE 0b11000000
#define UART_8250_FCR_64BYTE_TRIGGER_LEVEL_16BYTE 0b01000000
#define UART_8250_FCR_64BYTE_TRIGGER_LEVEL_32BYTE 0b10000000
#define UART_8250_FCR_64BYTE_TRIGGER_LEVEL_56BYTE 0b11000000

#define UART_8250_LCR_WORD_LENGTH_MASK  0b11
#define UART_8250_LCR_WORD_LENGTH_5BITS 0b00
#define UART_8250_LCR_WORD_LENGTH_6BITS 0b01
#define UART_8250_LCR_WORD_LENGTH_7BITS 0b10
#define UART_8250_LCR_WORD_LENGTH_8BITS 0b11
#define UART_8250_LCR_DUAL_STOP_BIT (1<<2)
#define UART_8250_LCR_PARITY_MASK  0b111000
#define UART_8250_LCR_PARITY_NONE  0b000000
#define UART_8250_LCR_PARITY_ODD   0b001000
#define UART_8250_LCR_PARITY_EVEN  0b011000
#define UART_8250_LCR_PARITY_MARK  0b101000
#define UART_8250_LCR_PARITY_SPACE 0b101000
#define UART_8250_LCR_SET_BREAK_ENABLE (1<<6)
#define UART_8250_LCR_DLAB     (1<<7)

#define UART_8250_LSR_DATA_READY      (1<<0)
#define UART_8250_LSR_OVERRUN_ERROR   (1<<1)
#define UART_8250_LSR_PARITY_ERROR    (1<<2)
#define UART_8250_LSR_FRAMING_ERROR   (1<<3)
#define UART_8250_LSR_BREAK_INTERRUPT (1<<4)
#define UART_8250_LSR_XMIT_EMPTY      (1<<5)
#define UART_8250_LSR_XMIT_IDLE       (1<<6)
#define UART_8250_LSR_RECV_FIFO_ERROR (1<<7)

struct uart_8250_port;

struct uart_8250_ops 
{
  unsigned int (*read_reg)(struct uart_8250_port *port, int reg_offset);
  void (*write_reg)(struct uart_8250_port *port, int reg_offset, unsigned int val);

  int(*xmit_full)(struct uart_8250_port *port);
  int(*recv_empty)(struct uart_8250_port *port);

  int(*xmit_push)(struct uart_8250_port *port, uint8_t);
  int(*recv_pull)(struct uart_8250_port *port, uint8_t*);

  int(*kick_output)(struct uart_8250_port *port);
  int(*kick_input)(struct uart_8250_port *port);

  int(*handle_irq)(struct uart_8250_port *port, unsigned int iir);
};

struct uart_8250_port
{
  struct uart_port port;

  uint64_t reg_base;
  int reg_shift;

  nk_irq_t irq;

  struct nk_dev *dev;

  spinlock_t input_lock;
  spinlock_t output_lock;

  struct uart_8250_ops ops;
};

// Initialized the struct with default values but doesn't init the device
int uart_8250_struct_init(struct uart_8250_port *port);

// Generic interrupt handler to register (invokes port->ops.handle_irq)
int uart_8250_interrupt_handler(struct nk_irq_action *action, struct nk_regs *reg, void *state);

// Wrappers for uart_8250_ops
inline static 
unsigned int uart_8250_read_reg(struct uart_8250_port *port, int offset) 
{
  return port->ops.read_reg(port, offset);
}

inline static 
void uart_8250_write_reg(struct uart_8250_port *port, int offset, unsigned int val) 
{
  port->ops.write_reg(port, offset, val);
}

inline static
int uart_8250_xmit_full(struct uart_8250_port *port) 
{
  return port->ops.xmit_full(port);
}
inline static
int uart_8250_recv_empty(struct uart_8250_port *port) 
{
  return port->ops.recv_empty(port);
}
inline static
int uart_8250_xmit_push(struct uart_8250_port *port, uint8_t src) 
{
  return port->ops.xmit_push(port,src);
}
inline static
int uart_8250_recv_pull(struct uart_8250_port *port, uint8_t *dest) 
{
  return port->ops.recv_pull(port,dest);
}
inline static
int uart_8250_kick_output(struct uart_8250_port *port) 
{
  return port->ops.kick_output(port);
}
inline static
int uart_8250_kick_input(struct uart_8250_port *port) 
{
  return port->ops.kick_input(port);
}
inline static
int uart_8250_handle_irq(struct uart_8250_port *port, unsigned int iir)
{
  return port->ops.handle_irq(port,iir);
}

/*
 * generic uart_ops
 */

int generic_8250_set_baud_rate(struct uart_port *port, unsigned int baud_rate);
int generic_8250_set_word_length(struct uart_port *port, unsigned int bits);
int generic_8250_set_parity(struct uart_port *port, unsigned int parity);
int generic_8250_set_stop_bits(struct uart_port *port, unsigned int bits);
extern struct uart_ops uart_8250_default_uart_ops;

/*
 * generic uart_8250_ops 
 */

// read_reg and write_reg
#ifdef NAUT_CONFIG_HAS_PORT_IO
unsigned int generic_8250_read_reg_port8(struct uart_8250_port *port, int offset);
void generic_8250_write_reg_port8(struct uart_8250_port *port, int offset, unsigned int val);
#endif

unsigned int generic_8250_read_reg_mem8(struct uart_8250_port *port, int offset);
void generic_8250_write_reg_mem8(struct uart_8250_port *port, int offset, unsigned int val);

unsigned int generic_8250_read_reg_mem32(struct uart_8250_port *port, int offset);
void generic_8250_write_reg_mem32(struct uart_8250_port *port, int offset, unsigned int val);

// recv_empty
int generic_8250_recv_empty(struct uart_8250_port *port);
// xmit_full
int generic_8250_xmit_full(struct uart_8250_port *port);
// recv_pull
int generic_8250_recv_pull(struct uart_8250_port *port, uint8_t *dest);
// xmit_push
int generic_8250_xmit_push(struct uart_8250_port *port, uint8_t src);
// kick_input
int generic_8250_kick_input(struct uart_8250_port *);
// kick_output
int generic_8250_kick_output(struct uart_8250_port *);
// handle_irq
int generic_8250_handle_irq(struct uart_8250_port *port, unsigned int iir);

extern struct uart_8250_ops uart_8250_default_uart_8250_ops;

/*
 * generic helper functions
 */

int generic_8250_probe_scratch(struct uart_8250_port *);
void generic_8250_direct_putchar(struct uart_8250_port *, uint8_t c);
int generic_8250_enable_fifos(struct uart_8250_port *);
int generic_8250_disable_fifos(struct uart_8250_port *);
int generic_8250_clear_fifos(struct uart_8250_port *);
int generic_8250_enable_recv_interrupts(struct uart_8250_port *);

/*
 * generic nk_char_dev_int ops 
 */

int generic_8250_char_dev_get_characteristics(void *state, struct nk_char_dev_characteristics *c);
int generic_8250_char_dev_read(void *state, uint8_t *dest);
int generic_8250_char_dev_write(void *state, uint8_t *src);
int generic_8250_char_dev_status(void *state);
extern struct nk_char_dev_int generic_8250_char_dev_int;

#endif
