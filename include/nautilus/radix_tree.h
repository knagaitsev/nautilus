#ifndef __RADIX_TREE_H__
#define __RADIX_TREE_H__

#include<nautilus/naut_types.h>

struct nk_radix_tree_layer;

// Synchronization is the responsibility of the user
struct nk_radix_tree 
{
  int height;
  struct nk_radix_tree_layer *root;
};

struct nk_radix_tree_iterator 
{
  unsigned long index;
  int height;
  struct nk_radix_tree_layer *layer;
};

void *radix_tree_get(struct nk_radix_tree *tree, unsigned long index);
void *radix_tree_remove(struct nk_radix_tree *tree, unsigned long index);
int nk_radix_tree_insert(struct nk_radix_tree *tree, unsigned long index, void *data);

int nk_radix_tree_iterator_start(struct nk_radix_tree *tree, struct nk_radix_tree_iterator *iter);
int nk_radix_tree_iterator_next(struct nk_radix_tree_iterator *iter);
int nk_radix_tree_iterator_check_end(struct nk_radix_tree_iterator *iter);

unsigned long nk_radix_tree_iterator_index(struct nk_radix_tree_iterator *iter);
void *nk_radix_tree_iterator_value(struct nk_radix_tree_iterator *iter);

#endif
