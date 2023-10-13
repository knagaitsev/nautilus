
#include<nautilus/of/dt.h>
#include<nautilus/of/fdt.h>
#include<nautilus/dev.h>
#include<nautilus/endian.h>

#include<lib/libfdt.h>

#ifndef NAUT_CONFIG_DEBUG_UNFLATTENED_DEVICE_TREE
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...) 
#endif

#define ERROR(fmt, args...) ERROR_PRINT("[Device Tree] " fmt, ##args)
#define WARN(fmt, args...) WARN_PRINT("[Device Tree] " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("[Device Tree] " fmt, ##args)
#define INFO(fmt, args...) INFO_PRINT("[Device Tree] " fmt, ##args)

static struct dt_node *dt_node_list = NULL;

static struct dt_node *alloc_dt_node(void) 
{
  struct dt_node *alloc = malloc(sizeof(struct dt_node));

  if(alloc == NULL) {
    return alloc;
  }

  memset(alloc, 0, sizeof(struct dt_node));

  alloc->next_node = dt_node_list;
  dt_node_list = alloc;

  DEBUG_PRINT("Allocated Node\n");

  return alloc;
}

static int free_dt_node_list(void) 
{
  struct dt_node *node = dt_node_list;
  if(node) {
    struct dt_node *to_free = node;
    node = node->next_node;
    free(to_free);
  }
  return 0;
}

static struct dt_node *dt_node_get_root(struct dt_node *node) 
{
  if(node == NULL) {
    return node;
  } 

  struct dt_node *original_node = node;

  while(node->parent != NULL) {
    node = node->parent;

    if(node == original_node) {
      // we have a loop
      ERROR("dt_node_get_root: Loop in device tree structure!\n");
      return NULL;
    }
  }

  return node;
}

static struct dt_node *dt_get_node_from_phandle(uint32_t phandle) 
{
  if(phandle == 0 || phandle == -1) {
    ERROR("Trying to find NULL phandle!\n");
    return NULL;
  }

  struct dt_node *node = dt_node_list;
  while(node != NULL) {

    if(node->phandle == phandle) {
      return node;
    }
  
    node = node->next_node;
  }

  ERROR("Failed to find phandle: %u\n", phandle);
  return NULL;
}

static struct dt_node *dt_node_get_interrupt_parent(struct dt_node *node) 
{
  if(node == NULL) {
    ERROR("dt_node_get_interrupt_parent: node is NULL!\n");
    return NULL;
  }

  if(node->flags & DT_NODE_FLAG_INT_PARENT_PHANDLE) 
  { 
    struct dt_node *interrupt_parent = dt_get_node_from_phandle(node->interrupt_parent_phandle);
    if(interrupt_parent != NULL) {
      node->interrupt_parent = interrupt_parent;
      node->flags &= ~DT_NODE_FLAG_INT_PARENT_PHANDLE;
    } else {
      ERROR("dt_node_get_interrupt_parent: Failed to get interrupt parent from phandle! phandle = %u\n", node->interrupt_parent_phandle);
    }
    return interrupt_parent;
  } 
  else 
  {
    return node->interrupt_parent;
  }
}

/*
 * Device Info Interface Functions
 */

