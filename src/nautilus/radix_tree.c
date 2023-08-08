
#include<nautilus/radix_tree.h>
#include<nautilus/nautilus.h>

#define NAUT_CONFIG_DEBUG_RADIX_TREE

#ifndef NAUT_CONFIG_DEBUG_RADIX_TREE
#undef DEBUG_PRINT
#define DEBUG_PRINT(fmt, args...)
#endif

#define PRINT(fmt, args...) INFO_PRINT("radix_tree: " fmt, ##args)
#define DEBUG(fmt, args...) DEBUG_PRINT("radix_tree: " fmt, ##args)
#define ERROR(fmt, args...) ERROR_PRINT("radix_tree: " fmt, ##args)
#define WARN(fmt, args...) WARN_PRINT("radix_tree: " fmt, ##args)

#define NK_RADIX_TREE_BITS_PER_LAYER 6
#define NK_RADIX_TREE_LAYER_INDEX_MASK ((1<<NK_RADIX_TREE_BITS_PER_LAYER)-1)
#define NK_RADIX_TREE_ELEM_PER_LAYER (1<<NK_RADIX_TREE_BITS_PER_LAYER)

struct nk_radix_tree_layer {
  struct nk_radix_tree_layer *parent;
  void *ptrs[NK_RADIX_TREE_ELEM_PER_LAYER];
};

static struct nk_radix_tree_layer *nk_radix_tree_allocate_layer(void)
{
  size_t size = sizeof(struct nk_radix_tree_layer);
  struct nk_radix_tree_layer *layer = (struct nk_radix_tree_layer*)malloc(size);

  if(layer == NULL) {
    return layer;
  } else {
    memset(layer, 0, size);
    return layer;
  }
}

static inline int nk_radix_tree_required_layers(unsigned long index)
{
  return (((sizeof(index)*8)-__builtin_clz(index))/8) + 1;
}

static inline int nk_radix_tree_add_layer(struct nk_radix_tree *tree) 
{
  struct nk_radix_tree_layer *new_root = nk_radix_tree_allocate_layer();
  if(new_root == NULL) {
    return -1;
  }
  new_root->ptrs[0] = tree->root;
  tree->root->parent = new_root;
  tree->root = new_root;
  tree->height += 1;

  return 0;
}

static inline void **nk_radix_tree_get_slot(struct nk_radix_tree *tree, unsigned long index) 
{
  if(tree == NULL) {
    return tree;
  }

  struct nk_radix_tree_layer *layer = tree->root;

  for(int i = tree->height-1; i > 0; i--) 
  {
    if(layer == NULL) {
      return layer;
    }
    int layer_index = (index >> (NK_RADIX_TREE_BITS_PER_LAYER * i)) & NK_RADIX_TREE_LAYER_INDEX_MASK;
    if(i != 0) {
      layer = (struct nk_radix_tree_layer *)layer->ptrs[layer_index];
    } 
  }

  if(layer == NULL) {
    return layer;
  }
  
  return layer->ptrs + (index & NK_RADIX_TREE_LAYER_INDEX_MASK);
}

void *nk_radix_tree_get(struct nk_radix_tree *tree, unsigned long index)
{
  void **slot = nk_radix_tree_get_slot(tree, index);
  if(slot != NULL) {
    return *slot;
  } else {
    return (void*)slot;
  }
};

void *nk_radix_tree_remove(struct nk_radix_tree *tree, unsigned long index)
{
  void **slot = nk_radix_tree_get_slot(tree, index);
  if(slot != NULL) {
    void *val = *slot;
    *slot = NULL;
    return val;
  } else {
    return (void*)slot;
  }
};

