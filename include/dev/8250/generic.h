#ifndef __GENERIC_8250_H__
#define __GENERIC_8250_H__

#include<nautilus/chardev.h>
#include<nautilus/spinlock.h>

/*
 * There is pretty much zero chance you actually run into an 8250 line serial uart,
 * but because so many uart's inherit from it's design, this driver is a general implementation
 * which can be used for many 8250 or 16550 compatible uart drivers, and modified to handle
 * their various quirks
 */

#define GENERIC_8250_DEFAULT_BUFFER_SIZE 512

#define GENERIC_8250_THR 0
#define GENERIC_8250_RBR 0
#define GENERIC_8250_DLL 0
#define GENERIC_8250_IER 1
#define GENERIC_8250_DLH 1
#define GENERIC_8250_IIR 2
#define GENERIC_8250_FCR 2
#define GENERIC_8250_LCR 3
#define GENERIC_8250_MCR 4
#define GENERIC_8250_LSR 5
#define GENERIC_8250_MSR 6
#define GENERIC_8250_SCR 7

#define GENERIC_8250_IER_RECV_DATA_AVAIL  (1<<0)
#define GENERIC_8250_IER_XMIT_REG_EMPTY   (1<<1)
#define GENERIC_8250_IER_RECV_LINE_STATUS (1<<2)
#define GENERIC_8250_IER_MODEM_STATUS     (1<<3)
#define GENERIC_8250_IER_SLEEP_MODE       (1<<4)
#define GENERIC_8250_IER_LOW_POWER_MODE   (1<<5)

#define GENERIC_8250_IIR_NONE              0b0001
#define GENERIC_8250_IIR_MSR_RESET         0b0000
#define GENERIC_8250_IIR_XMIT_REG_EMPTY    0b0010
#define GENERIC_8250_IIR_RECV_DATA_AVAIL   0b0100
#define GENERIC_8250_IIR_LSR_RESET         0b0110
#define GENERIC_8250_IIR_RECV_TIMEOUT      0b1100

#define GENERIC_8250_IIR_64BYTE_FIFO_ENABLED (1<<5)
#define GENERIC_8250_IIR_FIFO_MASK     0b11000000
#define GENERIC_8250_IIR_FIFO_NONE     0b00000000
#define GENERIC_8250_IIR_FIFO_NON_FUNC 0b10000000
#define GENERIC_8250_IIR_FIFO_ENABLED  0b11000000

#define GENERIC_8250_FCR_ENABLE_FIFO (1<<0)
#define GENERIC_8250_FCR_CLEAR_RECV_FIFO (1<<1)
#define GENERIC_8250_FCR_CLEAR_XMIT_FIFO (1<<2)
#define GENERIC_8250_FCR_DMA_MODE_SELECT (1<<3)
#define GENERIC_8250_FCR_ENABLE_64BYTE_FIFO (1<<5)
#define GENERIC_8250_FCR_TRIGGER_LEVEL_MASK   0b11000000
#define GENERIC_8250_FCR_TRIGGER_LEVEL_1BYTE  0b00000000
#define GENERIC_8250_FCR_TRIGGER_LEVEL_4BYTE  0b01000000
#define GENERIC_8250_FCR_TRIGGER_LEVEL_8BYTE  0b10000000
#define GENERIC_8250_FCR_TRIGGER_LEVEL_14BYTE 0b11000000
#define GENERIC_8250_FCR_64BYTE_TRIGGER_LEVEL_16BYTE 0b01000000
#define GENERIC_8250_FCR_64BYTE_TRIGGER_LEVEL_32BYTE 0b10000000
#define GENERIC_8250_FCR_64BYTE_TRIGGER_LEVEL_56BYTE 0b11000000

#define GENERIC_8250_LCR_WORD_LENGTH_MASK  0b11
#define GENERIC_8250_LCR_WORD_LENGTH_5BITS 0b00
#define GENERIC_8250_LCR_WORD_LENGTH_6BITS 0b01
#define GENERIC_8250_LCR_WORD_LENGTH_7BITS 0b10
#define GENERIC_8250_LCR_WORD_LENGTH_8BITS 0b11
#define GENERIC_8250_LCR_DUAL_STOP_BIT (1<<2)
#define GENERIC_8250_LCR_PARITY_MASK  0b111000
#define GENERIC_8250_LCR_PARITY_NONE  0b000000
#define GENERIC_8250_LCR_PARITY_ODD   0b001000
#define GENERIC_8250_LCR_PARITY_EVEN  0b011000
#define GENERIC_8250_LCR_PARITY_MARK  0b101000
#define GENERIC_8250_LCR_PARITY_SPACE 0b101000
#define GENERIC_8250_LCR_SET_BREAK_ENABLE (1<<6)
#define GENERIC_8250_LCR_DLAB_BITMASK     (1<<7)

