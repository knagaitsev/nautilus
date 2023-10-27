#ifndef __ARM64_UNIMPL_H__
#define __ARM64_UNIMPL_H__

#include<nautilus/printk.h>

#define _stringify(x) #x
#define _tostring(x) _stringify(x)

#define ARM64_ERR_UNIMPL \
  printk("\nERROR: Unimplemented section of code reached!\n" \
             "LINE: " _tostring(__LINE__) \
             " FILE: " __FILE__ \
             "\n")

#endif