static char *dt_node_get_name(void *state) 
{
  struct dt_node *node = (struct dt_node *)state;

  int lenp;
  char *name = fdt_get_name(node->dtb, node->dtb_offset, &lenp);

  if(name == NULL || lenp < 0) {
    return NULL;
  }

  if(name[lenp] != '\0') {
    // It's not NULL terminated
    return NULL;
  }

  return name;
}
static int dt_node_has_property(void *state, const char *prop_name) 
{
  struct dt_node *node = (struct dt_node *)state;

  return fdt_get_property(node->dtb, node->dtb_offset, prop_name, NULL) != NULL;
}
static int dt_node_read_int_array(void *state, const char *prop_name, int elem_size, void *buf, int *buf_count) 
{
  struct dt_node *node = (struct dt_node *)state;

  struct fdt_property *prop = fdt_get_property(node->dtb, node->dtb_offset, prop_name, NULL);

  if(prop == NULL) {
    return -1;
  }

  int max_count = prop->len / elem_size;
  *buf_count = max_count < *buf_count ? max_count : *buf_count;

  if(arch_little_endian()) {
    // Really this is unnecessary because 99% of properties will either be u32 or u64,
    // but a device can specify a "binary data" property and create u8, u16, u24, u40, etc. arrays technically
    uint8_t *b_buf = (uint8_t*)buf;
    for(int elem = 0; elem < *buf_count; elem++) {
      for(int le = 0; le < elem_size; le++) {
        int be = (elem_size-1)-le;
        b_buf[(elem * elem_size) + le] = prop->data[(elem * elem_size) + be];
      }
    }
  } else {
    // FDT is big endian so just copy the array
    memcpy(buf, prop->data, elem_size * (*buf_count));
  }

  return 0;
}
static int dt_node_read_string_array(void *state, const char *prop_name, char **buf, int *buf_count) 
{
  struct dt_node *node = (struct dt_node *)state;

  int count = fdt_stringlist_count(node->dtb, node->dtb_offset, prop_name);

  if(count < 0) {
    return count;
  }

  *buf_count = count < *buf_count ? count : *buf_count;

  for(int i = 0; i < *buf_count; i++) 
  {
    int lenp;
    char *str = fdt_stringlist_get(node->dtb, node->dtb_offset, prop_name, i, &lenp);

    if(lenp < 0) {
      return lenp;   
    } else if(str == NULL) {
      return -1;
    }

    buf[i] = str;
  }

  return 0;
}
static struct nk_dev_info *dt_node_get_parent(void *state) 
{
  struct dt_node *node = (struct dt_node *)state;
  return &(node->parent->dev_info);
}
static struct nk_dev_info *dt_node_children_start(void *state) 
{
  struct dt_node *node = (struct dt_node *)state; 
  return &(node->children->dev_info);
}
static struct nk_dev_info *dt_node_children_next(void *state, struct nk_dev_info *iter) 
{
  struct dt_node *node = (struct dt_node *)state;
  if(((struct dt_node *)iter->state)->siblings == node->children) {
    return NULL;
  } else {
    return &(((struct dt_node *)iter->state)->siblings->dev_info);
  }
}

static int dt_node_read_register_blocks(void *state, void **bases, int *sizes, int *count) 
{
  struct dt_node *node = (struct dt_node *)state;

  if(*count > 1) {
    fdt_reg_t regs[*count];

    if(fdt_getreg_array(node->dtb, node->dtb_offset, regs, count)) {
      return -1;
    }

    for(int i = 0; i < *count; i++) {
      if(bases != NULL) {
        bases[i] = (void*)regs[i].address;
      }
      if(sizes != NULL) {
        sizes[i] = (int)regs[i].size;
      }
    }
  }
  else {
    fdt_reg_t reg = { .address = 0, .size = 0 };
    
    if(fdt_getreg(node->dtb, node->dtb_offset, &reg)) {
      return -1;
    }

    if(bases != NULL) {
      *bases = (void*)reg.address;
    }
    if(sizes != NULL) {
      *sizes = (int)reg.size;
    }
  }

  return 0;
}

static nk_irq_t dt_node_translate_irq_from_parent(struct dt_node *node, struct dt_node *interrupt_parent, void *raw_irq)
{
  if(!(interrupt_parent->dev_info.flags & NK_DEV_INFO_FLAG_HAS_DEVICE) || interrupt_parent->dev_info.dev == NULL)  {
    // It has an interrupt parent but it's driver hasn't been called yet
    ERROR("Interrupt Parent does not have a device!\n");
    return NK_NULL_IRQ;
  }
  struct nk_dev *dev = interrupt_parent->dev_info.dev;

  if(dev->type != NK_DEV_IRQ) {
    // It has an interrupt parent but it isn't an IRQ chip?
    ERROR("Interrupt Parent is not an IRQ device!\n");
    return NK_NULL_IRQ;
  }

  struct nk_irq_dev *irq_dev = (struct nk_irq_dev *)dev;
  DEBUG("Calling nk_irq_dev translate on device: %s\n", dev->name);

  nk_hwirq_t hwirq;
  if(nk_irq_dev_translate(irq_dev, node->dev_info.type, raw_irq, &hwirq)) {
    ERROR("Failed to translate HWIRQ!\n");
    return NK_NULL_IRQ;
  }

  nk_irq_t irq;
  if(nk_irq_dev_revmap(irq_dev, hwirq, &irq)) {
    ERROR("Failed to revmap IRQ: dev=%s, hwirq=%d\n", dev->name, hwirq);
    return NK_NULL_IRQ;
  }

  return irq;
}

