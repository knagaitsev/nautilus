/* 
 * This file is part of the Nautilus AeroKernel developed
 * by the Hobbes and V3VEE Projects with funding from the 
 * United States National  Science Foundation and the Department of Energy.  
 *
 * The V3VEE Project is a joint project between Northwestern University
 * and the University of New Mexico.  The Hobbes Project is a collaboration
 * led by Sandia National Laboratories that includes several national 
 * laboratories and universities. You can find out more at:
 * http://www.v3vee.org  and
 * http://xstack.sandia.gov/hobbes
 *
 * Copyright (c) 2016, Peter Dinda <pdinda@northwestern.edu>
 * Copyright (c) 2015, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author: Peter Dinda <pdinda@northwestern.edu>
 *
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#ifndef __DEV
#define __DEV

#include <nautilus/list.h>

#define DEV_NAME_LEN 32
typedef enum {
    NK_DEV_GENERIC, 
    NK_DEV_TIMER,
    NK_DEV_BUS,
    NK_DEV_CHAR, 
    NK_DEV_BLK, 
    NK_DEV_NET,
    NK_DEV_GRAPHICS,
    NK_DEV_SOUND,
    NK_DEV_IRQ,
    NK_DEV_GPIO,
} nk_dev_type_t ; 


// this is the abstract base class for device interfaces
// it should be the first member of any type of specific device interface
struct nk_dev_int {
    int (*open)(void *state);
    int (*close)(void *state);
};

typedef struct nk_wait_queue nk_wait_queue_t;

#define NK_DEV_FLAG_NO_WAIT (1<<0) // you cannot wait on this device (nk_dev_wait will immediately return regardless of context)

// this is the class for devices.  It should be the first
// member of any specific type of device
struct nk_dev {
    char name[DEV_NAME_LEN];
    nk_dev_type_t type; 
    uint64_t      flags;
    struct list_head dev_list_node;
    
    void *state; // driver state
    
    struct nk_dev_int *interface;
    
    nk_wait_queue_t *waiting_threads;
};

// Not all request types apply to all device types
// Not all devices support the request types valid
// for a specific device type
typedef enum {NK_DEV_REQ_BLOCKING, NK_DEV_REQ_NONBLOCKING, NK_DEV_REQ_CALLBACK} nk_dev_request_type_t;

int nk_dev_init();
int nk_dev_deinit();

struct nk_dev *nk_dev_register(char *name, nk_dev_type_t type, uint64_t flags, struct nk_dev_int *inter, void *state);
int nk_dev_register_pre_allocated(struct nk_dev *d, char *name, nk_dev_type_t type, uint64_t flags, struct nk_dev_int *inter, void *state);
int            nk_dev_unregister(struct nk_dev *);
int            nk_dev_unregister_pre_allocated(struct nk_dev *);

struct nk_dev *nk_dev_find(char *name);

void nk_dev_wait(struct nk_dev*, int (*cond_check)(void *state), void *state);
void nk_dev_signal(struct nk_dev *);

void nk_dev_dump_devices();

/*
 * Device Info
 *
 * Abstraction for the Device tree and ACPI tables based on Linux's fwnode structures
 */

struct nk_dev_info_int {
  char*(*get_name)(void *state);
  
  int(*has_property)(void *state, const char *prop_name);
  int(*read_int_array)(void *state, const char *prop_name, int elem_size, void *buf, int *buf_cnt);
  int(*read_string_array)(void *state, const char *prop_name, char **buf, int *buf_cnt);

  int(*read_register_blocks)(void *state, void **bases, int *sizes, int *count);
  int(*read_irqs)(void *state, nk_irq_t *irqs_buf, int *irqs_buf_count);

  struct nk_dev_info *(*get_parent)(void *state);
  struct nk_dev_info *(*get_child_named)(void *state, const char *child_name);
  struct nk_dev_info *(*children_start)(void *state);
  struct nk_dev_info *(*children_next)(void *state, const struct nk_dev_info *iter);
};

typedef enum nk_dev_info_type {

  NK_DEV_INFO_OF,
  NK_DEV_INFO_ACPI

} nk_dev_info_type_t;

#define NK_DEV_INFO_FLAG_HAS_DEVICE (1<<0)

struct nk_dev_info {

  struct nk_dev *dev;
  struct nk_dev_info_int *interface;

  void *state;

  nk_dev_info_type_t type;
  int flags;
};

inline static int nk_dev_info_set_device(struct nk_dev_info *info, struct nk_dev *device) 
{
  if(info) {
    info->dev = device;
    info->flags |= NK_DEV_INFO_FLAG_HAS_DEVICE;
    return 0;
  } else {
    return -1;
  }
}

inline static struct nk_dev * nk_dev_info_get_device(struct nk_dev_info *info) 
{
  if(info && info->dev && (info->flags & NK_DEV_INFO_FLAG_HAS_DEVICE)) {
    return info->dev;
  } else {
    return NULL;
  }
}

inline static char * nk_dev_info_get_name(const struct nk_dev_info *info)
{
  if(info && info->interface && info->interface->get_name) {
    return info->interface->get_name(info->state);
  } else {
    return NULL;
  }
}

inline static int nk_dev_info_has_property(const struct nk_dev_info *info, const char *prop_name) 
{
  if(info && info->interface && info->interface->has_property) {
    return info->interface->has_property(info->state, prop_name);
  } else {
    return -1;
  }
}