int nk_radix_tree_insert(struct nk_radix_tree *tree, unsigned long index, void *data) 
{
  if(tree == NULL) {
    return -1;
  }

  int index_required_layers = nk_radix_tree_required_layers(index);

  while(tree->height < index_required_layers) {
    if(nk_radix_tree_add_layer(tree)) {
      ERROR("Failed to add layer to radix tree!\n");
      return -1;
    }
  }

  struct nk_radix_tree_layer *prev_layer = NULL;
  struct nk_radix_tree_layer **layer_slot = &tree->root;
  struct nk_radix_tree_layer *layer = tree->root;

  for(int i = tree->height-1; i > 0; i--) 
  {
    if(layer == NULL) {
      layer = nk_radix_tree_allocate_layer();
      if(layer == NULL) {
        ERROR("Failed to allocate layer for radix tree!\n");
        return -1;
      }
      layer->parent = prev_layer;
      *layer_slot = layer;
    }
    int layer_index = (index >> (NK_RADIX_TREE_BITS_PER_LAYER * i)) & NK_RADIX_TREE_LAYER_INDEX_MASK;
    prev_layer = layer;
    layer_slot = (struct nk_radix_tree_layer**)(layer->ptrs + layer_index);
    layer = *layer_slot;
  } 

  if(layer == NULL) {
    layer = nk_radix_tree_allocate_layer();
    if(layer == NULL) {
      ERROR("Failed to allocate layer for radix tree!\n");
      return -1;
    }
    layer->parent = prev_layer;
    *layer_slot = layer;
  }

  layer->ptrs[index & NK_RADIX_TREE_LAYER_INDEX_MASK] = data;
  return 0;
};

static inline void *nk_radix_tree_iterator_deref(struct nk_radix_tree_iterator *iter) 
{
  return iter->layer->ptrs[(iter->index>>(iter->height*NK_RADIX_TREE_BITS_PER_LAYER))
    & NK_RADIX_TREE_LAYER_INDEX_MASK];
}
static inline int nk_radix_tree_iterator_up_one(struct nk_radix_tree_iterator *iter) 
{
  if(iter->layer->parent == NULL) {
    return -1;
  }

  iter->height++;
  iter->layer = iter->layer->parent;

  return 0;
}
static inline int nk_radix_tree_iterator_down(struct nk_radix_tree_iterator *iter) 
{
  while(iter->height > 0) {
    void *next_layer = nk_radix_tree_iterator_deref(iter);
    if(next_layer == NULL) {
      break;
    }

    iter->layer = next_layer;
    iter->height--;
  }
  return 0;
}
static inline int nk_radix_tree_iterator_step(struct nk_radix_tree_iterator *iter) 
{
  void *deref = NULL;
  do {
    int inner_index = (iter->index>>(NK_RADIX_TREE_BITS_PER_LAYER*iter->height))
      & NK_RADIX_TREE_LAYER_INDEX_MASK;

    if(inner_index == NK_RADIX_TREE_LAYER_INDEX_MASK) {
      if(nk_radix_tree_iterator_up_one(iter)) {
        return -1;
      } 
      deref = NULL;
      continue;
    }
    else 
    {
      iter->index += 1<<(NK_RADIX_TREE_BITS_PER_LAYER*iter->height);
      if(iter->height > 0) {
        nk_radix_tree_iterator_down(iter);
        deref = nk_radix_tree_iterator_deref(iter);
      } else {
        deref = NULL;
      }
    } 
  } while(deref == NULL || iter->height > 0);

  return 0;
}

int nk_radix_tree_iterator_start(struct nk_radix_tree *tree, struct nk_radix_tree_iterator *iter) 
{
  iter->index = 0;
  iter->layer = tree->root;
  iter->height = tree->height;
 
  return 0;
}
int nk_radix_tree_iterator_next(struct nk_radix_tree_iterator *iter) 
{
  if(nk_radix_tree_iterator_step(iter)) {
    // Do nothing, we hit the end
  }
  return 0;
}
int nk_radix_tree_iterator_check_end(struct nk_radix_tree_iterator *iter) 
{
  return iter->height > 0;
}

unsigned long nk_radix_tree_iterator_index(struct nk_radix_tree_iterator *iter) 
{
  if(iter->height > 0) {
    return -1;
  }

  return iter->index;
}
void *nk_radix_tree_iterator_value(struct nk_radix_tree_iterator *iter) 
{
  if(iter->height > 0) {
    return NULL;
  }

  return nk_radix_tree_iterator_deref(iter);
}