static int dt_node_cache_irq_ext_data(struct dt_node *node) 
{
  int raw_len;
  uint32_t *raw = fdt_getprop(node->dtb, node->dtb_offset, "interrupts-extended", &raw_len);

  if(raw == NULL) {
    return -1;
  }

  void *original_raw = raw;
  int raw_cells = raw_len / 4;

  uint32_t phandles[raw_cells];
  int offsets[raw_cells];

  int num_irqs = 0;

  void * raw_end = raw + raw_cells;

  while(raw < raw_end) {
    phandles[num_irqs] = be32toh(*raw);
    raw += 1;
    offsets[num_irqs] = (int)((uint64_t)raw - (uint64_t)original_raw);

    struct dt_node *parent = dt_get_node_from_phandle(phandles[num_irqs]);

    if(parent == NULL) {
      ERROR("phandle in \"interrupts-extended\" does not correspond to a device tree node! (phandle = 0x%x)\n", phandles[num_irqs]);
      return -1;
    }

    int interrupt_cells_len = 0;
    uint32_t *interrupt_cells_ptr = fdt_getprop(parent->dtb, parent->dtb_offset, "#interrupt-cells", &interrupt_cells_len);

    if(interrupt_cells_ptr == NULL) {
      ERROR("Could not get \"#interrupt-cells\" property from parent node in \"interrupts-extended\"!\n");
      return -1;
    }

    raw += be32toh(*interrupt_cells_ptr);
    num_irqs += 1;
  }

  node->num_irq_extended = num_irqs;
  node->extended_parent_phandles = malloc(sizeof(uint32_t) * num_irqs);
  node->extended_offsets = malloc(sizeof(int) * num_irqs);
  for(int i = 0; i < num_irqs; i++) {
    node->extended_parent_phandles[i] = phandles[i];
    node->extended_offsets[i] = offsets[i];
  }

  node->flags |= DT_NODE_FLAG_CACHED_IRQ_EXT_DATA;
  return 0;
}

static nk_irq_t dt_node_read_interrupts_extended(struct dt_node *node, int index) 
{
  if((node->flags & DT_NODE_FLAG_CACHED_IRQ_EXT_DATA) == 0) {
    dt_node_cache_irq_ext_data(node);
  }
  if((node->flags & DT_NODE_FLAG_CACHED_IRQ_EXT_DATA) == 0) {
    return NK_NULL_IRQ;
  }
  
  // From this point on it's an error if this fails

  if(index >= node->num_irq_extended) {
    ERROR("Requested interrupt index (%u) from a device tree node with only (%u) interrupts!\n", index, node->num_irq_extended);
    return NK_NULL_IRQ;
  }

  int raw_len;
  uint32_t *raw = fdt_getprop(node->dtb, node->dtb_offset, "interrupts-extended", &raw_len);

  if(raw == NULL) {
    // Should not be possible but check anyways
    ERROR("dt_node_cache_ext_irq succeeded but \"interrupts-extended\" property is not present!\n");
    return NK_NULL_IRQ;
  }

  void *raw_irq = ((void*)raw) + node->extended_offsets[index]; 
  uint32_t parent_phandle = node->extended_parent_phandles[index];

  struct dt_node *interrupt_parent = dt_get_node_from_phandle(parent_phandle);
  if(interrupt_parent == NULL) {
    // Should not be possible but check anyways
    ERROR("Could not get interrupt parent phandle (%u)!\n", parent_phandle);
    return NK_NULL_IRQ;
  }

  return dt_node_translate_irq_from_parent(node, interrupt_parent, raw_irq);
}

