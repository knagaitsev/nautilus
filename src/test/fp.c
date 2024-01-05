
#include <nautilus/nautilus.h>
#include <nautilus/shell.h>

#if defined(NAUT_CONFIG_NPB_NAS_TEST)
#include "./NPB-NAS/math/nas_math.h"
#elif defined(NAUT_CONFIG_BASE_LIBM_COMPAT)
#include "nautilus/libccompat.h"
#else
#error "Cannot compile Floating point tests without a math library!"
#endif

#ifdef NAUT_CONFIG_ARCH_ARM64
#define FP_TEST_SAVE(r, v) \
  do {\
    asm volatile ("FMOV " #r ", " #v); \
  } while(0)
#define FP_TEST_CHECK(r, s) \
  do {\
    asm volatile ("FMOV %0, " #r : "=r" (s)); \
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

static int test_fp_register_volatility(void) {
 
  wait_to_be_annoying = 1;
  has_been_annoying = 0;
  if(nk_thread_start(annoying_func, NULL, 0, 0, 4096, NULL, my_cpu_id())) {
    ERROR_PRINT("Failed to launch annoying thread!\n");
  }

  FP_TEST_SAVE(d0, 12.);
  FP_TEST_SAVE(d1, 12.);
  FP_TEST_SAVE(d2, 12.);
  FP_TEST_SAVE(d3, 12.);
  FP_TEST_SAVE(d4, 12.);
  FP_TEST_SAVE(d5, 12.);
  FP_TEST_SAVE(d6, 12.);
  FP_TEST_SAVE(d7, 12.);
  FP_TEST_SAVE(d8, 12.);
  FP_TEST_SAVE(d9, 12.);
  FP_TEST_SAVE(d10, 12.);
  FP_TEST_SAVE(d11, 12.);
  FP_TEST_SAVE(d12, 12.);
  FP_TEST_SAVE(d13, 12.);
  FP_TEST_SAVE(d14, 12.);
  FP_TEST_SAVE(d15, 12.);
  FP_TEST_SAVE(d16, 12.);
  FP_TEST_SAVE(d17, 12.);
  FP_TEST_SAVE(d18, 12.);
  FP_TEST_SAVE(d19, 12.);
  FP_TEST_SAVE(d20, 12.);
  FP_TEST_SAVE(d21, 12.);
  FP_TEST_SAVE(d22, 12.);
  FP_TEST_SAVE(d23, 12.);
  FP_TEST_SAVE(d24, 12.);
  FP_TEST_SAVE(d25, 12.);
  FP_TEST_SAVE(d26, 12.);
  FP_TEST_SAVE(d27, 12.);
  FP_TEST_SAVE(d28, 12.);
  FP_TEST_SAVE(d29, 12.);
  FP_TEST_SAVE(d30, 12.);
  FP_TEST_SAVE(d31, 12.);

  // Let the other thread off the leash
  wait_to_be_annoying = 0;

  while(!has_been_annoying) {
    // Wait for the other thread to do it's work
  }

  // See if we were affected

  double values[32];

  FP_TEST_CHECK(d0, values[0]);
  FP_TEST_CHECK(d1, values[1]);
  FP_TEST_CHECK(d2, values[2]);
  FP_TEST_CHECK(d3, values[3]);
  FP_TEST_CHECK(d4, values[4]);
  FP_TEST_CHECK(d5, values[5]);
  FP_TEST_CHECK(d6, values[6]);
  FP_TEST_CHECK(d7, values[7]);
  FP_TEST_CHECK(d8, values[8]);
  FP_TEST_CHECK(d9, values[9]);
  FP_TEST_CHECK(d10, values[10]);
  FP_TEST_CHECK(d11, values[11]);
  FP_TEST_CHECK(d12, values[12]);
  FP_TEST_CHECK(d13, values[13]);
  FP_TEST_CHECK(d14, values[14]);
  FP_TEST_CHECK(d15, values[15]);
  FP_TEST_CHECK(d16, values[16]);
  FP_TEST_CHECK(d17, values[17]);
  FP_TEST_CHECK(d18, values[18]);
  FP_TEST_CHECK(d19, values[19]);
  FP_TEST_CHECK(d20, values[20]);
  FP_TEST_CHECK(d21, values[21]);
  FP_TEST_CHECK(d22, values[22]);
  FP_TEST_CHECK(d23, values[23]);
  FP_TEST_CHECK(d24, values[24]);
  FP_TEST_CHECK(d25, values[25]);
  FP_TEST_CHECK(d26, values[26]);
  FP_TEST_CHECK(d27, values[27]);
  FP_TEST_CHECK(d28, values[28]);
  FP_TEST_CHECK(d29, values[29]);
  FP_TEST_CHECK(d30, values[30]);
  FP_TEST_CHECK(d31, values[31]);

  int failed = 0;
  for(unsigned int i = 0; i < 32; i++) {
    if(values[i] != 12.) {
      printk("Register (d%u) was corrupted during context switch!\n");
      failed++;
    }
  }

  uint64_t fpcr;
  LOAD_SYS_REG(FPCR, fpcr);
  printk("FPCR = 0x%llx\n", fpcr);

  nk_join_all_children(NULL);

  return failed;
}
#endif

#define DEFINE_SIMPLE_TEST(FUNC_NAME) \
int simple_ ## FUNC_NAME ## _test(double x, double check, double range) { \
  double s = FUNC_NAME(x); \
  if(s <= (check+range) && s >= (check-range)) { \
    printk("\t" #FUNC_NAME "(%10.10lf) = %20.20lf [CORRECT]\n", x, s, check-range, check+range); \
    return 0; \
  } else { \
    printk("\t" #FUNC_NAME "(%10.10lf) = %20.20lf (low_bound = %20.20lf, high_bound = %20.20lf)\n", x, s, check-range, check+range); \
    return 1; \
  } \
}

DEFINE_SIMPLE_TEST(sqrt)
DEFINE_SIMPLE_TEST(exp)
DEFINE_SIMPLE_TEST(log)
DEFINE_SIMPLE_TEST(sin)
DEFINE_SIMPLE_TEST(cos)

#define PI 3.14159265358979323
#define E 2.718281828459045
#define E_SQR 7.38905609893

int sqrt_test(void) 
{
  int failed = 0;
  failed += simple_sqrt_test(2.0, 1.4142135, 0.00001);
  failed += simple_sqrt_test(20000.0, 141.42135, 0.00001);
  failed += simple_sqrt_test(1.0, 1.0, 0.0);
  failed += simple_sqrt_test(0.0, 0.0, 0.0);
  failed += simple_sqrt_test(0.25, 0.49999999, 0.0000001);
  return failed;
}

int exp_test(void) 
{
  int failed = 0;
  failed += simple_exp_test(2.0, 7.389056, 0.000001);
  failed += simple_exp_test(0.0, 1.0, 0.0);
  failed += simple_exp_test(-1.0, 0.367879, 0.00001);
  return failed;
}

int log_test(void) 
{
  int failed = 0;
  failed += simple_log_test(2.0, 0.693147, 0.000001);
  failed += simple_log_test(1.0, 0.0, 0.0);
  failed += simple_log_test(E, 1.0, 0.000001);
  failed += simple_log_test(E_SQR, 2.0, 0.000001);
  failed += simple_log_test(exp(E), E, 0.000001);
  return failed;
}

int sin_test(void) {
  int failed = 0;
  failed += simple_sin_test(0.0, 0.0, 0.0);
  failed += simple_sin_test(PI, 0.0, 0.000001);
  failed += simple_sin_test(-PI, 0.0, 0.000001);
  failed += simple_sin_test(PI/2, 1.0, 0.000001);
  failed += simple_sin_test(-PI/2, -1.0, 0.000001);
  failed += simple_sin_test((3*PI)/2, -1.0, 0.000001);
  failed += simple_sin_test(-(3*PI)/2, 1.0, 0.000001);
  failed += simple_sin_test(2*PI, 0.0, 0.000001);
  failed += simple_sin_test(-2*PI, 0.0, 0.000001);
  return failed;
}

int cos_test(void) {
  int failed = 0;
  failed += simple_cos_test(0.0, 1.0, 0.0);
  failed += simple_cos_test(PI, -1.0, 0.000001);
  failed += simple_cos_test(-PI, -1.0, 0.000001);
  failed += simple_cos_test(PI/2, 0.0, 0.000001);
  failed += simple_cos_test(-PI/2, 0.0, 0.000001);
  failed += simple_cos_test((3*PI)/2, 0.0, 0.000001);
  failed += simple_cos_test(-(3*PI)/2, 0.0, 0.000001);
  return failed;
}

static int
handle_fptest (char * buf, void * priv)
{
  const char *result;
#ifdef NAUT_CONFIG_ARCH_ARM64
    if(test_fp_register_volatility()) {
      result = "FAILED";
    } else {
      result = "PASSED";
    }
    printk("FP Register Volatility Test: [%s]\n", result);
#endif
    if(sqrt_test()) {
      result = "FAILED";
    } else {
      result = "PASSED";
    }
    printk("Sqrt Test: [%s]\n", result);

    if(exp_test()) {
      result = "FAILED";
    } else {
      result = "PASSED";
    }
    printk("Exp Test: [%s]\n", result);

    if(log_test()) {
      result = "FAILED";
    } else {
      result = "PASSED";
    }
    printk("Log Test: [%s]\n", result);

    if(sin_test()) {
      result = "FAILED";
    } else {
      result = "PASSED";
    }
    printk("Sin Test: [%s]\n", result);

    if(cos_test()) {
      result = "FAILED";
    } else {
      result = "PASSED";
    }
    printk("Cos Test: [%s]\n", result);

    return 0;
}

static struct shell_cmd_impl fp_test_impl = {
    .cmd      = "fptest",
    .help_str = "fptest",
    .handler  = handle_fptest,
};
nk_register_shell_cmd(fp_test_impl);

