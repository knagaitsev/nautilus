
#include<nautilus/ipi.h>

#ifndef NAUT_CONFIG_DEBUG
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) 
#endif

#define IPI_ERROR(fmt, args...) ERROR_PRINT("[IPI] " fmt, ##args)
#define IPI_DEBUG(fmt, args...) DEBUG_PRINT("[IPI] " fmt, ##args)
#define IPI_PRINT(fmt, args...) printk("[IPI] " fmt, ##args)

struct stress_state {
  int stressing;
  int running;
  nk_thread_id_t tid;
  nk_irq_t ipi_to_spam;
  cpu_id_t cpu_to_spam;
  uint64_t delay;
  uint64_t num_sent;
};

static volatile struct stress_state *cpu_states = NULL;

__attribute__((annotate("nohook")))
static int spam_ipi_thread(void *input, void **output) 
{
  // This CPU gets dedicated to spamming 
  uint8_t iflag = irq_disable_save();
  volatile struct stress_state *state = &cpu_states[my_cpu_id()];
  while(state->stressing) {
    for(volatile uint64_t i = 0; i < state->delay; i++) {
    }
    if(nk_send_ipi(state->ipi_to_spam, state->cpu_to_spam)) {
      IPI_ERROR("ERROR: Failed to Send IPI (%u) to CPU (%u)\n", state->ipi_to_spam, state->cpu_to_spam);
      state->stressing = 0;
      return 1;
    } else {
      state->num_sent += 1;
    }
  }
  state->running = 0;
  irq_enable_restore(iflag);
  return 0;
}

int ipi_stress_init(void) {
  cpu_states = malloc(sizeof(struct stress_state) * nk_get_num_cpus());
  memset(cpu_states, 0, sizeof(struct stress_state) * nk_get_num_cpus());
}

int set_ipi_stress(cpu_id_t from, cpu_id_t to, nk_irq_t ipi, uint64_t delay) 
{
  if(cpu_states == NULL) {
    return -1;
  }

  int num_cpus = nk_get_num_cpus();

  if(to >= num_cpus) {
    return -1;
  }
  if(from >= num_cpus) {
    return -1;
  }

  volatile struct stress_state *state = &cpu_states[from];

  if(state->stressing) {
    return -1;
  }

  state->ipi_to_spam = ipi;
  state->cpu_to_spam = to;
  state->delay = delay;
  state->stressing = 1;

  if(state->running) {
    if(clear_ipi_stress(from, NULL)) {
      IPI_ERROR("Failed to clear IPI stress state from CPU %d\n", from);
      return -1;
    }
  }

  state->num_sent = 0;

  if(nk_thread_create (
    (nk_thread_fun_t)spam_ipi_thread,
    NULL,
    NULL,
    1,
    0x2000,
    &state->tid,
    from
    )) {
    state->stressing = 0;
    IPI_ERROR("Failed to create spam thread!\n");
    return -1;
  }

  if(nk_thread_run(state->tid)) {
    state->stressing = 0;
    IPI_ERROR("Failed to run spam thread!\n");
    return -1;
  } else {
    state->running = 1;
  }

  return 0;
}

int clear_ipi_stress(cpu_id_t stressor, uint64_t * num_sent) 
{
  if(cpu_states == NULL) {
    return -1;
  }
  if(stressor >= nk_get_num_cpus()) {
    return -1;
  }
  volatile struct stress_state *state = &cpu_states[stressor];

  if(num_sent != NULL) {
    *num_sent = state->num_sent;
  }

  state->stressing = 0;

  return 0;
}

static int handle_spam_ipi(char * buf, void * priv)
{
  int ipi_n, cpu_n, bound_cpu_n, delay_n;

  if(sscanf(buf, "spamipi %d %d %d %d", &ipi_n, &cpu_n, &bound_cpu_n, &delay_n) != 4) {
    printk("spamipi [IPI] [TO] [FROM] [DELAY]\n");
    return 0;
  }

  if(set_ipi_stress((cpu_id_t)bound_cpu_n, (cpu_id_t)cpu_n, (nk_irq_t)ipi_n, (uint64_t)delay_n)) {
    printk("Failed to set IPI spam\n");
    return 0;
  }

  printk("Dedicated CPU %u to sending IPI (%u) to CPU %u\n",
      bound_cpu_n, ipi_n, cpu_n);

  return 0;
}

static int handle_clear_spam_ipi(char * buf, void * priv)
{
  int from_n;

  if(sscanf(buf, "stopipi %d", &from_n) != 1) {
    printk("stopipi [FROM]\n");
    return 0;
  }

  if(clear_ipi_stress((cpu_id_t)from_n, NULL)) {
    IPI_ERROR("Failed to clear IPI spam\n");
    return 0;
  }

  printk("Cleared IPI spamming state of CPU %d\n", from_n);
  return 0;
}



static int
handle_ls_ipi(char * buf, void * priv) 
{
  nk_irq_t max = nk_max_irq();

  printk("--- Registered IPI's ---\n");
  for(nk_irq_t i = 0; i <= max; i++) {
    struct nk_irq_desc *desc = nk_irq_to_desc(i);

    if(desc == NULL) {
      continue;
    }

    if(!(desc->flags & NK_IRQ_DESC_FLAG_IPI)) {
      continue;
    }

    nk_dump_irq(i);
  }

  printk("------------------------\n");

  return 0;
}

static int handle_send_ipi(char * buf, void * priv)
{
  int ipi_n, cpu_n;
  if(sscanf(buf, "sendipi %d %d", &ipi_n, &cpu_n) != 2) {
    printk("sendipi [IPI] [TO]\n");
    return 0;
  }

  nk_irq_t ipi = (nk_irq_t)ipi_n;
  cpu_id_t cpu = (cpu_id_t)cpu_n;

  uint32_t num_cpus = nk_get_num_cpus();
  if(cpu >= num_cpus) {
    printk("Invalid CPU (%u) [NR_CPUS=%u]\n", cpu, num_cpus);
    return 0;
  }

  int res = nk_send_ipi(ipi, cpu);

  if(res != 0) {
    printk("ERROR: Failed to Send IPI (%u) to CPU (%u)\n", ipi, cpu);
    return 0;
  }

  return 0;
}

static struct shell_cmd_impl ls_ipi_impl = {
    .cmd      = "lsipi",
    .help_str = "List registered IPI's",
    .handler  = handle_ls_ipi,
};
nk_register_shell_cmd(ls_ipi_impl);

static struct shell_cmd_impl send_ipi_impl = {
    .cmd      = "sendipi",
    .help_str = "sendipi [IPI] [TO]",
    .handler  = handle_send_ipi,
};
nk_register_shell_cmd(send_ipi_impl);

static struct shell_cmd_impl spam_ipi_impl = {
    .cmd      = "spamipi",
    .help_str = "spamipi [IPI] [TO] [FROM]",
    .handler  = handle_spam_ipi,   
};
nk_register_shell_cmd(spam_ipi_impl);

static struct shell_cmd_impl clear_spam_ipi_impl = {
    .cmd      = "stopipi",
    .help_str = "stopipi [FROM]",
    .handler  = handle_clear_spam_ipi,   
};
nk_register_shell_cmd(clear_spam_ipi_impl);


