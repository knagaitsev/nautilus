
#include<dev/8250/generic.h>

#ifdef NAUT_CONFIG_HAS_PORT_IO
#include<nautilus/arch.h>
#endif

uint64_t generic_8250_read_reg(struct generic_8250 *uart, uint32_t reg_offset)
{
   uint64_t val;
   uint64_t addr = uart->reg_base + (reg_offset << uart->reg_shift);
   switch(uart->reg_width) {
     case 0:
     case 1:
#ifdef NAUT_CONFIG_HAS_PORT_IO
       if(uart->flags & GENERIC_8250_FLAG_PORT_IO) {
         val = inb(addr);
       } else {
         val = *(volatile uint8_t*)((void*)addr);
       } 
#else
       val = *(volatile uint8_t*)((void*)addr);
#endif
       break;
     case 2:
#ifdef NAUT_CONFIG_HAS_PORT_IO
       if(uart->flags & GENERIC_8250_FLAG_PORT_IO) {
         val = inw(addr);
       } else {
         val = *(volatile uint16_t*)((void*)addr);
       }
#else
       val = *(volatile uint16_t*)((void*)addr);
#endif
       break;
     case 4:
#ifdef NAUT_CONFIG_HAS_PORT_IO
       if(uart->flags & GENERIC_8250_FLAG_PORT_IO) {
         val = inl(addr);
       } else {
         val = *(volatile uint32_t*)((void*)addr);
       }
#else
       val = *(volatile uint32_t*)((void*)addr);
#endif
       break;
     case 8:
#ifdef NAUT_CONFIG_HAS_PORT_IO
       if(uart->flags & GENERIC_8250_FLAG_PORT_IO) {
         val = 0;
       } else {
         val = *(volatile uint64_t*)((void*)addr);
       }
#else
       val = *(volatile uint64_t*)((void*)addr);
#endif
       break;
   }
   return val;
}

void generic_8250_write_reg(struct generic_8250 *uart, uint32_t reg_offset, uint64_t val)
{
   uint64_t addr = uart->reg_base + (reg_offset << uart->reg_shift);
   switch(uart->reg_width) {
     case 0:
     case 1:
#ifdef NAUT_CONFIG_HAS_PORT_IO
       if(uart->flags & GENERIC_8250_FLAG_PORT_IO) {
         outb(val,addr);
       } else {
         *(volatile uint8_t*)((void*)addr) = (uint8_t)val;
       } 
#else
       *(volatile uint8_t*)((void*)addr) = (uint8_t)val;
#endif
       break;
     case 2:
#ifdef NAUT_CONFIG_HAS_PORT_IO
       if(uart->flags & GENERIC_8250_FLAG_PORT_IO) {
         outw(val,addr);
       } else {
         *(volatile uint16_t*)((void*)addr) = (uint16_t)val;
       }
#else
       *(volatile uint16_t*)((void*)addr) = (uint16_t)val;
#endif
       break;
     case 4:
#ifdef NAUT_CONFIG_HAS_PORT_IO
       if(uart->flags & GENERIC_8250_FLAG_PORT_IO) {
         outl(val,addr);
       } else {
         *(volatile uint32_t*)((void*)addr) = (uint32_t)val;
       }
#else
       *(volatile uint32_t*)((void*)addr) = (uint32_t)val;
#endif
       break;
     case 8:
#ifdef NAUT_CONFIG_HAS_PORT_IO
       if(uart->flags & GENERIC_8250_FLAG_PORT_IO) {
         val = 0;
       } else {
         *(volatile uint64_t*)((void*)addr) = (uint64_t)val;
       }
#else
       *(volatile uint64_t*)((void*)addr) = (uint64_t)val;
#endif
       break;
   }
}

