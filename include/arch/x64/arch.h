#pragma once

#define MAX_IRQ_NUM 256
#define MAX_IVEC_NUM 256

inline int arch_atomics_enabled() {
  return 1;
}