static int dt_node_cache_irq_data(struct dt_node *node)
{
  int raw_irqs_len;
  void *raw_irqs = fdt_getprop(node->dtb, node->dtb_offset, "interrupts", &raw_irqs_len);

  if(raw_irqs == NULL) {
    ERROR("DTB node is missing \"interrupts\" property!\n");
    return -1;
  }

  struct dt_node *interrupt_parent = dt_node_get_interrupt_parent(node);

  if(interrupt_parent == NULL) 
  {
    // It has no interrupt parent
    ERROR("dt_node_read_interrupts: Called on Node with no Interrupt Parent!\n");
    return -1;
  }

  int interrupt_cells_len = 0;
  uint32_t *interrupt_cells_ptr = fdt_getprop(interrupt_parent->dtb, interrupt_parent->dtb_offset, "#interrupt-cells", &interrupt_cells_len);

  if(interrupt_cells_ptr == NULL) {
    ERROR("Could not read #interrupt-cells property from DTB interrupt parent node!\n");
    return -1;
  } 

  if(interrupt_cells_len < 4) {
    ERROR("#interrupt-cells property is not even a single cell! len=%u\n", interrupt_cells_len);
    return -1;
  }

  if(interrupt_cells_len != 4) {
    WARN("#interrupt-cells property was not exactly 1 cell! size=%u Continuing anyways...\n",interrupt_cells_len);
  }

  uint32_t interrupt_cells = be32toh(interrupt_cells_ptr[0]);

  uint32_t *raw_irq_cells = (uint32_t*)raw_irqs;
  int num_cells = raw_irqs_len / 4;
  int num_interrupts = num_cells / interrupt_cells;

  if(num_cells % interrupt_cells) {
    WARN("Number of cells in \"interrupts\" property is not a multiple of the \"#interrupt-cells\" property! continuing anyway (#interrupt-cells = %u, num_cells = %u\n", interrupt_cells, num_cells);
  }

  node->num_irq = num_interrupts;
  node->irq_cells = interrupt_cells;
  node->flags |= DT_NODE_FLAG_CACHED_IRQ_DATA;
  return 0;
}

static nk_irq_t dt_node_read_interrupts(struct dt_node *node, int index) 
{ 
  int raw_irqs_len;
  void *raw_irqs = fdt_getprop(node->dtb, node->dtb_offset, "interrupts", &raw_irqs_len);

  if(raw_irqs == NULL) {
    ERROR("DTB node is missing \"interrupts\" property!\n");
    return NK_NULL_IRQ;
  }

  struct dt_node *interrupt_parent = dt_node_get_interrupt_parent(node);

  if(interrupt_parent == NULL) 
  {
    // It has no interrupt parent
    ERROR("dt_node_read_interrupts: Called on Node with no Interrupt Parent!\n");
    return NK_NULL_IRQ;
  }

  if((node->flags & DT_NODE_FLAG_CACHED_IRQ_DATA) == 0) {
    if(dt_node_cache_irq_data(node)) {
      ERROR("Could not get IRQ num or cells!\n");
      return NK_NULL_IRQ;
    }
  }

  if(index >= node->num_irq) {
    ERROR("Invalid interrupt index (%u) is being read from device tree \"interrupts\" property with only (%u) interrupts!\n", index, node->num_irq);
    return NK_NULL_IRQ;
  }

  void *raw_irq = ((uint32_t*)raw_irqs) + (node->irq_cells * index);

  return dt_node_translate_irq_from_parent(node, interrupt_parent, raw_irq);
}

static nk_irq_t dt_node_read_irq(void *state, int index) 
{ 
  struct dt_node *node = (struct dt_node *)state;
  nk_irq_t irq = dt_node_read_interrupts_extended(node, index);
  if(irq == NK_NULL_IRQ) {
    irq = dt_node_read_interrupts(node, index);
  }
  return irq;
}

static int dt_node_num_interrupts_extended(struct dt_node *node, int *num) 
{
  if((node->flags & DT_NODE_FLAG_CACHED_IRQ_EXT_DATA) == 0) 
  {
    dt_node_cache_irq_ext_data(node);
  }
  if((node->flags & DT_NODE_FLAG_CACHED_IRQ_EXT_DATA) == 0) 
  {
    return -1;
  }
  *num = node->num_irq_extended;
  return 0; 
}