int generic_8250_init(struct generic_8250 *uart) 
{
  spinlock_init(&uart->input_lock);
  spinlock_init(&uart->output_lock);

  // Set up the buffers
  if(uart->output_buffer == NULL) {
    if(uart->output_buffer_size == 0) {
      uart->output_buffer_size = GENERIC_8250_DEFAULT_BUFFER_SIZE;
    }
    uart->output_buffer = malloc(uart->output_buffer_size);
    uart->output_buffer_head = 0;
    uart->output_buffer_tail = 0;
  }
  if(uart->input_buffer == NULL) {
    if(uart->input_buffer_size == 0) {
      uart->input_buffer_size = GENERIC_8250_DEFAULT_BUFFER_SIZE;
    }
    uart->input_buffer = malloc(uart->input_buffer_size);
    uart->input_buffer_head = 0;
    uart->input_buffer_tail = 0;
  }

  if(uart->ops->configure) {
    uart->ops->configure(uart);
  }

  if(uart->ops->interrupt_handler != NULL) {
    nk_irq_add_handler_dev(uart->uart_irq, 
                           uart->ops->interrupt_handler,
                           uart,
                           (struct nk_dev*)uart->dev);
    nk_unmask_irq(uart->uart_irq);
  }

  return 0;
}

/*
 * Feature Probing Functions
 */

int generic_8250_probe_scratch(struct generic_8250 *uart) 
{
  generic_8250_write_reg(uart,GENERIC_8250_SCR,0xde);
  if(generic_8250_read_reg(uart,GENERIC_8250_SCR) != 0xde) {
    return 0;
  } else {
    return 1;
  }
}

// Should only be called with FIFO's disabled 
// (Returns FIFO size, 0 if no functioning FIFO)
int generic_8250_probe_fifo(struct generic_8250 *uart) 
{
  // Try enabling FIFO's
  generic_8250_write_reg(uart,GENERIC_8250_FCR,
      GENERIC_8250_FCR_ENABLE_FIFO|
      GENERIC_8250_FCR_CLEAR_RECV_FIFO|
      GENERIC_8250_FCR_CLEAR_XMIT_FIFO|
      GENERIC_8250_FCR_ENABLE_64BYTE_FIFO|
      GENERIC_8250_FCR_64BYTE_TRIGGER_LEVEL_56BYTE);

  uint64_t iir = generic_8250_read_reg(uart,iir);
  uint64_t fifo_mask = iir & GENERIC_8250_IIR_FIFO_MASK;
  switch(fifo_mask) {
    case GENERIC_8250_IIR_FIFO_ENABLED:
      if(iir & GENERIC_8250_IIR_64BYTE_FIFO_ENABLED) {
        return 64;
      } else {
        return 16;
      }
      break;
    default:
    case GENERIC_8250_IIR_FIFO_NON_FUNC:
    case GENERIC_8250_IIR_FIFO_NONE:
      return 0;
      break;
  }

  // Disable FIFO's
  generic_8250_write_reg(uart,GENERIC_8250_FCR,0);
}

/*
 * Buffer Functions
 */

static void input_buffer_push(struct generic_8250 *uart, uint8_t val) {
  uart->input_buffer[uart->input_buffer_tail] = val;
  uart->input_buffer_tail = (uart->input_buffer_tail+1) % uart->input_buffer_size;
}
static uint8_t input_buffer_pull(struct generic_8250 *uart) {
  uint8_t ret = uart->input_buffer[uart->input_buffer_head];
  uart->input_buffer_head = (uart->input_buffer_head+1) % uart->input_buffer_size;
  return ret;
}
static int input_buffer_empty(struct generic_8250 *uart) {
  return uart->input_buffer_head == uart->input_buffer_tail;
}
static int input_buffer_full(struct generic_8250 *uart) {
  return ((uart->input_buffer_tail+1)&uart->input_buffer_size) == uart->input_buffer_head;
}

static void output_buffer_push(struct generic_8250 *uart, uint8_t val) {
  uart->output_buffer[uart->output_buffer_tail] = val;
  uart->output_buffer_tail = (uart->output_buffer_tail+1) % uart->output_buffer_size;
}
static uint8_t output_buffer_pull(struct generic_8250 *uart) {
  uint8_t ret = uart->output_buffer[uart->output_buffer_head];
  uart->output_buffer_head = (uart->output_buffer_head+1) % uart->output_buffer_size;
  return ret;
}
static int output_buffer_empty(struct generic_8250 *uart) {
  return uart->output_buffer_head == uart->output_buffer_tail;
}
static int output_buffer_full(struct generic_8250 *uart) {
  return ((uart->output_buffer_tail+1)&uart->output_buffer_size) == uart->output_buffer_head;
}

