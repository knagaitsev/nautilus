
#include<nautilus/naut_types.h>
#include<nautilus/setjmp.h>

#include<arch/arm64/unimpl.h>

int setjmp(jmp_buf env) {
  ARM64_ERR_UNIMPL;
  return -1;
}

void longjmp(jmp_buf env, int val) {
  ARM64_ERR_UNIMPL;
}