static int dt_node_num_interrupts(struct dt_node *node, int *num) 
{
  int raw_irqs_len;
  void *raw_irqs = fdt_getprop(node->dtb, node->dtb_offset, "interrupts", &raw_irqs_len);

  if(raw_irqs == NULL) {
    ERROR("DTB node is missing \"interrupts\" property!\n");
    return -1;
  }

  struct dt_node *interrupt_parent = dt_node_get_interrupt_parent(node);

  if(interrupt_parent == NULL) 
  {
    // It has no interrupt parent
    ERROR("Called on Node with no Interrupt Parent!\n");
    return -1;
  }

  int interrupt_cells_len;
  uint32_t *interrupt_cells_ptr = fdt_getprop(interrupt_parent->dtb, interrupt_parent->dtb_offset, "#interrupt-cells", &interrupt_cells_len);

  if(interrupt_cells_ptr == NULL) {
    ERROR("Could not read #interrupt-cells property from DTB interrupt parent node!\n");
    return -1;
  } 

  if(interrupt_cells_len < 4) {
    ERROR("#interrupt-cells property is less than one cell wide! len=%u\n", interrupt_cells_len);
    return -1;
  }

  if(interrupt_cells_len > 4) {
    WARN("#interrupt-cells property is more than a single cells wide! len=%u (continuing anyways)...\n", interrupt_cells_len);
  }

  uint32_t interrupt_cells = be32toh(*interrupt_cells_ptr);

  *num = (raw_irqs_len>>2) / interrupt_cells;
  return 0;
}

static int dt_node_num_irq(void *state) 
{
  struct dt_node *node = (struct dt_node*)state;
  int num;

  if(dt_node_num_interrupts_extended(node, &num)) {
    // interrupts-extended failed, try interrupts
    if(dt_node_num_interrupts(node, &num)) {
      return 0;
    }
  }

  return num;
}

static struct nk_dev_info_int dt_node_int = {
  .get_name = dt_node_get_name,
  .has_property = dt_node_has_property,
  .read_int_array = dt_node_read_int_array,
  .read_string_array = dt_node_read_string_array,
  .read_irq = dt_node_read_irq,
  .num_irq = dt_node_num_irq,
  .read_register_blocks = dt_node_read_register_blocks,
  .get_parent = dt_node_get_parent,
  .children_start = dt_node_children_start,
  .children_next = dt_node_children_next
};

static int fdt_initialize_dt_node(void *fdt, int node_offset, struct dt_node *node, struct dt_node *node_parent) 
{
  node->parent = node_parent;
  node->dev_info.interface = &dt_node_int;

  node->dtb = fdt;
  node->dtb_offset = node_offset;

  int phandle = fdt_get_phandle(fdt, node_offset);
  if(phandle != 0 && phandle != -1) {
    node->phandle = phandle;
    DEBUG("fdt_intiailize_dt_node: Set explicit phandle = %u\n",node->phandle);
  } else {
    node->phandle = 0;
  }

  int lenp;
  uint32_t *interrupt_parent_prop = fdt_getprop(fdt, node_offset, "interrupt-parent", &lenp);

  node->flags |= DT_NODE_FLAG_INT_PARENT_PHANDLE;
  if(interrupt_parent_prop != NULL && lenp == 4) {
    node->interrupt_parent_phandle = be32toh(*interrupt_parent_prop);
    DEBUG("fdt_initialize_dt_node: read interrupt parent: phandle = %u\n", node->interrupt_parent_phandle);
  } else if(node_parent != NULL) {
    if(node_parent->flags & DT_NODE_FLAG_INT_PARENT_PHANDLE) {
      node->interrupt_parent_phandle = node_parent->interrupt_parent_phandle;
      DEBUG("fdt_initialize_dt_node: inheriting interrupt parent: phandle = %u\n", node->interrupt_parent_phandle);
    } else {
      node->interrupt_parent = node_parent->interrupt_parent;
      DEBUG("fdt_initiailize_dt_node: inheriting interrupt parent: node_ptr = %p\n", node->interrupt_parent);
    }
  } else {
    DEBUG("fdt_initialize_dt_node: setting node interrupt parent phandle to 0\n");
    node->interrupt_parent_phandle = 0;
  }

  node->dev_info.type = NK_DEV_INFO_OF;
  node->dev_info.state = node;
  DEBUG("fdt_initialize_dt_node: finished\n");

  return 0;
}

