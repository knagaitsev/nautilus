
#include<dev/pl011.h>

#include<nautilus/dev.h>
#include<nautilus/chardev.h>
#include<nautilus/spinlock.h>
#include<nautilus/of/fdt.h>
#include<nautilus/backtrace.h>

#include<nautilus/of/dt.h>

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

#define UART_INT_RECV         (1<<4)
#define UART_INT_TRANS        (1<<5)
#define UART_INT_RECV_TIMEOUT (1<<6)
#define UART_INT_FRAMING_ERR  (1<<7)
#define UART_INT_PARITY_ERR   (1<<8)
#define UART_INT_BREAK_ERR    (1<<9)
#define UART_INT_OVERRUN_ERR  (1<<10)

#ifndef NAUT_CONFIG_DEBUG_PL011_UART
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

#define PRINT(fmt, args...) INFO_PRINT("[pl011] " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("[pl011] " fmt, ##args)
#define ERROR(fmt, args...) ERROR_PRINT("[pl011] " fmt, ##args)
#define WARN(fmt, args...) WARN_PRINT("[pl011] " fmt, ##args)

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

static inline int pl011_uart_configure(struct pl011_uart *p) {

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

  // Re-enable fifo
  //pl011_enable_fifo(p);
  
  // Re-enable the PL011
  pl011_enable(p);
  
  return 0;
}
            
#ifdef NAUT_CONFIG_PL011_UART_EARLY_OUTPUT

static struct pl011_uart pre_vc_pl011;
static uint64_t pre_vc_pl011_dtb_offset = -1;

#define QEMU_PL011_VIRT_BASE_ADDR 0x9000000
#define QEMU_PL011_VIRT_BASE_CLOCK 24000000 // 24 MHz Clock
         
void pl011_uart_pre_vc_puts(char *s) {
  pl011_uart_puts_blocking(&pre_vc_pl011, s);
}
void pl011_uart_pre_vc_putchar(char c) {
  pl011_uart_putchar_blocking(&pre_vc_pl011, c);
}

void pl011_uart_pre_vc_init(uint64_t dtb) {

  struct pl011_uart *p = &pre_vc_pl011;
 
  int offset = fdt_node_offset_by_compatible((void*)dtb, -1, "arm,pl011");

  if(offset < 0) {
    return 1;
  }

  pre_vc_pl011_dtb_offset = offset;

  void *base = QEMU_PL011_VIRT_BASE_ADDR;
  uint64_t clock = QEMU_PL011_VIRT_BASE_CLOCK;

  fdt_reg_t reg = { .address = 0, .size = 0 };
  if(!fdt_getreg(dtb, offset, &reg)) {
     base = reg.address;
  } else {
    return 1;
  }
  
  // TODO: Still need to use the fdt to read the clock speed but it's less clear how to get it,
  // because we need to get to the clock device through a phandle, and Nautilus currently
  // doesn't have any way to go from device tree entry phandle, to actual device

  p->dev = NULL;
  p->mmio_base = base;
  p->clock = clock; 
  p->baudrate = 115200;
  spinlock_init(&p->input_lock);
  spinlock_init(&p->output_lock);

  if(pl011_uart_configure(p)) {
    return 1;
  }

  if(nk_pre_vc_register(pl011_uart_pre_vc_putchar, pl011_uart_pre_vc_puts)) {
    return 1;
  }

  return 0;
}
#endif

static int pl011_uart_dev_get_characteristics(void *state, struct nk_char_dev_characteristics *c) {
  memset(c,0,sizeof(struct nk_char_dev_characteristics));
  return 0;
}

static void __pl011_uart_putchar(struct pl011_uart *p, uint8_t src) {
  pl011_write_reg(p, UART_DATA, (uint32_t)src);
}
static uint32_t __pl011_uart_getchar(struct pl011_uart *p) {
  uint32_t c = (uint32_t)pl011_read_reg(p, UART_DATA);

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

  return c;
}

static inline void pl011_uart_puts_non_blocking(struct pl011_uart *p, const char *src) {
  while(*src != '\0') {
    while(pl011_busy(p) || pl011_write_full(p)) {}
    __pl011_uart_putchar(p, *src);
    src++;
  }
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

  flags = spin_lock_irq_save(&p->input_lock);
  if(!pl011_read_empty(p)) {
    ret |= NK_CHARDEV_READABLE;
  }
  spin_unlock_irq_restore(&p->input_lock, flags);

  flags = spin_lock_irq_save(&p->output_lock);
  if (!pl011_write_full(p)) {
    ret |= NK_CHARDEV_WRITEABLE;
  }
  spin_unlock_irq_restore(&p->output_lock, flags);

  return ret;
}

