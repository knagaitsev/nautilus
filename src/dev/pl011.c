
#include<dev/pl011.h>

#include<nautilus/dev.h>
#include<nautilus/chardev.h>
#include<nautilus/spinlock.h>
#include<nautilus/fdt.h>

#define UART_DATA        0x0
#define UART_RECV_STATUS 0x4
#define UART_ERR_CLR     0x4
#define UART_FLAG        0x18
#define UART_IRDA_LP_CTR 0x20
#define UART_INT_BAUD    0x24
#define UART_FRAC_BAUD   0x28
#define UART_LINE_CTRL_H 0x2C
#define UART_CTRL        0x30
#define UART_INT_FIFO    0x34
#define UART_INT_MASK    0x38
#define UART_RAW_INT_STAT     0x3C
#define UART_MASK_INT_STAT    0x40
#define UART_INT_CLR     0x44
#define UART_DMA_CTRL    0x48

#define UART_PERIP_ID_0  0xFE0
#define UART_PERIP_ID_1  0xFE4
#define UART_PERIP_ID_2  0xFE8
#define UART_PERIP_ID_3  0xFEC
#define UART_CELL_ID_0   0xFF0
#define UART_CELL_ID_1   0xFF4
#define UART_CELL_ID_2   0xFF8
#define UART_CELL_ID_3   0xFFC

#define UART_FLAG_CLR_TO_SEND (1<<0)
#define UART_FLAG_DATA_SET_RDY (1<<1)
#define UART_FLAG_DATA_CARR_DETECT (1<<2)
#define UART_FLAG_BSY (1<<3)
#define UART_FLAG_RECV_FIFO_EMPTY (1<<4)
#define UART_FLAG_TRANS_FIFO_FULL (1<<5)
#define UART_FLAG_RECV_FIFO_FULL (1<<6)
#define UART_FLAG_TRANS_FIFO_EMPTY (1<<7)
#define UART_FLAG_RING (1<<8)

#define UART_LINE_CTRL_BRK_BITMASK (1<<0)
#define UART_LINE_CTRL_PARITY_ENABLE_BITMASK (1<<1)
#define UART_LINE_CTRL_EVEN_PARITY_BITMASK (1<<2)
#define UART_LINE_CTRL_TWO_STP_BITMASK (1<<3)
#define UART_LINE_CTRL_FIFO_ENABLE_BITMASK (1<<4)
#define UART_LINE_CTRL_WORD_LENGTH_BITMASK (0b11<<5)
#define UART_LINE_CTRL_STICK_PARITY_BITMASK (1<<7)

#define UART_CTRL_ENABLE_BITMASK (1<<0)
#define UART_CTRL_SIR_ENABLE_BITMASK (1<<1)
#define UART_CTRL_SIR_LOW_POW_BITMASK (1<<2)
#define UART_CTRL_LOOPBACK_ENABLE_BITMASK (1<<7)
#define UART_CTRL_TRANS_ENABLE_BITMASK (1<<8)
#define UART_CTRL_RECV_ENABLE_BITMASK (1<<9)

static inline void pl011_write_reg(struct pl011_uart *p, uint32_t reg, uint32_t val) {
  *(volatile uint32_t*)(p->mmio_base + reg) = val;
}
static inline uint32_t pl011_read_reg(struct pl011_uart *p, uint32_t reg) {
  return *(volatile uint32_t*)(p->mmio_base + reg);
}
static inline void pl011_and_reg(struct pl011_uart *p, uint32_t reg, uint32_t val) {
  *(volatile uint32_t*)(p->mmio_base + reg) &= val;
}
static inline void pl011_or_reg(struct pl011_uart *p, uint32_t reg, uint32_t val) {
  *(volatile uint32_t*)(p->mmio_base + reg) |= val;
}

static inline void pl011_enable(struct pl011_uart *p) {
  pl011_or_reg(p, UART_CTRL, UART_CTRL_ENABLE_BITMASK);
}
static inline void pl011_disable(struct pl011_uart *p) {
  pl011_and_reg(p, UART_CTRL, ~UART_CTRL_ENABLE_BITMASK);
}

