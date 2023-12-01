
#include <nautilus/nautilus.h>
#include <nautilus/shell.h>

#define FP_TEST_SAVE(r, v) \
  do {\
    asm volatile ("FMOV " #r ", " #v); \
  } while(0)
#define FP_TEST_CHECK(r, s) \
  do {\
    asm volatile ("FCVTPU %0, " #r : "=r" (s)); \
  } while(0)


static volatile int wait_to_be_annoying = 1;
static volatile int has_been_annoying = 0;

static void
annoying_func(void *in, void **out)
{
  while(wait_to_be_annoying) {
    // Wait for the other thread to set up the fp registers
  }

  FP_TEST_SAVE(d0, 2.0);
  FP_TEST_SAVE(d1, 2.0);
  FP_TEST_SAVE(d2, 2.0);
  FP_TEST_SAVE(d3, 2.0);
  FP_TEST_SAVE(d4, 2.0);
  FP_TEST_SAVE(d5, 2.0);
  FP_TEST_SAVE(d6, 2.0);
  FP_TEST_SAVE(d7, 2.0);
  FP_TEST_SAVE(d8, 2.0);
  FP_TEST_SAVE(d9, 2.0);
  FP_TEST_SAVE(d10, 2.0);
  FP_TEST_SAVE(d11, 2.0);
  FP_TEST_SAVE(d12, 2.0);
  FP_TEST_SAVE(d13, 2.0);
  FP_TEST_SAVE(d14, 2.0);
  FP_TEST_SAVE(d15, 2.0);
  FP_TEST_SAVE(d16, 2.0);
  FP_TEST_SAVE(d17, 2.0);
  FP_TEST_SAVE(d18, 2.0);
  FP_TEST_SAVE(d19, 2.0);
  FP_TEST_SAVE(d20, 2.0);
  FP_TEST_SAVE(d21, 2.0);
  FP_TEST_SAVE(d22, 2.0);
  FP_TEST_SAVE(d23, 2.0);
  FP_TEST_SAVE(d24, 2.0);
  FP_TEST_SAVE(d25, 2.0);
  FP_TEST_SAVE(d26, 2.0);
  FP_TEST_SAVE(d27, 2.0);
  FP_TEST_SAVE(d28, 2.0);
  FP_TEST_SAVE(d29, 2.0);
  FP_TEST_SAVE(d30, 2.0);
  FP_TEST_SAVE(d31, 2.0);
 
  has_been_annoying = 1;
}

static void test_fp(void) {
 
  wait_to_be_annoying = 1;
  has_been_annoying = 0;
  if(nk_thread_start(annoying_func, NULL, 0, 0, 4096, NULL, my_cpu_id())) {
    ERROR_PRINT("Failed to launch annoying thread!\n");
  }

  FP_TEST_SAVE(d0, 0);
  FP_TEST_SAVE(d1, 0);
  FP_TEST_SAVE(d2, 0);
  FP_TEST_SAVE(d3, 0);
  FP_TEST_SAVE(d4, 0);
  FP_TEST_SAVE(d5, 0);
  FP_TEST_SAVE(d6, 0);
  FP_TEST_SAVE(d7, 0);
  FP_TEST_SAVE(d8, 0);
  FP_TEST_SAVE(d9, 0);
  FP_TEST_SAVE(d10, 0);
  FP_TEST_SAVE(d11, 0);
  FP_TEST_SAVE(d12, 0);
  FP_TEST_SAVE(d13, 0);
  FP_TEST_SAVE(d14, 0);
  FP_TEST_SAVE(d15, 0);
  FP_TEST_SAVE(d16, 0);
  FP_TEST_SAVE(d17, 0);
  FP_TEST_SAVE(d18, 0);
  FP_TEST_SAVE(d19, 0);
  FP_TEST_SAVE(d20, 0);
  FP_TEST_SAVE(d21, 0);
  FP_TEST_SAVE(d22, 0);
  FP_TEST_SAVE(d23, 0);
  FP_TEST_SAVE(d24, 0);
  FP_TEST_SAVE(d25, 0);
  FP_TEST_SAVE(d26, 0);
  FP_TEST_SAVE(d27, 0);
  FP_TEST_SAVE(d28, 0);
  FP_TEST_SAVE(d29, 0);
  FP_TEST_SAVE(d30, 0);
  FP_TEST_SAVE(d31, 0);

  // Let the other thread off the leash
  wait_to_be_annoying = 0;

  while(!has_been_annoying) {
    // Wait for the other thread to do it's work
  }

  // See if we were affected

  uint64_t zeros[32];

  FP_TEST_CHECK(d0, zeros[0]);
  FP_TEST_CHECK(d1, zeros[1]);
  FP_TEST_CHECK(d2, zeros[2]);
  FP_TEST_CHECK(d3, zeros[3]);
  FP_TEST_CHECK(d4, zeros[4]);
  FP_TEST_CHECK(d5, zeros[5]);
  FP_TEST_CHECK(d6, zeros[6]);
  FP_TEST_CHECK(d7, zeros[7]);
  FP_TEST_CHECK(d8, zeros[8]);
  FP_TEST_CHECK(d9, zeros[9]);
  FP_TEST_CHECK(d10, zeros[10]);
  FP_TEST_CHECK(d11, zeros[11]);
  FP_TEST_CHECK(d12, zeros[12]);
  FP_TEST_CHECK(d13, zeros[13]);
  FP_TEST_CHECK(d14, zeros[14]);
  FP_TEST_CHECK(d15, zeros[15]);
  FP_TEST_CHECK(d16, zeros[16]);
  FP_TEST_CHECK(d17, zeros[17]);
  FP_TEST_CHECK(d18, zeros[18]);
  FP_TEST_CHECK(d19, zeros[19]);
  FP_TEST_CHECK(d20, zeros[20]);
  FP_TEST_CHECK(d21, zeros[21]);
  FP_TEST_CHECK(d22, zeros[22]);
  FP_TEST_CHECK(d23, zeros[23]);
  FP_TEST_CHECK(d24, zeros[24]);
  FP_TEST_CHECK(d25, zeros[25]);
  FP_TEST_CHECK(d26, zeros[26]);
  FP_TEST_CHECK(d27, zeros[27]);
  FP_TEST_CHECK(d28, zeros[28]);
  FP_TEST_CHECK(d29, zeros[29]);
  FP_TEST_CHECK(d30, zeros[30]);
  FP_TEST_CHECK(d31, zeros[31]);

  for(unsigned int i = 0; i < 32; i++) {
    printk("d%u = %p\n", i, zeros[i]);
  }
}

static int
handle_fptest (char * buf, void * priv)
{
    test_fp();
    return 0;
}

static struct shell_cmd_impl fp_test_impl = {
    .cmd      = "fptest",
    .help_str = "fptest",
    .handler  = handle_fptest,
};
nk_register_shell_cmd(fp_test_impl);

