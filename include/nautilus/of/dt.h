#ifndef __NAUT_DEVICE_TREE_H__
#define __NAUT_DEVICE_TREE_H__

#include<nautilus/dev.h>

#define DT_NODE_FLAG_INT_PARENT_PHANDLE  (1<<0)
#define DT_NODE_FLAG_CACHED_IRQ_DATA     (1<<1)
#define DT_NODE_FLAG_CACHED_IRQ_EXT_DATA (1<<2)

struct dt_node;
struct dt_node {
  struct nk_dev_info dev_info;
  int flags;

  uint32_t phandle;

  // Cached IRQ (EXT) data
  int num_irq;
  int irq_cells;
  int num_irq_extended;
  uint32_t *extended_parent_phandles;
  int *extended_offsets;

  struct dt_node *parent;
  struct dt_node *siblings;
  struct dt_node *children;
  int child_count;

  union {
    uint32_t interrupt_parent_phandle;
    struct dt_node *interrupt_parent;
  };

  void *dtb;
  int dtb_offset;

  struct dt_node *next_node;
};

struct of_dev_properties {
  const int count;
  const char **names;
};

struct of_dev_id {
  const char *name;
  const char *compatible;
  const struct of_dev_properties *properties;
};

struct of_dev_match {
  const struct of_dev_id * ids;
  const int num_ids;

  const int max_num_matches;
};

#define declare_of_dev_match_id_array(VAR, ID_ARR) \
  static struct of_dev_match VAR = { \
    .ids = ID_ARR, \
    .num_ids = sizeof(ID_ARR)/sizeof(struct of_dev_id) \
  };

int fdt_free_tree(struct dt_node *root);
int fdt_unflatten_tree(void *fdt, struct dt_node **root);


int of_init(void *fdt);
// Frees the device tree data structure at the end of initialization
int of_cleanup(void);

// Returns the number of devices which matched but init_callback returned NULL
int of_for_each_match(const struct of_dev_match *match, int(*callback)(struct nk_dev_info *info));
int of_for_each_subnode_match(const struct of_dev_match *match, int(*callback)(struct nk_dev_info *info), struct nk_dev_info *root);

#endif