static inline int pl011_busy(struct pl011_uart *p) {
  return *((volatile uint32_t*)(p->mmio_base + UART_FLAG)) & UART_FLAG_BSY;
}
static inline int pl011_write_full(struct pl011_uart *p) {
  return *((volatile uint32_t*)(p->mmio_base + UART_FLAG)) & UART_FLAG_TRANS_FIFO_FULL;
}
static inline int pl011_read_empty(struct pl011_uart *p) {
  return *((volatile uint32_t*)(p->mmio_base + UART_FLAG)) & UART_FLAG_RECV_FIFO_EMPTY;
}

static inline void pl011_enable_fifo(struct pl011_uart *p) {
  pl011_or_reg(p, UART_LINE_CTRL_H, UART_LINE_CTRL_FIFO_ENABLE_BITMASK);
}

static inline void pl011_disable_fifo(struct pl011_uart *p) {
  pl011_and_reg(p, UART_LINE_CTRL_H, ~UART_LINE_CTRL_FIFO_ENABLE_BITMASK);
}

static inline void pl011_configure(struct pl011_uart *p) {

  // Disable the PL011 for configuring
  pl011_disable(p);

  // Wait for any transmission to complete
  while(pl011_busy(p)) {}

  // Flushes the fifo queues
  pl011_disable_fifo(p);

  // div = clock / (16 * baudrate)
  // 2^6 * div = (2^2 * clock) / baudrate
  uint32_t div_mult = ((1<<6)*(p->clock)) / p->baudrate;
  uint16_t div_int = (div_mult>>6) & 0xFFFF;
  uint16_t div_frac = div_mult & 0x3F;

  // Write the baudrate divisior registers
  pl011_write_reg(p, UART_INT_BAUD, div_int);
  pl011_write_reg(p, UART_FRAC_BAUD, div_frac);


  // Set the word size to 8 bits
  pl011_or_reg(p, UART_LINE_CTRL_H, UART_LINE_CTRL_WORD_LENGTH_BITMASK);
  pl011_and_reg(p, UART_LINE_CTRL_H, ~UART_LINE_CTRL_PARITY_ENABLE_BITMASK);
  pl011_and_reg(p, UART_LINE_CTRL_H, ~UART_LINE_CTRL_TWO_STP_BITMASK);
  
  // Enable receiving
  pl011_or_reg(p, UART_CTRL, UART_CTRL_RECV_ENABLE_BITMASK | UART_CTRL_TRANS_ENABLE_BITMASK);

  // Disable SIR
  pl011_and_reg(p, UART_CTRL, ~UART_CTRL_SIR_ENABLE_BITMASK);

  // Disable DMA
  pl011_write_reg(p, UART_DMA_CTRL, 0);
  
  // Re-enable the PL011
  pl011_enable(p);
  
}

#define QEMU_PL011_VIRT_BASE_ADDR 0x9000000
#define QEMU_PL011_VIRT_BASE_CLOCK 24000000 // 24 MHz Clock
                                      
void pl011_uart_early_init(struct pl011_uart *p, uint64_t dtb) {

  void *base = QEMU_PL011_VIRT_BASE_ADDR;
  uint64_t clock = QEMU_PL011_VIRT_BASE_CLOCK;

  int offset = fdt_subnode_offset_namelen(dtb, 0, "pl011", 5);
  fdt_reg_t reg = { .address = 0, .size = 0 };
  if(!fdt_getreg(dtb, offset, &reg)) {
     base = reg.address;
  } else {
    // If we can't read from the dtb, then we can't exactly log an error
  }
  

  // TODO: Still need to use the fdt toread the clock speed but it's less clear which field to read

  // The chardev system isn't initialized yet
  p->dev = NULL;
  p->mmio_base = base;
  p->clock = clock; 
  p->baudrate = 100000;
  spinlock_init(&p->input_lock);
  spinlock_init(&p->output_lock);

  pl011_configure(p);
}

static int pl011_uart_dev_get_characteristics(void *state, struct nk_char_dev_characteristics *c) {
  memset(c,0,sizeof(struct nk_char_dev_characteristics));
  return 0;
}

static void __pl011_uart_putchar(struct pl011_uart *p, uint8_t src) {
  pl011_write_reg(p, UART_DATA, (uint32_t)src);
}
static uint32_t __pl011_uart_getchar(struct pl011_uart *p) {
  uint32_t c = (uint32_t)pl011_read_reg(p, UART_DATA);
  if((c&0xFF) == '\r') {
    c = (c & ~0xFF) | '\n';
  }
  return c;
}

