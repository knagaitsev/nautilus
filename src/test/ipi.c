
#include<nautilus/interrupt.h>
#include<nautilus/shell.h>

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