#define GENERIC_8250_LSR_DATA_READY      (1<<0)
#define GENERIC_8250_LSR_OVERRUN_ERROR   (1<<1)
#define GENERIC_8250_LSR_PARITY_ERROR    (1<<2)
#define GENERIC_8250_LSR_FRAMING_ERROR   (1<<3)
#define GENERIC_8250_LSR_BREAK_INTERRUPT (1<<4)
#define GENERIC_8250_LSR_XMIT_EMPTY      (1<<5)
#define GENERIC_8250_LSR_XMIT_IDLE       (1<<6)
#define GENERIC_8250_LSR_RECV_FIFO_ERROR (1<<7)

// Extra flags to differentiate features of different models/implementations
#define GENERIC_8250_FLAG_PORT_IO            (1<<0)
#define GENERIC_8250_FLAG_NO_INTERRUPTS      (1<<1)

#define GENERIC_8250_FEAT_HAS_SCRATCH    (1<<0)
#define GENERIC_8250_FEAT_HAS_FIFO       (1<<1)
#define GENERIC_8250_FEAT_HAS_SLEEP      (1<<2)
#define GENERIC_8250_FEAT_HAS_LOW_POWER  (1<<3)

struct generic_8250;

struct generic_8250_ops {

  int(*configure)(struct generic_8250*);
  int(*kick_input)(struct generic_8250*);
  int(*kick_output)(struct generic_8250*);
  int(*interrupt_handler)(struct nk_irq_action*, struct nk_regs*, void*);

};

struct generic_8250 {

  struct nk_char_dev *dev;

  nk_irq_t uart_irq;

  void * reg_base;
  int reg_shift;
  int reg_width;

  int baud_rate;
  int baud_rate_divisor_multiplier;

  int features;
  int flags;

  int fifo_size;

  spinlock_t input_lock;
  spinlock_t output_lock;

  uint8_t *input_buffer;
  uint32_t input_buffer_size;
  uint32_t input_buffer_head;
  uint32_t input_buffer_tail;

  uint8_t *output_buffer;
  uint32_t output_buffer_size;
  uint32_t output_buffer_head;
  uint32_t output_buffer_tail;

  struct generic_8250_ops *ops;

};

uint64_t generic_8250_read_reg(struct generic_8250 *uart, uint32_t reg_offset);
void generic_8250_write_reg(struct generic_8250 *uart, uint32_t reg_offset, uint64_t val);

int generic_8250_init(struct generic_8250 *uart);

/*
 * Feature Probing Functions
 */

int generic_8250_probe_scratch(struct generic_8250 *uart);

// Should only be called with FIFO's disabled 
// (Returns FIFO size, 0 if no functioning FIFO)
int generic_8250_probe_fifo(struct generic_8250 *uart);

/*
 * Direct output functions which go around the buffering,
 * should only be used for early output before interrupts are possible
 */

// blocking
void generic_8250_direct_putchar(struct generic_8250 *uart, uint8_t c);

/*
 * Generic Implementations of "generic_8250_ops"
 */

int generic_8250_configure(struct generic_8250 *uart);
int generic_8250_kick_input(struct generic_8250 *uart);
int generic_8250_kick_output(struct generic_8250 *uart); 
int generic_8250_interrupt_handler(struct nk_irq_action *action, struct nk_regs *regs, void *state);
extern struct generic_8250_ops generic_8250_default_ops;

/*
 * Generic Implementations of the 8250 chardev ops
 */

int generic_8250_char_dev_get_characteristics(void *state, struct nk_char_dev_characteristics *c);
int generic_8250_char_dev_read(void *state, uint8_t *dest);
int generic_8250_char_dev_write(void *state, uint8_t *src);
int generic_8250_char_dev_status(void *state);
extern struct nk_char_dev_int generic_8250_char_dev_int;

#endif