inline static int nk_dev_info_read_int_array(const struct nk_dev_info *info, const char *prop_name, int elem_size, void *buf, int *cnt) 
{
  if(info && info->interface && info->interface->read_int_array) {
    return info->interface->read_int_array(info->state, prop_name, elem_size, buf, cnt);
  } else {
    return -1;
  }
}
inline static int nk_dev_info_read_int(const struct nk_dev_info *info, const char *prop_name, int size, void *val) 
{
  int read = 1;
  int ret = nk_dev_info_read_int_array(info->state, prop_name, size, val, &read); 
  if(read != 1) {
    ret = -1;
  }
  return ret;
}

#define declare_read_int(BITS) \
  _Static_assert(BITS%8 == 0, "nk_dev_info: declare_read_int needs BITS to be a multiple of 8"); \
  inline static int nk_dev_info_read_u ## BITS (const struct nk_dev_info *info, const char *propname, uint ## BITS ## _t *val) { \
    return nk_dev_info_read_int(info, propname, BITS/8, val); \
  }
#define declare_read_int_array(BITS) \
  _Static_assert(BITS%8 == 0, "nk_dev_info: declare_read_int_array needs BITS to be a multiple of 8"); \
  inline static int nk_dev_info_read_u ## BITS ## _array(const struct nk_dev_info *info, const char *propname, uint ## BITS ## _t *buf, int *cnt) { \
    return nk_dev_info_read_int_array(info, propname, BITS/8, buf, cnt); \
  }

declare_read_int(8) //nk_dev_info_read_u8(...);
declare_read_int_array(8) //nk_dev_info_read_u8_array(...);

declare_read_int(16) //nk_dev_info_read_u16(...);
declare_read_int_array(16) //nk_dev_info_read_u16_array(...);

declare_read_int(32) //nk_dev_info_read_u32(...);
declare_read_int_array(32) //nk_dev_info_read_u32_array(...);

declare_read_int(64) //nk_dev_info_read_u64(...);
declare_read_int_array(64) //nk_dev_info_read_u64_array(...);

inline static int nk_dev_info_read_string_array(const struct nk_dev_info *info, const char *prop_name, char **buf, int *cnt) 
{
  if(info && info->interface && info->interface->read_string_array) {
    return info->interface->read_string_array(info->state, prop_name, buf, cnt);
  } else {
    return -1;
  }
}
inline static int nk_dev_info_read_string(const struct nk_dev_info *info, const char *prop_name, char **str)
{
  int read = 1;
  int ret = nk_dev_info_read_string_array(info->state, prop_name, str, &read);
  if(read != 1) {
    ret = -1;
  }
  return ret;
}

inline static int nk_dev_info_read_irqs(const struct nk_dev_info *info, nk_irq_t *irq_buf, int *irq_count) 
{
  if(info && info->interface && info->interface->read_irqs) {
    return info->interface->read_irqs(info->state, irq_buf, irq_count);
  } else {
    return -1;
  }
}
inline static int nk_dev_info_read_irqs_exact(const struct nk_dev_info *info, nk_irq_t *irq_buf, int irq_count) 
{
  int mod_count = irq_count;
  int ret = nk_dev_info_read_irqs(info->state, irq_buf, &mod_count);
  if(mod_count != irq_count) {
    ret = -1;
  }
  return ret;
}
inline static int nk_dev_info_read_irq(const struct nk_dev_info *info, nk_irq_t *irq) {
  return nk_dev_info_read_irqs_exact(info, irq, 1);
}

inline static int nk_dev_info_read_register_blocks(const struct nk_dev_info *info, void** bases, int *sizes, int *block_count) 
{
  if(info && info->interface && info->interface->read_register_blocks) {
    return info->interface->read_register_blocks(info->state, bases, sizes, block_count);
  } else {
    return -1;
  }
}
inline static int nk_dev_info_read_register_blocks_exact(const struct nk_dev_info *info, void** bases, int *sizes, int block_count) 
{
  int mod_count = block_count;
  int ret = nk_dev_info_read_register_blocks(info, bases, sizes, &mod_count);
  if(mod_count != block_count) {
    ret = -1;
  }
  return ret;
}
inline static int nk_dev_info_read_register_block(const struct nk_dev_info *info, void **base, int *size) {
  return nk_dev_info_read_register_blocks_exact(info, base, size, 1);
}

inline static struct nk_dev_info *nk_dev_info_get_parent(const struct nk_dev_info *info) 
{
  if(info && info->interface && info->interface->get_parent) {
    return info->interface->get_parent(info->state);
  } else {
    return NULL;
  }
}

inline static struct nk_dev_info *nk_dev_info_children_start(const struct nk_dev_info *info) 
{
  if(info && info->interface && info->interface->children_start) {
    return info->interface->children_start(info->state);
  } else {
    return NULL;
  }
}
inline static struct nk_dev_info *nk_dev_info_children_next(const struct nk_dev_info *info, const struct nk_dev_info *iter) 
{
  if(info && info->interface && info->interface->children_next) {
    return info->interface->children_next(info->state, iter);
  } else {
    return NULL;
  }
}

/*
 * Device Numbering
 *
 * Atomic functions to consistently name devices which might share a name
 */

// Get N for a new "serial<N>" device
uint32_t nk_dev_get_serial_device_number(void);

#endif