/*
 * Go around the buffers
 */

void generic_8250_direct_putchar(struct generic_8250 *uart, uint8_t c) 
{  
  uint8_t ls;
  do {
    ls = (uint8_t)generic_8250_read_reg(uart,GENERIC_8250_LSR);
  }
  while (!(ls & GENERIC_8250_LSR_XMIT_EMPTY));

  generic_8250_write_reg(uart,GENERIC_8250_THR,c);
}


/*
 * Generic Implementations of "generic_8250_ops"
 */

int generic_8250_configure(struct generic_8250 *uart) 
{
  // Enable access to dll and dlm
  generic_8250_write_reg(uart,GENERIC_8250_LCR,GENERIC_8250_LCR_DLAB_BITMASK);

  // TODO: Support Baudrates other than 115200
  generic_8250_write_reg(uart,GENERIC_8250_DLL,1);
  generic_8250_write_reg(uart,GENERIC_8250_DLH,0);

  // Disable access to dll and dlm and set:
  //     8 bit words,
  //     1 stop bit,
  //     no parity
  generic_8250_write_reg(uart,GENERIC_8250_LCR,
      GENERIC_8250_LCR_WORD_LENGTH_8BITS|
      GENERIC_8250_LCR_PARITY_NONE);

  // Enable data received interrupts
  if(uart->flags & GENERIC_8250_FLAG_NO_INTERRUPTS) {
    // make sure all interrupts are disabled
    generic_8250_write_reg(uart,GENERIC_8250_IER,0);
  }
  else {
    generic_8250_write_reg(uart,GENERIC_8250_IER,GENERIC_8250_IER_RECV_DATA_AVAIL);
  }

  if(uart->flags & GENERIC_8250_FEAT_HAS_FIFO) {
    // For now we will just disable the fifo's entirely
    generic_8250_write_reg(uart,GENERIC_8250_FCR,0);
  }

  return 0;
}

int generic_8250_kick_input(struct generic_8250 *uart) 
{
    uint64_t count=0;
    
    while (!input_buffer_full(uart)) { 
	uint8_t ls = (uint8_t)generic_8250_read_reg(uart,GENERIC_8250_LSR);
	if (ls & GENERIC_8250_LSR_PARITY_ERROR) {
	    // parity error, skip this byte
	    continue;
	}
	if (ls & GENERIC_8250_LSR_FRAMING_ERROR) { 
	    // framing error, skip this byte
	    continue;
	}
	if (ls & GENERIC_8250_LSR_BREAK_INTERRUPT) { 
	    // break interrupt, but we do want this byte
	}
	if (ls & GENERIC_8250_LSR_OVERRUN_ERROR) { 
	    // overrun error - we have lost a byte
	    // but we do want this next one
	}
	if (ls & GENERIC_8250_LSR_DATA_READY) { 
	    // grab a byte from the device if there is room
	    uint8_t data = (uint8_t)generic_8250_read_reg(uart,GENERIC_8250_RBR);
	    input_buffer_push(uart,data);
	    count++;
	} else {
	    // chip is empty, stop receiving from it
	    break;
	}
    }

    if (count>0) {
	nk_dev_signal((struct nk_dev *)uart->dev);
    }

    return 0;
}

