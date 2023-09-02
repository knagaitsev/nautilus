#include <nautilus/nautilus.h>
#include <nautilus/interrupt.h>
#include <nautilus/chardev.h>
#include <nautilus/dev.h>
#include <nautilus/of/dt.h>
#include <arch/riscv/sbi.h>
#include <dev/sifive_serial.h>

#ifndef NAUT_CONFIG_DEBUG_SIFIVE_SERIAL
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

#define PRINT(fmt, args...) INFO_PRINT("[sifive] " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("[sifive] " fmt, ##args)
#define ERROR(fmt, args...) ERROR_PRINT("[sifive] " fmt, ##args)
#define WARN(fmt, args...) WARN_PRINT("[sifive] " fmt, ##args)

#define UART_TXFIFO_FULL  0x80000000
#define UART_RXFIFO_EMPTY 0x80000000
#define UART_RXFIFO_DATA  0x000000ff
#define UART_TXCTRL_TXEN  0x1
#define UART_RXCTRL_RXEN  0x1
#define UART_IP_RXWM      0x2

#define SIFIVE_DEFAULT_BAUD_RATE 115200

#ifdef NAUT_CONFIG_SIFIVE_SERIAL_EARLY_OUTPUT
static int pre_vc_sifive_serial_dtb_offset = -1;
static struct sifive_serial pre_vc_sifive;

static void pre_vc_sifive_putchar(uint8_t c) 
{
  while (pre_vc_sifive.regs->txfifo & UART_TXFIFO_FULL) {}
  pre_vc_sifive.regs->txfifo = c;
}

int sifive_serial_fdt_init(const void *fdt, int offset, struct sifive_serial *sifive) {
    int lenp = 0;
    sifive->regs = (struct sifive_serial_regs*)fdt_getreg_address(fdt, offset);
    if(sifive->regs == NULL) {
      return -1;
    }
    return 0;
}

int sifive_serial_pre_vc_init(void *fdt) 
{
  memset(&pre_vc_sifive, 0, sizeof(struct sifive_serial));
  int offset = fdt_node_offset_by_compatible((void*)fdt, offset, "sifive,uart0");

  if(offset < 0) {
    return -1;
  }

  pre_vc_sifive_serial_dtb_offset = offset;

  sifive_serial_fdt_init(fdt, offset, &pre_vc_sifive);

  pre_vc_sifive.regs->txctrl = UART_TXCTRL_TXEN;

  // Disable receiving and interrupts
  pre_vc_sifive.regs->rxctrl = (pre_vc_sifive.regs->rxctrl & ~UART_RXCTRL_RXEN);
  pre_vc_sifive.regs->ie = 0;

  return nk_pre_vc_register(pre_vc_sifive_putchar, NULL);
}
#endif

static struct sifive_serial_regs * regs;
static bool_t   inited = false;

int sifive_handler (struct nk_irq_action *action, struct nk_regs *r, void *state) {
    panic("sifive_handler!\n");
    while (1) {
        uint32_t r = regs->rxfifo;
        if (r & UART_RXFIFO_EMPTY) break;
        char buf = r & 0xFF;
        // call virtual console here!
        sifive_serial_putchar(buf);
    }
}

int sifive_test(void) {
    arch_enable_ints();
    while(1) {
        //printk("ie=%p, ip=%p, sie=%p, status=%p, sip=%p, pp=%p\t\n", regs->ie, regs->ip, read_csr(sie), read_csr(sstatus), read_csr(sip), plic_pending());
        printk("BEFORE WFI\n");
        asm volatile ("wfi");
        printk("AFTER WFI\n");
        /* printk("%d\n", sifive_serial_getchar()); */
    }
}


static int sifive_init_one(struct nk_dev_info *info) 
{
  struct sifive_serial *sifive;

  int did_alloc = 0;
  int did_register = 0;

  struct nk_dev *existing_device = nk_dev_info_get_device(info);
  if(existing_device != NULL) {
    ERROR("Trying to initialize a device node (%s) as SiFive Serial device when it already has a device: %s\n", nk_dev_info_get_name(info), existing_device->name); 
  }

#ifdef NAUT_CONFIG_SIFIVE_SERIAL_EARLY_OUTPUT
  if(info->type == NK_DEV_INFO_OF && ((struct dt_node *)info->state)->dtb_offset != pre_vc_sifive_serial_dtb_offset) {
    sifive = malloc(sizeof(struct sifive_serial));
    did_alloc = 1;
  } else {
    sifive = &pre_vc_sifive;
    did_alloc = 0;
  }
#else
  sifive = malloc(sizeof(struct sifive_serial));
  did_alloc = 1;
#endif

  uint64_t mmio_base;
  int mmio_size;
  if(nk_dev_info_read_register_block(info, &mmio_base, &mmio_size)) {
    ERROR("init_one: Failed to read register block!\n");
    goto err_exit;
  }

  sifive->regs = (struct sifive_serial_regs *)mmio_base;

  sifive->regs->txctrl = UART_TXCTRL_TXEN;
  sifive->regs->rxctrl = UART_RXCTRL_RXEN;
  sifive->regs->ie = 0b10;

  if(nk_dev_info_read_irq(info, &sifive->irq)) {
    ERROR("init_one: Failed to read IRQ!\n");
    goto err_exit;
  }

  char name_buf[DEV_NAME_LEN];
  snprintf(name_buf,DEV_NAME_LEN,"serial%u",nk_dev_get_serial_device_number());
// TODO
  //sifive->dev = nk_char_dev_register(name_buf,0,&sifive_serial_char_dev_ops,(void*)sifive);

  if(sifive->dev == NULL) {
    ERROR("init_one: Failed to register device!\n");
    goto err_exit;
  } else {
    did_register = 1;
  }

  nk_dev_info_set_device(info, (struct nk_dev*)sifive->dev);

  nk_irq_add_handler_dev(sifive->irq, sifive_handler, NULL, (struct nk_dev*)sifive->dev);
  nk_unmask_irq(sifive->irq);

  INFO("SIFIVE @ %p, irq=%d\n", (void*)mmio_base, sifive->irq);

  return 0;

err_exit:

  if(did_register) {
    nk_char_dev_unregister(sifive->dev);
  }
  if(did_alloc) {
    free(sifive);
  }

  return -1;
}

void sifive_serial_write(const char *b) {
    while (b && *b) {
        sifive_serial_putchar(*b);
        b++;
    }
}

void sifive_serial_putchar(unsigned char ch) {
    if (!inited) {
        sbi_call(SBI_CONSOLE_PUTCHAR, ch);
    } else {
        while (regs->txfifo & UART_TXFIFO_FULL) {
        }

        regs->txfifo = ch;
    }
}

int sifive_serial_getchar(void) {
    if (!inited) {
        struct sbiret ret = sbi_call(SBI_CONSOLE_GETCHAR);
        return ret.error == -1 ? -1 : ret.value;
    } else {
        uint32_t r = regs->rxfifo;
        return (r & UART_RXFIFO_EMPTY) ? -1 : r & 0xFF;
    }
}

