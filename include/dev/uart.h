#ifndef __UART_H__
#define __UART_H__

#define UART_PARITY_NONE  0
#define UART_PARITY_ODD   1
#define UART_PARITY_EVEN  2
#define UART_PARITY_MARK  3
#define UART_PARITY_SPACE 4

struct uart_port;

struct uart_ops 
{
  int(*set_baud_rate)(struct uart_port *, unsigned int baud_rate);
  int(*set_word_length)(struct uart_port *, unsigned int bits);
  int(*set_parity)(struct uart_port *, unsigned int parity);
  int(*set_stop_bits)(struct uart_port *, unsigned int bits);
};

struct uart_port 
{
  unsigned int baud_rate;
  unsigned int word_length;
  unsigned int parity;
  unsigned int stop_bits;

  struct uart_ops ops;
};

static inline
int uart_port_set_baud_rate(struct uart_port *port, unsigned int baud_rate)
{
  return port->ops.set_baud_rate(port, baud_rate);
}
static inline
int uart_port_set_word_length(struct uart_port *port, unsigned int bits) 
{
  return port->ops.set_word_length(port, bits);
}
static inline
int uart_port_set_parity(struct uart_port *port, unsigned int parity) 
{
  return port->ops.set_parity(port, parity);
}
static inline
int uart_port_set_stop_bits(struct uart_port *port, unsigned int bits) 
{
  return port->ops.set_stop_bits(port, bits);
}

#endif