static struct nk_char_dev_int pl011_uart_char_dev_ops = {
  .get_characteristics = pl011_uart_dev_get_characteristics,
  .read = pl011_uart_dev_read,
  .write = pl011_uart_dev_write,
  .status = pl011_uart_dev_status
};

static int pl011_interrupt_handler(struct nk_irq_action *action, struct nk_regs *, void *state) {

  struct pl011_uart *p = (struct pl011_uart*)state;

  if(p == NULL) {
    ERROR("null UART in interrupt handler!\n");
    return 0;
  }

  uint32_t int_status = pl011_read_reg(p, UART_MASK_INT_STAT);

  if(p->dev == NULL) {
    ERROR("null UART Device in interrupt handler!\n");
    return 0;
  }

  if(int_status & (UART_INT_RECV | UART_INT_RECV_TIMEOUT)) {
    nk_dev_signal(p->dev);
  }

  pl011_write_reg(p, UART_INT_CLR, int_status);

  return 0;
}

int pl011_uart_init_one(struct nk_dev_info *info) 
{
  struct nk_dev *existing_device = nk_dev_info_get_device(info);
  if(existing_device != NULL) {
    ERROR("Trying to initialize a device node (%s) as PL011 when it already has a device: %s\n", nk_dev_info_get_name(info), existing_device->name); 
  }

  struct pl011_uart *uart;
  int did_alloc = 0;
  int registered_device = 0;
#ifdef NAUT_CONFIG_PL011_UART_EARLY_OUTPUT
  if(info->type == NK_DEV_INFO_OF && ((struct dt_node *)info->state)->dtb_offset != pre_vc_pl011_dtb_offset) { 
    uart = malloc(sizeof(struct pl011_uart));
    did_alloc = 1;
  } else {
    uart = &pre_vc_pl011;
  }
#else
  uart = malloc(sizeof(struct pl011_uart));
  did_alloc = 1;
#endif

  if(uart == NULL) {
    ERROR("init_one: uart = NULL\n");
    goto err_exit;
  }

  int mmio_size;
  int ret;
  if(ret = nk_dev_info_read_register_block(info, &uart->mmio_base, &mmio_size)) {
    ERROR("init_one: Failed to read register block! ret = %u\n", ret);
    goto err_exit;
  }

  if(pl011_uart_configure(uart)) {
    ERROR("init_one: Failed to configure PL011!\n");
    goto err_exit;
  }

  if(nk_dev_info_read_irq(info, &uart->irq)) {
    ERROR("init_one: Failed to read IRQ!\n");
    goto err_exit;
  }

  char name_buf[DEV_NAME_LEN];
  snprintf(name_buf,DEV_NAME_LEN,"serial%u",nk_dev_get_serial_device_number());

  uart->dev = nk_char_dev_register(name_buf,0,&pl011_uart_char_dev_ops,(void*)uart);

  if(uart->dev == NULL) {
    ERROR("init_one: Failed to allocate PL011 chardev!\n");
    goto err_exit;
  }
  registered_device = 1;

  nk_dev_info_set_device(info, uart->dev);

  // Clear out spurrious interrupts
  uint32_t int_status = pl011_read_reg(uart, UART_RAW_INT_STAT);
  pl011_write_reg(uart, UART_INT_CLR, int_status);

  nk_irq_add_handler_dev(uart->irq, pl011_interrupt_handler, (void*)uart, (struct nk_dev*)uart->dev);
  nk_unmask_irq(uart->irq);

  pl011_write_reg(uart, UART_INT_MASK, 0x50);

  return 0;

err_exit:
  if(did_alloc) {
    free(uart);
  }
  if(registered_device) {
    nk_char_dev_unregister(uart->dev);
  }
  return 1;
}

static struct of_dev_id pl011_dev_ids[] = {
  {.compatible = "arm,pl011"}
};
declare_of_dev_match_id_array(pl011_of_match, pl011_dev_ids);

int pl011_uart_init(void) {
  return of_for_each_match(&pl011_of_match, pl011_uart_init_one);
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