static int fdt_unflatten_tree_recursive(void *fdt, int node_offset, struct dt_node *node, struct dt_node *node_parent) 
{
  if(fdt_initialize_dt_node(fdt, node_offset, node, node_parent)) {
    ERROR("Failed to initialize dt_node!\n");
    return -1;
  }

  int subnode_offset = fdt_first_subnode(fdt, node_offset);
  node->child_count = 0;

  struct dt_node *list_end = NULL;
  while(subnode_offset != -FDT_ERR_NOTFOUND) 
  { 
    struct dt_node *subnode = alloc_dt_node();

    if(subnode == NULL) {
      return -1;
    }

    if(fdt_unflatten_tree_recursive(fdt, subnode_offset, subnode, node)) {
      return -1;
    }

    if(list_end == NULL) {
      node->children = subnode;
      list_end = subnode;
    } else {
      list_end->siblings = subnode;
      list_end = subnode;
    }
    node->child_count += 1;

    subnode_offset = fdt_next_subnode(fdt, subnode_offset);
  }

  if(list_end != NULL) 
  {
    list_end->siblings = node->children;
  }

  return 0;
}

int fdt_unflatten_tree(void *fdt, struct dt_node **root)
{
  *root = alloc_dt_node();

  if(root == NULL) {
    return -1;
  }

  return fdt_unflatten_tree_recursive(fdt, 0, *root, NULL);
}

static struct dt_node *global_dt_root;
int of_init(void *fdt) 
{
  if(fdt_unflatten_tree(fdt, &global_dt_root)) {
    return -1;
  }

  DEBUG("inited");

  return 0;
}

int of_cleanup(void)
{
  if(free_dt_node_list()) {
    return -1;
  }

  return 0;
}

static int of_node_matches(struct dt_node *node, struct of_dev_id *id) {
  if(id == NULL || node == NULL) 
  {
    return 0;
  }

  if(id->name != NULL && !fdt_nodename_eq(node->dtb, node->dtb_offset, id->name, strlen(id->name))) {
    return 0;
  } 

  if(id->compatible != NULL && fdt_node_check_compatible(node->dtb, node->dtb_offset, id->compatible)) {
    return 0;
  }

  if(id->properties != NULL) {
    DEBUG("passed compatible, checking properties\n");
    for(int i = 0; i < id->properties->count; i++) { 
      DEBUG("checking property %u: %s\n", i, id->properties->names[i]);
      if(fdt_get_property(node->dtb, node->dtb_offset, id->properties->names[i], NULL) == NULL) {
        DEBUG("failed property %u\n", i);
        return 0;
      }
      DEBUG("passed property %u\n", i);
    }
  }
  DEBUG("passed properties!\n");
  return 1;
}

int of_for_each_match(const struct of_dev_match *match, int(*callback)(struct nk_dev_info *info)) 
{
  DEBUG("Beginning for_each_match\n");
  struct dt_node *node = dt_node_list;

  int fail_count = 0;
  int num_matches = 0;

  if(match->num_ids == 0) {
    return 0;
  }
  
  while(node != NULL) {
    if(match->max_num_matches > 0 && num_matches >= match->max_num_matches) {
      break;
    }
    for(int id_index = 0; id_index < match->num_ids; id_index++) { 
      if(of_node_matches(node, match->ids + id_index)) {
        DEBUG("\tMATCH\n");
        num_matches += 1;
        if(callback) {
          if(callback(&node->dev_info)) {
            fail_count += 1;
          }
        } else {
          fail_count += 1;
        }
        break;
      } else {
        DEBUG("failed match: compat = %s\n", match->ids[id_index].compatible);
      }
    }
    node = node->next_node;
  }

  return fail_count;
}

int of_for_each_subnode_match(const struct of_dev_match *match, int(*callback)(struct nk_dev_info *info), struct nk_dev_info *root) 
{
  if(root->type != NK_DEV_INFO_OF) {
    return -1;
  }

  struct dt_node *root_node = (struct dt_node*)root->state;

  struct dt_node *node = root_node->children;
  int num_children_left = root_node->child_count;

  int fail_count = 0;
  int num_matches = 0;

  if(match->num_ids == 0) {
    return 0;
  }
  
  while(node != NULL && num_children_left > 0) 
  {
    num_children_left -= 1;

    if(match->max_num_matches > 0 && num_matches >= match->max_num_matches) {
      break;
    }
    for(int id_index = 0; id_index < match->num_ids; id_index++) { 
      if(of_node_matches(node, match->ids + id_index)) {
        num_matches += 1;
        if(callback) {
          if(callback(&node->dev_info)) {
            fail_count += 1;
          }
        } else {
          fail_count += 1;
        }
        break;
      }
    }
    node = node->siblings;
  }

  return fail_count;
}