int generic_8250_kick_output(struct generic_8250 *uart) 
{
    uint64_t count=0;

    while (!output_buffer_empty(uart)) { 
	uint8_t ls = generic_8250_read_reg(uart,GENERIC_8250_LSR);
	if (ls & GENERIC_8250_LSR_XMIT_EMPTY) { 
	    // transmit holding register is empty
	    // drive a byte to the device
	    uint8_t data = output_buffer_pull(uart);
	    generic_8250_write_reg(uart,GENERIC_8250_THR,data);
	    count++;
	} else {
	    // chip is full, stop sending to it
	    // but since we have more data, have it
	    // interrupt us when it has room
	    uint8_t ier = (uint8_t)generic_8250_read_reg(uart,GENERIC_8250_IER);
	    ier |= GENERIC_8250_IER_XMIT_REG_EMPTY;
	    generic_8250_write_reg(uart,GENERIC_8250_IER,ier);
	    goto out;
	}
    }
    
    // the chip has room, but we have no data for it, so
    // disable the transmit interrupt for now
    uint8_t ier = (uint8_t)generic_8250_read_reg(uart,GENERIC_8250_IER);
    ier &= ~GENERIC_8250_IER_XMIT_REG_EMPTY;
    generic_8250_write_reg(uart,GENERIC_8250_IER,ier);
 out:
    if (count>0) { 
	nk_dev_signal((struct nk_dev*)(uart->dev));
    }
    return 0;
}

int generic_8250_interrupt_handler(struct nk_irq_action *action, struct nk_regs *regs, void *state) 
{

  struct generic_8250 *uart = (struct generic_8250*)state;

  uint8_t iir;
  
  do {
      iir = (uint8_t)generic_8250_read_reg(uart,GENERIC_8250_IIR);

      switch (iir & 0xf)  {
      case 0: // modem status reset + ignore
          (void)generic_8250_read_reg(uart,GENERIC_8250_MSR);
          break;
      case 2: // THR empty (can send more data)
          spin_lock(&uart->output_lock);
          if(uart->ops && uart->ops->kick_output) {
            uart->ops->kick_output(uart);
          }
          spin_unlock(&uart->output_lock);
          break;
      case 4:  // received data available 
      case 12: // received data available (FIFO timeout)
          spin_lock(&uart->input_lock);
          if(uart->ops && uart->ops->kick_input) {
            uart->ops->kick_input(uart);
          }
          spin_unlock(&uart->input_lock);
          break;
      case 6: // line status reset + ignore
          (void)generic_8250_read_reg(uart,GENERIC_8250_LSR);
          break;
      case 1:   // done
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

struct generic_8250_ops generic_8250_default_ops = {
  .configure = generic_8250_configure,
  .kick_input = generic_8250_kick_input,
  .kick_output = generic_8250_kick_output,
  .interrupt_handler = generic_8250_interrupt_handler
};

/*
 * Generic Implementations of the 8250 chardev ops
 */

int generic_8250_char_dev_get_characteristics(void *state, struct nk_char_dev_characteristics *c) 
{
    memset(c,0,sizeof(*c));
    return 0;
}
int generic_8250_char_dev_read(void *state, uint8_t *dest) 
{
  struct generic_8250 *uart = (struct generic_8250*)state;

  int rc = -1;
  int flags;

  flags = spin_lock_irq_save(&uart->input_lock);

  if(input_buffer_empty(uart)) {
    rc = 0;
  } else {
    *dest = input_buffer_pull(uart);
    rc = 1;
  }

  spin_unlock_irq_restore(&uart->input_lock, flags);
  return rc;
}

int generic_8250_char_dev_write(void *state, uint8_t *src)
{ 
  struct generic_8250 *uart = (struct generic_8250*)state;

  int wc = -1;
  int flags;

  flags = spin_lock_irq_save(&uart->output_lock);

  if(output_buffer_full(uart)) {
    wc = 0;
  } else {
    output_buffer_push(uart, *src);
    wc = 1;
  }

  if(uart->ops && uart->ops->kick_output) {
    uart->ops->kick_output(uart);
  }
  spin_unlock_irq_restore(&uart->output_lock, flags);
  return wc;
}

int generic_8250_char_dev_status(void *state) 
{
  struct generic_8250 *uart = (struct generic_8250*)state;

  int status = 0;
  int flags;

  flags = spin_lock_irq_save(&uart->input_lock);
  if(!input_buffer_empty(uart)) {
    status |= NK_CHARDEV_READABLE;
  } 
  spin_unlock_irq_restore(&uart->input_lock, flags);

  flags = spin_lock_irq_save(&uart->output_lock);
  if(!output_buffer_full(uart)) {
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

