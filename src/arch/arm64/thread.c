
#include<nautilus/thread.h>

// When we enter this function from assembly,
// we should be effectively threadless
void __thread_fork_threadless(struct nk_regs *regs) 
{
  // TODO: implement forking
  // KJH - I have it set up where we enter this function with our thread saved correctly, and nk_regs is a pointer to it's 
  // registers on it's stack, but we need to actually create the child thread here.
  // For now it just fails "correctly"
  regs->x0 = NK_BAD_THREAD_ID;
  return;
}

