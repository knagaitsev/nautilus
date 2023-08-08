
#include<nautilus/nautilus.h>
#include<nautilus/naut_types.h>
#include<dev/8250/pc_8250.h>
#include<dev/8250/generic.h>

struct pc_8250 {
  struct generic_8250 generic;
  int dynamic_alloc;
};

#define COM1_3_IRQ 4
#define COM2_4_IRQ 3
#define COM1_ADDR 0x3F8
#define COM2_ADDR 0x2F8
#define COM3_ADDR 0x3E8
#define COM4_ADDR 0x2E8

#ifdef NAUT_CONFIG_PC_8250_UART_EARLY_OUTPUT

#if NAUT_CONFIG_PC_8250_UART_EARLY_COM_PORT == 1
#define EARLY_COM_IRQ COM1_3_IRQ
#define EARLY_COM_ADDR COM1_ADDR
#elif NAUT_CONFIG_PC_8250_UART_EARLY_COM_PORT == 2
#define EARLY_COM_IRQ COM2_4_IRQ
#define EARLY_COM_ADDR COM2_ADDR
#elif NAUT_CONFIG_PC_8250_UART_EARLY_COM_PORT == 3 
#define EARLY_COM_IRQ COM1_3_IRQ
#define EARLY_COM_ADDR COM3_ADDR
#elif NAUT_CONFIG_PC_8250_UART_EARLY_COM_PORT == 4
#define EARLY_COM_IRQ COM2_4_IRQ
#define EARLY_COM_ADDR COM4_ADDR
#else
#error "Invalid Value of NAUT_CONFIG_PC_8250_UART_EARLY_COM_PORT!"
#endif

static struct pc_8250 pre_vc_pc_8250 = {
  .generic = {
    .uart_irq = EARLY_COM_IRQ,
    .reg_base = EARLY_COM_ADDR,
    .reg_shift = 0,
    .reg_width = 1,
    .baud_rate = 115200,
    .flags = GENERIC_8250_FLAG_PORT_IO | GENERIC_8250_FLAG_NO_INTERRUPTS,
    .ops = &generic_8250_default_ops
  },
  .dynamic_alloc = 0
};

static void pc_8250_early_putchar(char c) {
  generic_8250_direct_putchar(&pre_vc_pc_8250.generic, c);
}

int pc_8250_pre_vc_init(void)
{
  memset(&pre_vc_pc_8250, 0, sizeof(struct pc_8250));

  if(generic_8250_configure(&pre_vc_pc_8250)) {
    return -1;
  }

  if(nk_pre_vc_register(pc_8250_early_putchar, NULL)) {
    return -1;
  }

  return 0;
}

#endif

static struct pc_8250 *com_ports[4];

int pc_8250_init(void) 
{
  for(int i = 1; i <= 4; i++) {
#ifdef NAUT_CONFIG_PC_8250_SPEC_EARLY_PORT
    if(i == NAUT_CONFIG_PC_8250_EARLY_PORT) {
      com_ports[i-1] = &pre_vc_pc_8250;
      continue;
    }
#endif
    com_ports[i-1] = malloc(sizeof(struct pc_8250));
    if(com_ports[i-1] == NULL) {
      for(int j = 1; j < i; j++) {
        if(com_ports[j-1]->dynamic_alloc) {
          free(com_ports[j-1]);
        }
      }
      return -1;
    }
    com_ports[i-1]->dynamic_alloc = 1;
  }

  int num_failed = 0;
  for(int i = 0; i < 4; i++) {

    char name_buf[DEV_NAME_LEN];
    snprintf(name_buf,DEV_NAME_LEN,"serial%u",nk_dev_get_serial_device_number());

    struct nk_char_dev *dev = nk_char_dev_register(name_buf,0,&generic_8250_char_dev_int,(void*)com_ports[i]);

    if(dev == NULL) {
      num_failed += 1;
      continue;
    }

    com_ports[i]->generic.dev = dev;
    if(generic_8250_init(&com_ports[i]->generic)) {
      num_failed += 1;
      continue;
    }
  }

  return num_failed;
}