// Non-blocking
int pl011_uart_dev_read(void *state, uint8_t *dest) {

  struct pl011_uart *p = (struct pl011_uart*)state;

  int rc = -1;
  int flags;

  if(!spin_try_lock_irq_save(&p->input_lock, &flags)) {
    if(pl011_read_empty(p) || pl011_busy(p)) {
      rc = 0;
    }
    else {
      uint32_t val = __pl011_uart_getchar(p);
      if(val & ~0xFF) {
        // One of the error bits was set
        rc = -1;
      } else {
        *dest = val;
        rc = 1;
      }
    }
    spin_unlock_irq_restore(&p->input_lock, flags);
  } else {
    rc = 0;
  }

  return rc;
}

// Non-blocking
int pl011_uart_dev_write(void *state, uint8_t *src) {

  struct pl011_uart *p = (struct pl011_uart*)state;

  int wc = -1;
  int flags;

  if(!spin_try_lock_irq_save(&p->output_lock, &flags)) {
    if(pl011_busy(p) || pl011_write_full(p)) {
      wc = 0;
    } else {
      __pl011_uart_putchar(p, *src);
      wc = 1;
    }

    spin_unlock_irq_restore(&p->output_lock, flags);
  } else {
    wc = 0;
  }

  return wc;
}

static int pl011_uart_dev_status(void *state) {
  struct pl011_uart *p = (struct pl011_uart*)state;

  int ret = 0;
  int flags;

  if(!spin_try_lock_irq_save(&p->input_lock, &flags)) {
    if(!pl011_busy(p) && !pl011_read_empty(p)) {
      ret |= NK_CHARDEV_READABLE;
    }
    spin_unlock_irq_restore(&p->input_lock, flags);
  }

  if(!spin_try_lock_irq_save(&p->output_lock, &flags)) {
    if (!pl011_busy(p) && !pl011_write_full(p)) {
      ret |= NK_CHARDEV_WRITEABLE;
    }
    spin_unlock_irq_restore(&p->output_lock, flags);
  }

  return ret;
}

static struct nk_char_dev_int pl011_uart_char_dev_ops = {
  .get_characteristics = pl011_uart_dev_get_characteristics,
  .read = pl011_uart_dev_read,
  .write = pl011_uart_dev_write,
  .status = pl011_uart_dev_status
};

static int pl011_interrupt_handler(excp_entry_t *excp, ulong_t vec, void *state) {

  struct pl011_uart *p = (struct pl011_uart*)state;

  uint32_t int_status = pl011_read_reg(p, UART_MASK_INT_STAT);

  nk_dev_signal(p->dev);

  pl011_write_reg(p, UART_INT_CLR, int_status);

  int_status = pl011_read_reg(p, UART_MASK_INT_STAT);

  return 0;
}

int pl011_uart_dev_init(const char *name, struct pl011_uart *p) {

  spin_lock(&p->output_lock);

  // Clear out spurrious interrupts
  uint32_t int_status = pl011_read_reg(p, UART_RAW_INT_STAT);
  pl011_write_reg(p, UART_INT_CLR, int_status);

  arch_irq_install(33, pl011_interrupt_handler, (void*)p);
  pl011_write_reg(p, UART_INT_MASK, 0x50);

  spin_unlock(&p->output_lock);

  // This may print so we need to unlock before calling it
  p->dev = nk_char_dev_register(name,0,&pl011_uart_char_dev_ops, p);

  if(!p->dev) {
    return -1;
  }

  return 0;
}

void pl011_uart_putchar_blocking(struct pl011_uart *p, const char c) {
  int flags = spin_lock_irq_save(&p->output_lock);
  while(pl011_busy(p) || pl011_write_full(p)) {}
  __pl011_uart_putchar(p, c);
  spin_unlock_irq_restore(&p->output_lock, flags);
}
void pl011_uart_puts_blocking(struct pl011_uart *p, const char *src) {
  int flags = spin_lock_irq_save(&p->output_lock);
  while(*src != '\0') {
    while(pl011_busy(p) || pl011_write_full(p)) {}
    __pl011_uart_putchar(p, *src);
    src++;
  }
  spin_unlock_irq_restore(&p->output_lock, flags);
}

