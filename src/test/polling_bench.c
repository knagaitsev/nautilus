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
 * Copyright (c) 2021, Kevin McAfee <kevinmcafee2022@u.northwestern.edu>
 * Copyright (c) 2021, The V3VEE Project  <http://www.v3vee.org> 
 *                     The Hobbes Project <http://xstack.sandia.gov/hobbes>
 * All rights reserved.
 *
 * Author:  Kevin McAfee <kevinmcafee2022@u.northwestern.edu>
 *  
 * This is free software.  You are permitted to use,
 * redistribute, and modify it as specified in the file "LICENSE.txt".
 */

#include <nautilus/nautilus.h>
#include <nautilus/shell.h>
#include <nautilus/vc.h>
#include <nautilus/cpu.h>
#include <nautilus/libccompat.h>
#include <dev/apic.h>

#define DO_PRINT       0

#if DO_PRINT
#define PRINT(...) nk_vc_printf(__VA_ARGS__)
#else
#define PRINT(...) 
#endif 

static inline uint64_t my_rdtscp() {
  uint64_t tsc;
  __asm__ __volatile__("rdtscp; "         // serializing read of tsc
                     "shl $32,%%rdx; "  // shift higher 32 bits stored in rdx up
                     "or %%rdx,%%rax"   // and or onto rax
                     : "=a"(tsc)        // output to tsc variable
                     :
                     : "%rcx", "%rdx"); // rcx and rdx are clobbered
  return tsc;
}

/******************* Test Routines *******************/

// PMIO
#define PORT_80  0x80

// MMIO
#define LAPIC_ID_REGISTER 0xFEC00020
#define PCI_CONFIGURATOR_ADDRESS 0xCF8
#define PCI_CONFIGURATOR_DATA 0xCFC

#define READ_PORT(port) inb(port)
#define WRITE_PORT(port) outb(port)

#define READ_MEM(address)         (*((volatile uint32_t*)(address)))
#define WRITE_MEM(address)     ((*((volatile uint32_t*)(address)))=(v))

#define R1 100            // number of rows in Matrix-1
#define C1 100            // number of columns in Matrix-1
#define R2 100            // number of rows in Matrix-2
#define C2 100            // number of columns in Matrix-2

void fillMatrices(int mat1[][C1], int mat2[][C2]) {
  for (int r = 0; r < R1; r++) {
      for (int c = 0; c < C1; c++) {
        mat1[r][c] = rand();
      }
    }
    for (int r = 0; r < R2; r++) {
      for (int c = 0; c < C2; c++) {
        mat2[r][c] = rand();
      }
    }
}

void mulMatricesPure(int mat1[][C1], int mat2[][C2]) {
  volatile int result[R1][C2];
  for (int i = 0; i < R1; i++) {
    for (int j = 0; j < C2; j++) {
      result[i][j] = 0;
      for (int k = 0; k < R2; k++) {
        result[i][j] += mat1[i][k] * mat2[k][j];
      }
    }
  }
}

uint64_t doMatrixMulPure() {
  int mat1[R1][C1];
  int mat2[R2][C2];
  fillMatrices(mat1, mat2);
  uint64_t start = rdtscp();
  mulMatricesPure(mat1, mat2);
  uint64_t end = rdtscp();
  return end - start;
}

uint8_t mulMatricesPIO(int mat1[][C1], int mat2[][C2]) {
  uint8_t status;
  volatile int result[R1][C2];
  for (int i = 0; i < R1; i++) {
    // do pmio
    status = inb(PORT_80);
    for (int j = 0; j < C2; j++) {
      result[i][j] = 0;

      for (int k = 0; k < R2; k++) {
        result[i][j] += mat1[i][k] * mat2[k][j];
      }
    }
  }
  return status;
}

uint8_t mulMatricesMMIOLapic(int mat1[][C1], int mat2[][C2]) {
  uint8_t status;
  volatile int result[R1][C2];
  for (int i = 0; i < R1; i++) {
    // do pmio
    status = READ_MEM(LAPIC_ID_REGISTER);
    for (int j = 0; j < C2; j++) {
      result[i][j] = 0;

      for (int k = 0; k < R2; k++) {
        result[i][j] += mat1[i][k] * mat2[k][j];
      }
    }
  }
  return status;
}

uint64_t doMatrixMulPIO() {
  int mat1[R1][C1];
  int mat2[R2][C2];
  fillMatrices(mat1, mat2);
  uint64_t start = rdtscp();
  mulMatricesPIO(mat1, mat2);
  uint64_t end = rdtscp();
  return end - start;
}

uint64_t doMatrixMulMMIOLapic() {
  int mat1[R1][C1];
  int mat2[R2][C2];
  fillMatrices(mat1, mat2);
  uint64_t start = rdtscp();
  mulMatricesMMIOLapic(mat1, mat2);
  uint64_t end = rdtscp();
  return end - start;
}

uint64_t doArithmeticPure() {
  volatile int a = rand();
  volatile int b = rand();
  volatile int c = rand();
  uint64_t start = rdtscp();
  a+=b;
  b-=a;
  c = a+b - c;
  c*=b;
  b = (b + 2*c - a +4) / 3;
  uint64_t end = rdtscp();
  return end - start;
}

uint64_t doArithmeticPIO() {
  volatile int a = rand();
  volatile int b = rand();
  volatile int c = rand();
  volatile uint8_t status;
  uint64_t start = rdtscp();
  a+=b;
  b-=a;
  c = a+b - c;
  c*=b;
  status = inb(PORT_80);
  b = (b + 2*c - a +4) / 3;
  uint64_t end = rdtscp();
  return end - start;
}

uint64_t doArithmeticMMIOLapic() {
  volatile int a = rand();
  volatile int b = rand();
  volatile int c = rand();
  volatile uint8_t status;
  uint64_t start = rdtscp();
  a+=b;
  b-=a;
  c = a+b - c;
  c*=b;
  status = READ_MEM(LAPIC_ID_REGISTER);
  b = (b + 2*c - a +4) / 3;
  uint64_t end = rdtscp();
  return end - start;
}

__attribute__ ((noinline))
uint16_t num(int n) {
  return n;
}

uint64_t doLoopPure() {
  uint16_t n = num(100);
  volatile uint16_t arr[n];
  uint64_t start = rdtscp();
  for (int i = 0; i < n; i++) {
    arr[i] = i;
  }
  for (int i = 0; i < n; i++) {
    arr[i] = arr[(i+1)%n];
  }
  uint64_t end = rdtscp();
  return end - start;
}

uint64_t doLoopPIO() {
  uint16_t n = num(100);
  volatile uint16_t arr[n];
  volatile uint8_t status;
  uint64_t start = rdtscp();
  for (int i = 0; i < n; i++) {
    arr[i] = i;
  }
  status = inb(PORT_80);
  for (int i = 0; i < n; i++) {
    arr[i] = arr[(i+1)%n];
  }
  uint64_t end = rdtscp();
  return end - start;
}

uint64_t doLoopMMIOLapic() {
  uint16_t n = num(100);
  volatile uint16_t arr[n];
  volatile uint8_t status;
  uint64_t start = rdtscp();
  for (int i = 0; i < n; i++) {
    arr[i] = i;
  }
  status = READ_MEM(LAPIC_ID_REGISTER);
  for (int i = 0; i < n; i++) {
    arr[i] = arr[(i+1)%n];
  }
  uint64_t end = rdtscp();
  return end - start;
}

uint64_t straightPMIO() {
  uint64_t diff;
  // uint64_t start = rdtscp();
  // inb(PORT_80);
  // uint64_t end = rdtscp();
  __asm__ __volatile__("rdtscp; " 
                      "mov %%edx, %%edi;"
                      "mov %%eax, %%esi;" 
                     "in $0x80, %%al;"
                     "rdtscp; "
                     "shl $32,%%rdx; " 
                     "or %%rdx,%%rax; " 
                     "shl $32,%%rdi; " 
                     "or %%rdi,%%rsi; "
                     "sub %%rsi, %%rax;" 
                     : "=a"(diff) 
                     :
                     : "%rcx", "%rdx", "%rdi", "%rsi");
  // return end - start;
  return diff;
}

uint64_t straightMMIOLapic() {
  uint64_t start = rdtscp();
  uint8_t in = READ_MEM(LAPIC_ID_REGISTER);
  uint64_t end = rdtscp();
  return end - start;
}

int test_straight_PMIO() {
  uint64_t pmio = straightPMIO();
  nk_vc_printf("Straight PMIO complete.\n\tCycles: %lu\n", pmio);
  return 0;
}

int test_straight_MMIOLapic() {
  uint64_t pmio = straightPMIO();
  nk_vc_printf("Straight MMIO Lapic complete.\n\tCycles: %lu\n", pmio);
  return 0;
}

int test_pmio_matrix_mul() {
  uint64_t pure = doMatrixMulPure();
  uint64_t pio = doMatrixMulPIO();
  nk_vc_printf("PMIO matrix mul complete.\n\tPure cycles: %lu\n\tPMIO cycles: %lu\n", pure, pio);
  return 0;
}

int test_mmio_lapic_matrix_mul() {
  uint64_t pure = doMatrixMulPure();
  uint64_t pio = doMatrixMulMMIOLapic();
  nk_vc_printf("MMIO Lapic matrix mul complete.\n\tPure cycles: %lu\n\tMMIO cycles: %lu\n", pure, pio);
  return 0;
}

int test_pmio_arithmetic() {
  uint64_t pure = doArithmeticPure();
  uint64_t pio = doArithmeticPIO();
  nk_vc_printf("PMIO arithmetic complete.\n\tPure cycles: %lu\n\tPMIO cycles: %lu\n", pure, pio);
  return 0;
}

int test_mmio_lapic_arithmetic() {
  uint64_t pure = doArithmeticPure();
  uint64_t pio = doArithmeticMMIOLapic();
  nk_vc_printf("MMIO Lapic arithmetic complete.\n\tPure cycles: %lu\n\tMMIO cycles: %lu\n", pure, pio);
  return 0;
}

int test_pmio_simple_loop() {
  uint64_t pure = doLoopPure();
  uint64_t pio = doLoopPIO();
  nk_vc_printf("PMIO simple loop complete.\n\tPure cycles: %lu\n\tPMIO cycles: %lu\n", pure, pio);
  return 0;
}

int test_mmio_lapic_simple_loop() {
  uint64_t pure = doLoopPure();
  uint64_t pio = doLoopMMIOLapic();
  nk_vc_printf("MMIO Lapic simple loop complete.\n\tPure cycles: %lu\n\tMMIO cycles: %lu\n", pure, pio);
  return 0;
}

#define IO_ITERATIONS 1048576 // 2^20

int test_port_80_rdtsc_wrap_loop() {
  uint64_t start = rdtsc();
  for (uint32_t i = 0; i < IO_ITERATIONS; i++) {
    inb(PORT_80);
  }
  uint64_t end = rdtsc();
  uint64_t t_avg = (end-start) / IO_ITERATIONS;
  nk_vc_printf("test_port_80_rdtsc_wrap_loop.\n\tAverage cycles: %lu\n", t_avg);
  return 0;
}

int test_port_80_rdtsc_wrap_loop_asm() {
  uint64_t diff;
  __asm__ __volatile__(
      
                      "rdtsc; " 
                      "mov %%edx, %%edi;" // start upper half
                      "mov %%eax, %%esi;" // start lower half
                      "mov $0x100000, %%ecx;" // IO_ITERATIONS hard-coded for 2^20
                      "1:"
                      "in $0x80, %%al;"
                      "sub $0x1, %%ecx;"
                      "jne 1b;"

                      "rdtsc; "
                      "shl $32,%%rdx; " 
                      "or %%rdx,%%rax; " 
                      "shl $32,%%rdi; " 
                      "or %%rdi,%%rsi; "
                      "sub %%rsi, %%rax;" 
                      : "=a"(diff) 
                      :
                      : "%rcx", "%rdx", "%rdi", "%rsi");
  uint64_t t_avg = (diff) / IO_ITERATIONS;
  nk_vc_printf("test_port_80_rdtsc_wrap_loop_asm.\n\tAverage cycles: %lu\n", t_avg);
  return 0;
}

int test_port_80_rdtsc_within_loop() {
  uint64_t sum = 0;
  uint64_t sum2 = 0;
  uint64_t min = ~0;
  for (uint32_t i = 0; i < IO_ITERATIONS; i++) {
    uint64_t start = rdtsc();
    inb(PORT_80);
    uint64_t end = rdtsc();
    uint64_t diff = end-start;
    sum += diff;
    sum2 += diff*diff;
    min = diff < min ? diff : min;
  }
  uint64_t t_avg = (sum) / IO_ITERATIONS;
  uint64_t variance = (sum2/IO_ITERATIONS) - (t_avg*t_avg);
  nk_vc_printf("test_port_80_rdtsc_within_loop.\n\tAverage cycles: %lu\n\tMinimum cycle: %lu\n\tVariance: %lu\n", t_avg, min, variance);
  return 0;
}

int test_port_80_rdtsc_within_loop_asm() {
  uint64_t sum = 0;
  uint64_t sum2 = 0;
  uint64_t min = ~0;
  __asm__ __volatile__(
                      "xor %0, %0;"
                      "xor %1, %1;"
                      "mov 0x0, %2;"
                      "sub 0x1, %2;"
                      "mov $0x100000, %%r8;" // IO_ITERATIONS hard-coded for 2^20                  
                      "1: "
                      "rdtsc; "
                      "mov %%edx, %%edi;" // start upper half
                      "mov %%eax, %%esi;" // start lower half
                      
                      "in $0x80, %%al;"

                      "rdtsc;"
                      "shl $32,%%rdx; " 
                      "or %%rdx,%%rax; " 
                      "shl $32,%%rdi; " 
                      "or %%rdi,%%rsi; "
                      "sub %%rsi, %%rax;" // rax has diff

                      "cmp %%rax, %2;" // check for min
                      "cmova %%rax, %2;" // mov if above min

                      "add %%rax, %0;" // add diff to sum
                      "imul %%rax, %%rax;" // square diff
                      "add %%rax, %1;" // add diff^2 to sum2

                      "sub $0x1, %%r8;"
                      "jne 1b;"

                      : "=r"(sum), "=r"(sum2), "=r"(min) 
                      : 
                      : "%rax", "%rcx", "%rdx", "%rdi", "%rsi", "%r8");

  uint64_t t_avg = (sum) / IO_ITERATIONS;
  uint64_t variance = (sum2/IO_ITERATIONS) - (t_avg*t_avg);
  nk_vc_printf("test_port_80_rdtsc_within_loop_asm.\n\tAverage cycles: %lu\n\tMinimum cycle: %lu\n\tVariance: %lu\n", t_avg, min, variance);
  return 0;
}

int test_port_80_rdtscp_wrap_loop() {
  uint64_t start = rdtscp();
  for (uint32_t i = 0; i < IO_ITERATIONS; i++) {
    inb(PORT_80);
  }
  uint64_t end = rdtscp();
  uint64_t t_avg = (end-start) / IO_ITERATIONS;
  nk_vc_printf("test_port_80_rdtscp_wrap_loop.\n\tAverage cycles: %lu\n", t_avg);
  return 0;
}

int test_port_80_rdtscp_wrap_loop_asm() {
  uint64_t diff;
  __asm__ __volatile__(
      
                      "rdtscp; " 
                      "mov %%edx, %%edi;" // start upper half
                      "mov %%eax, %%esi;" // start lower half
                      "mov $0x100000, %%ecx;" // IO_ITERATIONS hard-coded for 2^20
                      "1:"
                      "in $0x80, %%al;"
                      "sub $0x1, %%ecx;"
                      "jne 1b;"

                      "rdtscp; "
                      "shl $32,%%rdx; " 
                      "or %%rdx,%%rax; " 
                      "shl $32,%%rdi; " 
                      "or %%rdi,%%rsi; "
                      "sub %%rsi, %%rax;" 
                      : "=a"(diff) 
                      :
                      : "%rcx", "%rdx", "%rdi", "%rsi");
  uint64_t t_avg = (diff) / IO_ITERATIONS;
  nk_vc_printf("test_port_80_rdtscp_wrap_loop_asm.\n\tAverage cycles: %lu\n", t_avg);
  return 0;
}

int test_port_80_rdtscp_within_loop() {
  uint64_t sum = 0;
  uint64_t sum2 = 0;
  uint64_t min = ~0;
  for (uint32_t i = 0; i < IO_ITERATIONS; i++) {
    uint64_t start = rdtscp();
    inb(PORT_80);
    uint64_t end = rdtscp();
    uint64_t diff = end-start;
    sum += diff;
    sum2 += diff*diff;
    min = diff < min ? diff : min;
  }  
  uint64_t t_avg = (sum) / IO_ITERATIONS;
  uint64_t variance = (sum2/IO_ITERATIONS) - (t_avg*t_avg);
  nk_vc_printf("test_port_80_rdtscp_within_loop.\n\tAverage cycles: %lu\n\tMinimum cycle: %lu\n\tVariance: %lu\n", t_avg, min, variance);
  return 0;
}

int test_port_80_rdtscp_within_loop_asm() {
  uint64_t sum = 0;
  uint64_t sum2 = 0;
  uint64_t min = ~0;
  __asm__ __volatile__(
                      "xor %0, %0;"
                      "xor %1, %1;"
                      "mov 0x0, %2;"
                      "sub 0x1, %2;" 
                      "mov $0x100000, %%r8;" // IO_ITERATIONS hard-coded for 2^20                  
                      "1: "
                      "rdtscp; "
                      "mov %%edx, %%edi;" // start upper half
                      "mov %%eax, %%esi;" // start lower half
                      
                      "in $0x80, %%al;"

                      "rdtscp;"
                      "shl $32,%%rdx; " 
                      "or %%rdx,%%rax; " 
                      "shl $32,%%rdi; " 
                      "or %%rdi,%%rsi; "
                      "sub %%rsi, %%rax;" // rax has diff

                      "cmp %%rax, %2;" // check for min
                      "cmova %%rax, %2;" // mov if above min

                      "add %%rax, %0;" // add diff to sum
                      "imul %%rax, %%rax;" // square diff
                      "add %%rax, %1;" // add diff^2 to sum2

                      "sub $0x1, %%r8;"
                      "jne 1b;"

                      : "=r"(sum), "=r"(sum2), "=r"(min) 
                      : 
                      : "%rax", "%rcx", "%rdx", "%rdi", "%rsi", "%r8");

  uint64_t t_avg = (sum) / IO_ITERATIONS;
  uint64_t variance = (sum2/IO_ITERATIONS) - (t_avg*t_avg);
  nk_vc_printf("test_port_80_rdtscp_within_loop_asm.\n\tAverage cycles: %lu\n\tMinimum cycle: %lu\n\tVariance: %lu\n", t_avg, min, variance);
  return 0;
}

//
// LAPIC
//

int test_lapic_rdtsc_wrap_loop() {
  uint64_t start = rdtsc();
  for (uint32_t i = 0; i < IO_ITERATIONS; i++) {
    READ_MEM(LAPIC_ID_REGISTER);
  }
  uint64_t end = rdtsc();
  uint64_t t_avg = (end-start) / IO_ITERATIONS;
  nk_vc_printf("test_lapic_rdtsc_wrap_loop.\n\tAverage cycles: %lu\n", t_avg);
  return 0;
}

int test_lapic_rdtsc_wrap_loop_asm() {
  uint64_t diff;
  __asm__ __volatile__(
                      "mov $0xfec00020, %%rbx;"
                      "rdtsc; " 
                      "mov %%edx, %%edi;" // start upper half
                      "mov %%eax, %%esi;" // start lower half
                      "mov $0x100000, %%ecx;" // IO_ITERATIONS hard-coded for 2^20
                      "1:"
                      "mov (%%rbx), %%eax;"
                      "sub $0x1, %%ecx;"
                      "jne 1b;"

                      "rdtsc; "
                      "shl $32,%%rdx; " 
                      "or %%rdx,%%rax; " 
                      "shl $32,%%rdi; " 
                      "or %%rdi,%%rsi; "
                      "sub %%rsi, %%rax;" 
                      : "=a"(diff) 
                      :
                      : "%rbx", "%rcx", "%rdx", "%rdi", "%rsi");
  uint64_t t_avg = (diff) / IO_ITERATIONS;
  nk_vc_printf("test_lapic_rdtsc_wrap_loop_asm.\n\tAverage cycles: %lu\n", t_avg);
  return 0;
}

int test_lapic_rdtsc_within_loop() {
  uint64_t sum = 0;
  uint64_t sum2 = 0;
  uint64_t min = ~0;
  uint64_t max = 0;
  for (uint32_t i = 0; i < IO_ITERATIONS; i++) {
    uint64_t start = rdtsc();
    READ_MEM(LAPIC_ID_REGISTER);
    uint64_t end = rdtsc();
    uint64_t diff = end-start;
    sum += diff;
    sum2 += diff*diff;
    min = diff < min ? diff : min;
    max = diff > max ? diff : max;
  }  
  uint64_t t_avg = (sum) / IO_ITERATIONS;
  uint64_t variance = (sum2/IO_ITERATIONS) - (t_avg*t_avg);
  nk_vc_printf("test_lapic_rdtsc_within_loop.\n\tAverage cycles: %lu\n\tMinimum cycle: %lu\n\tVariance: %lu\n\tSum: %lu\n\tMax: %lu\n", t_avg, min, variance, sum, max);
  return 0;
}

int test_lapic_rdtsc_within_loop_asm() {
  uint64_t sum = 0;
  uint64_t sum2 = 0;
  uint64_t min = ~0;
  __asm__ __volatile__(
                      "mov $0xfec00020, %%rbx;"
                      "xor %0, %0;"
                      "xor %1, %1;"
                      "mov 0x0, %2;"
                      "sub 0x1, %2;"
                      "mov $0x100000, %%r8;" // IO_ITERATIONS hard-coded for 2^20                  
                      "1: "
                      "rdtsc; "
                      "mov %%edx, %%edi;" // start upper half
                      "mov %%eax, %%esi;" // start lower half
                      
                      "mov (%%rbx), %%eax;"

                      "rdtsc;"
                      "shl $32,%%rdx; " 
                      "or %%rdx,%%rax; " 
                      "shl $32,%%rdi; " 
                      "or %%rdi,%%rsi; "
                      "sub %%rsi, %%rax;" // rax has diff

                      "cmp %%rax, %2;" // check for min
                      "cmova %%rax, %2;" // mov if above min

                      "add %%rax, %0;" // add diff to sum
                      "imul %%rax, %%rax;" // square diff
                      "add %%rax, %1;" // add diff^2 to sum2

                      "sub $0x1, %%r8;"
                      "jne 1b;"

                      : "=r"(sum), "=r"(sum2), "=r"(min) 
                      : 
                      : "%rax", "%rbx", "%rcx", "%rdx", "%rdi", "%rsi", "%r8");

  uint64_t t_avg = (sum) / IO_ITERATIONS;
  uint64_t variance = (sum2/IO_ITERATIONS) - (t_avg*t_avg);
  nk_vc_printf("test_lapic_rdtsc_within_loop_asm.\n\tAverage cycles: %lu\n\tMinimum cycle: %lu\n\tVariance: %lu\n", t_avg, min, variance);
  return 0;
}


int test_lapic_rdtscp_wrap_loop() {
  uint64_t start = rdtscp();
  for (uint32_t i = 0; i < IO_ITERATIONS; i++) {
    READ_MEM(LAPIC_ID_REGISTER);
  }
  uint64_t end = rdtscp();
  uint64_t t_avg = (end-start) / IO_ITERATIONS;
  nk_vc_printf("test_lapic_rdtscp_wrap_loop.\n\tAverage cycles: %lu\n", t_avg);
  return 0;
}

int test_lapic_rdtscp_wrap_loop_asm() {
  uint64_t diff;
  __asm__ __volatile__(
                      "mov $0xfec00020, %%rbx;"
                      "rdtscp; " 
                      "mov %%edx, %%edi;" // start upper half
                      "mov %%eax, %%esi;" // start lower half
                      "mov $0x100000, %%ecx;" // IO_ITERATIONS hard-coded for 2^20
                      "1:"
                      "mov (%%rbx), %%eax;"
                      "sub $0x1, %%ecx;"
                      "jne 1b;"

                      "rdtscp; "
                      "shl $32,%%rdx; " 
                      "or %%rdx,%%rax; " 
                      "shl $32,%%rdi; " 
                      "or %%rdi,%%rsi; "
                      "sub %%rsi, %%rax;" 
                      : "=a"(diff) 
                      :
                      : "%rbx", "%rcx", "%rdx", "%rdi", "%rsi");
  uint64_t t_avg = (diff) / IO_ITERATIONS;
  nk_vc_printf("test_lapic_rdtscp_wrap_loop_asm.\n\tAverage cycles: %lu\n", t_avg);
  return 0;
}

int test_lapic_rdtscp_within_loop() {
  uint64_t sum = 0;
  uint64_t sum2 = 0;
  uint64_t min = ~0;
  uint64_t max = 0;
  for (uint32_t i = 0; i < IO_ITERATIONS; i++) {
    uint64_t start = rdtscp();
    READ_MEM(LAPIC_ID_REGISTER);
    uint64_t end = rdtscp();
    uint64_t diff = end-start;
    sum += diff;
    sum2 += diff*diff;
    min = diff < min ? diff : min;
    max = diff > max ? diff : max;
  }  
  uint64_t t_avg = (sum) / IO_ITERATIONS;
  uint64_t variance = (sum2/IO_ITERATIONS) - (t_avg*t_avg);
  nk_vc_printf("test_lapic_rdtscp_within_loop.\n\tAverage cycles: %lu\n\tMinimum cycle: %lu\n\tVariance: %lu\n\tSum: %lu\n\tMax: %lu\n", t_avg, min, variance, sum, max);
  return 0;
}

int test_lapic_rdtscp_within_loop_asm() {
  uint64_t sum = 0;
  uint64_t sum2 = 0;
  uint64_t min = ~0;
  __asm__ __volatile__(
                      "mov $0xfec00020, %%rbx;"
                      "xor %0, %0;"
                      "xor %1, %1;"
                      "mov 0x0, %2;"
                      "sub 0x1, %2;"
                      "mov $0x100000, %%r8;" // IO_ITERATIONS hard-coded for 2^20                  
                      "1: "
                      "rdtscp; "
                      "mov %%edx, %%edi;" // start upper half
                      "mov %%eax, %%esi;" // start lower half
                      
                      "mov (%%rbx), %%eax;"

                      "rdtscp;"
                      "shl $32,%%rdx; " 
                      "or %%rdx,%%rax; " 
                      "shl $32,%%rdi; " 
                      "or %%rdi,%%rsi; "
                      "sub %%rsi, %%rax;" // rax has diff

                      "cmp %%rax, %2;" // check for min
                      "cmova %%rax, %2;" // mov if above min

                      "add %%rax, %0;" // add diff to sum
                      "imul %%rax, %%rax;" // square diff
                      "add %%rax, %1;" // add diff^2 to sum2

                      "sub $0x1, %%r8;"
                      "jne 1b;"

                      : "=r"(sum), "=r"(sum2), "=r"(min) 
                      : 
                      : "%rax", "%rbx", "%rcx", "%rdx", "%rdi", "%rsi", "%r8");

  uint64_t t_avg = (sum) / IO_ITERATIONS;
  uint64_t variance = (sum2/IO_ITERATIONS) - (t_avg*t_avg);
  nk_vc_printf("test_lapic_rdtscp_within_loop_asm.\n\tAverage cycles: %lu\n\tMinimum cycle: %lu\n\tVariance: %lu\n", t_avg, min, variance);
  return 0;
}

//
// PCI CONFIGURATOR
//

int test_pci_config_rdtsc_wrap_loop_asm() {
  uint64_t diff;
  __asm__ __volatile__(
                      "mov $0xcf8, %%rbx;"
                      "rdtsc; " 
                      "mov %%edx, %%edi;" // start upper half
                      "mov %%eax, %%esi;" // start lower half
                      "mov $0x100000, %%ecx;" // IO_ITERATIONS hard-coded for 2^20
                      "1:"
                      "mov (%%rbx), %%eax;"
                      "sub $0x1, %%ecx;"
                      "jne 1b;"

                      "rdtsc; "
                      "shl $32,%%rdx; " 
                      "or %%rdx,%%rax; " 
                      "shl $32,%%rdi; " 
                      "or %%rdi,%%rsi; "
                      "sub %%rsi, %%rax;" 
                      : "=a"(diff) 
                      :
                      : "%rbx", "%rcx", "%rdx", "%rdi", "%rsi");
  uint64_t t_avg = (diff) / IO_ITERATIONS;
  nk_vc_printf("test_pci_config_rdtsc_wrap_loop_asm.\n\tAverage cycles: %lu\n", t_avg);
  return 0;
}

int test_pci_config_rdtsc_within_loop_asm() {
  uint64_t sum = 0;
  uint64_t sum2 = 0;
  uint64_t min = ~0;
  __asm__ __volatile__(
                      "mov $0xcf8, %%rbx;"
                      "xor %0, %0;"
                      "xor %1, %1;"
                      "mov 0x0, %2;"
                      "sub 0x1, %2;"
                      "mov $0x100000, %%r8;" // IO_ITERATIONS hard-coded for 2^20                  
                      "1: "
                      "rdtsc; "
                      "mov %%edx, %%edi;" // start upper half
                      "mov %%eax, %%esi;" // start lower half
                      
                      "mov (%%rbx), %%eax;"

                      "rdtsc;"
                      "shl $32,%%rdx; " 
                      "or %%rdx,%%rax; " 
                      "shl $32,%%rdi; " 
                      "or %%rdi,%%rsi; "
                      "sub %%rsi, %%rax;" // rax has diff

                      "cmp %%rax, %2;" // check for min
                      "cmova %%rax, %2;" // mov if above min

                      "add %%rax, %0;" // add diff to sum
                      "imul %%rax, %%rax;" // square diff
                      "add %%rax, %1;" // add diff^2 to sum2

                      "sub $0x1, %%r8;"
                      "jne 1b;"

                      : "=r"(sum), "=r"(sum2), "=r"(min) 
                      : 
                      : "%rax", "%rbx", "%rcx", "%rdx", "%rdi", "%rsi", "%r8");

  uint64_t t_avg = (sum) / IO_ITERATIONS;
  uint64_t variance = (sum2/IO_ITERATIONS) - (t_avg*t_avg);
  nk_vc_printf("test_pci_config_rdtsc_within_loop_asm.\n\tAverage cycles: %lu\n\tMinimum cycle: %lu\n\tVariance: %lu\n", t_avg, min, variance);
  return 0;
}

int test_pci_config_rdtscp_wrap_loop_asm() {
  uint64_t diff;
  __asm__ __volatile__(
                      "mov $0xcf8, %%rbx;"
                      "rdtscp; " 
                      "mov %%edx, %%edi;" // start upper half
                      "mov %%eax, %%esi;" // start lower half
                      "mov $0x100000, %%ecx;" // IO_ITERATIONS hard-coded for 2^20
                      "1:"
                      "mov (%%rbx), %%eax;"
                      "sub $0x1, %%ecx;"
                      "jne 1b;"

                      "rdtscp; "
                      "shl $32,%%rdx; " 
                      "or %%rdx,%%rax; " 
                      "shl $32,%%rdi; " 
                      "or %%rdi,%%rsi; "
                      "sub %%rsi, %%rax;" 
                      : "=a"(diff) 
                      :
                      : "%rbx", "%rcx", "%rdx", "%rdi", "%rsi");
  uint64_t t_avg = (diff) / IO_ITERATIONS;
  nk_vc_printf("test_pci_config_rdtscp_wrap_loop_asm.\n\tAverage cycles: %lu\n", t_avg);
  return 0;
}

int test_pci_config_rdtscp_within_loop_asm() {
  uint64_t sum = 0;
  uint64_t sum2 = 0;
  uint64_t min = ~0;
  __asm__ __volatile__(
                      "mov $0xcf8, %%rbx;"
                      "xor %0, %0;"
                      "xor %1, %1;"
                      "mov 0x0, %2;"
                      "sub 0x1, %2;"
                      "mov $0x100000, %%r8;" // IO_ITERATIONS hard-coded for 2^20                  
                      "1: "
                      "rdtscp; "
                      "mov %%edx, %%edi;" // start upper half
                      "mov %%eax, %%esi;" // start lower half
                      
                      "mov (%%rbx), %%eax;"

                      "rdtscp;"
                      "shl $32,%%rdx; " 
                      "or %%rdx,%%rax; " 
                      "shl $32,%%rdi; " 
                      "or %%rdi,%%rsi; "
                      "sub %%rsi, %%rax;" // rax has diff

                      "cmp %%rax, %2;" // check for min
                      "cmova %%rax, %2;" // mov if above min

                      "add %%rax, %0;" // add diff to sum
                      "imul %%rax, %%rax;" // square diff
                      "add %%rax, %1;" // add diff^2 to sum2

                      "sub $0x1, %%r8;"
                      "jne 1b;"

                      : "=r"(sum), "=r"(sum2), "=r"(min) 
                      : 
                      : "%rax", "%rbx", "%rcx", "%rdx", "%rdi", "%rsi", "%r8");

  uint64_t t_avg = (sum) / IO_ITERATIONS;
  uint64_t variance = (sum2/IO_ITERATIONS) - (t_avg*t_avg);
  nk_vc_printf("test_pci_config_rdtscp_within_loop_asm.\n\tAverage cycles: %lu\n\tMinimum cycle: %lu\n\tVariance: %lu\n", t_avg, min, variance);
  return 0;
}

//
// APIC msr
//

int test_apic_msr_rdtsc_wrap_loop() {
  struct apic_dev * apic = per_cpu_get(apic);
  uint64_t start = rdtsc();
  for (uint32_t i = 0; i < IO_ITERATIONS; i++) {
    apic_read(apic, APIC_GET_IER(1));
  }
  uint64_t end = rdtsc();
  uint64_t t_avg = (end-start) / IO_ITERATIONS;
  nk_vc_printf("test_apic_msr_rdtsc_wrap_loop.\n\tAverage cycles: %lu\n", t_avg);
  return 0;
}

int test_apic_msr_rdtscp_wrap_loop() {
  struct apic_dev * apic = per_cpu_get(apic);
  uint64_t start = rdtscp();
  for (uint32_t i = 0; i < IO_ITERATIONS; i++) {
    apic_read(apic, APIC_GET_IER(1));
  }
  uint64_t end = rdtscp();
  uint64_t t_avg = (end-start) / IO_ITERATIONS;
  nk_vc_printf("test_apic_msr_rdtscp_wrap_loop.\n\tAverage cycles: %lu\n", t_avg);
  return 0;
}

int test_apic_msr_rdtsc_within_loop() {
  struct apic_dev * apic = per_cpu_get(apic);
  uint64_t sum = 0;
  uint64_t sum2 = 0;
  uint64_t min = ~0;
  uint64_t max = 0;
  for (uint32_t i = 0; i < IO_ITERATIONS; i++) {
    uint64_t start = rdtsc();
    apic_read(apic, APIC_GET_IER(1));
    uint64_t end = rdtsc();
    uint64_t diff = end-start;
    sum += diff;
    sum2 += diff*diff;
    min = diff < min ? diff : min;
    max = diff > max ? diff : max;
  }  
  uint64_t t_avg = (sum) / IO_ITERATIONS;
  uint64_t variance = (sum2/IO_ITERATIONS) - (t_avg*t_avg);
  nk_vc_printf("test_apic_msr_rdtsc_within_loop.\n\tAverage cycles: %lu\n\tMinimum cycle: %lu\n\tVariance: %lu\n\tSum: %lu\n\tMax: %lu\n", t_avg, min, variance, sum, max);
  return 0;
}

int test_apic_msr_rdtscp_within_loop() {
  struct apic_dev * apic = per_cpu_get(apic);
  uint64_t sum = 0;
  uint64_t sum2 = 0;
  uint64_t min = ~0;
  uint64_t max = 0;
  for (uint32_t i = 0; i < IO_ITERATIONS; i++) {
    uint64_t start = rdtscp();
    apic_read(apic, APIC_GET_IER(1));
    uint64_t end = rdtscp();
    uint64_t diff = end-start;
    sum += diff;
    sum2 += diff*diff;
    min = diff < min ? diff : min;
    max = diff > max ? diff : max;
  }  
  uint64_t t_avg = (sum) / IO_ITERATIONS;
  uint64_t variance = (sum2/IO_ITERATIONS) - (t_avg*t_avg);
  nk_vc_printf("test_apic_msr_rdtscp_within_loop.\n\tAverage cycles: %lu\n\tMinimum cycle: %lu\n\tVariance: %lu\n\tSum: %lu\n\tMax: %lu\n", t_avg, min, variance, sum, max);
  return 0;
}

//
// READ IER x7 ?
//

//
// RDTSC and RDTSCP
//

int test_rdtsc_within_loop() {
  uint64_t sum = 0;
  uint64_t sum2 = 0;
  uint64_t min = ~0;
  // for (uint32_t i = 0; i < IO_ITERATIONS; i++) {
  //   uint64_t start = rdtsc();
  //   uint64_t end = rdtsc();
  //   uint64_t diff = end-start;
  //   sum += diff;
  //   sum2 += diff*diff;
  //   min = diff < min ? diff : min;
  // }  
  __asm__ __volatile__(
                      "mov $0xfec00020, %%rbx;"
                      "xor %0, %0;"
                      "xor %1, %1;"
                      "mov 0x0, %2;"
                      "sub 0x1, %2;"
                      "mov $0x100000, %%r8;" // IO_ITERATIONS hard-coded for 2^20                  
                      "1: "
                      "rdtsc; "
                      "mov %%edx, %%edi;" // start upper half
                      "mov %%eax, %%esi;" // start lower half
                      "rdtsc;"
                      "shl $32,%%rdx; " 
                      "or %%rdx,%%rax; " 
                      "shl $32,%%rdi; " 
                      "or %%rdi,%%rsi; "
                      "sub %%rsi, %%rax;" // rax has diff

                      "cmp %%rax, %2;" // check for min
                      "cmova %%rax, %2;" // mov if above min

                      "add %%rax, %0;" // add diff to sum
                      "imul %%rax, %%rax;" // square diff
                      "add %%rax, %1;" // add diff^2 to sum2

                      "sub $0x1, %%r8;"
                      "jne 1b;"

                      : "=r"(sum), "=r"(sum2), "=r"(min) 
                      : 
                      : "%rax", "%rbx", "%rcx", "%rdx", "%rdi", "%rsi", "%r8");
  uint64_t t_avg = (sum) / IO_ITERATIONS;
  uint64_t variance = (sum2/IO_ITERATIONS) - (t_avg*t_avg);
  nk_vc_printf("test_rdtsc_within_loop.\n\tAverage cycles: %lu\n\tMinimum cycle: %lu\n\tVariance: %lu\n", t_avg, min, variance);
  return 0;
}

int test_rdtscp_within_loop() {
  uint64_t sum = 0;
  uint64_t sum2 = 0;
  uint64_t min = ~0;
  // for (uint32_t i = 0; i < IO_ITERATIONS; i++) {
  //   uint64_t start = rdtscp();
  //   uint64_t end = rdtscp();
  //   uint64_t diff = end-start;
  //   sum += diff;
  //   sum2 += diff*diff;
  //   min = diff < min ? diff : min;
  // }  
  __asm__ __volatile__(
                      "mov $0xfec00020, %%rbx;"
                      "xor %0, %0;"
                      "xor %1, %1;"
                      "mov 0x0, %2;"
                      "sub 0x1, %2;"
                      "mov $0x100000, %%r8;" // IO_ITERATIONS hard-coded for 2^20                  
                      "1: "
                      "rdtscp; "
                      "mov %%edx, %%edi;" // start upper half
                      "mov %%eax, %%esi;" // start lower half
                      "rdtscp;"
                      "shl $32,%%rdx; " 
                      "or %%rdx,%%rax; " 
                      "shl $32,%%rdi; " 
                      "or %%rdi,%%rsi; "
                      "sub %%rsi, %%rax;" // rax has diff

                      "cmp %%rax, %2;" // check for min
                      "cmova %%rax, %2;" // mov if above min

                      "add %%rax, %0;" // add diff to sum
                      "imul %%rax, %%rax;" // square diff
                      "add %%rax, %1;" // add diff^2 to sum2

                      "sub $0x1, %%r8;"
                      "jne 1b;"

                      : "=r"(sum), "=r"(sum2), "=r"(min) 
                      : 
                      : "%rax", "%rbx", "%rcx", "%rdx", "%rdi", "%rsi", "%r8");
  uint64_t t_avg = (sum) / IO_ITERATIONS;
  uint64_t variance = (sum2/IO_ITERATIONS) - (t_avg*t_avg);
  nk_vc_printf("test_rdtscp_within_loop.\n\tAverage cycles: %lu\n\tMinimum cycle: %lu\n\tVariance: %lu\n", t_avg, min, variance);
  return 0;
}


//
// Testing for manual interrupt clearing
//

void wait_for_any_interrupt(struct apic_dev * apic) {
  int irr_set = 0;
  while (!irr_set) {
    for (int i = 0; i < 8; i++) {
      uint32_t this_irr = apic_read(apic, APIC_GET_IRR(i));
      irr_set |= this_irr;
      if (this_irr) {
        nk_vc_printf("irr index: %d\n", i);
        printf ("%08x\n", this_irr);
        uint32_t this_irr_2 = apic_read(apic, APIC_GET_IRR(i));
        printf ("%08x\n", this_irr_2);
        break;
      }
    }
  }
}

void print_all_irr(struct apic_dev * apic) {
  nk_vc_printf("Printing IRR registers\n");
  for (int i = 0; i < 8; i++) {
    uint32_t this_irr = apic_read(apic, APIC_GET_IRR(i));
    printf("%08x\n", this_irr);
  }
}

int test_clear_interrupt_seoi() {
  struct apic_dev * apic = per_cpu_get(apic);

  // First check to see if seoi is enabled. If this faults the extended feature set is not even mapped and so none of this is enabled
  nk_vc_printf("Checking if processor is IER and SEOI capable\n");
  uint32_t extended_apic_feature_register = apic_read(apic, APIC_REG_EXFR);\
  int ier_capable = extended_apic_feature_register & 1;
  int seoi_capable = extended_apic_feature_register & 2;

  if (ier_capable) nk_vc_printf("APIC - IER Capable\n");
  else nk_vc_printf("APIC - IER NOT Capable\n");
  if (seoi_capable) nk_vc_printf("APIC - SEOI Capable\n");
  else nk_vc_printf("APIC - SEOI NOT Capable\n");

  // if we want to use the IER or SEOI we may need to first write the extended apic control register
  nk_vc_printf("Writing control register to enable use of IER and SEOI\n");
  apic_write(apic, APIC_REG_EXFC, 3);

  nk_vc_printf("Disabling interrupts\n");
  cli();

  uint32_t irrs[8];
  int irr_set = 0;

  // Wait until we have at least one IRR bit set
  while (!irr_set) {
    for (int i = 0; i < 8; i++) {
      uint32_t this_irr = apic_read(apic, APIC_GET_IRR(i));
      irr_set |= this_irr;
    }
  }

  nk_vc_printf("Printing IRR registers\n");
  for (int i = 0; i < 8; i++) {
    uint32_t this_irr = apic_read(apic, APIC_GET_IRR(i));
    irrs[i] = this_irr;
    printf ("%08x\n", this_irr);
  }

  // Clear every single interrupt vector with SEOI
  nk_vc_printf("CLearing all interrupts with SEOI\n");
  for (uint32_t vec = 0; vec < 256; vec++) {
    apic_write(apic, APIC_REG_SEOI, vec);
  }

  nk_vc_printf("Printing IRR registers\n");
  for (int i = 0; i < 8; i++) {
    uint32_t this_irr = apic_read(apic, APIC_GET_IRR(i));
    irrs[i] = this_irr;
    printf ("%08x\n", this_irr);
  }

  return 0;

}

// Test that writing 0 to all IER disables interrupts
int test_ier_disable_interrupts() {
  struct apic_dev * apic = per_cpu_get(apic);

  // First check to see if seoi is enabled. If this faults the extended feature set is not even mapped and so none of this is enabled
  nk_vc_printf("Checking if processor is IER and SEOI capable\n");
  uint32_t extended_apic_feature_register = apic_read(apic, APIC_REG_EXFR);
  int ier_capable = extended_apic_feature_register & 1;
  int seoi_capable = extended_apic_feature_register & 2;

  if (ier_capable) nk_vc_printf("APIC - IER Capable\n");
  else nk_vc_printf("APIC - IER NOT Capable\n");
  if (seoi_capable) nk_vc_printf("APIC - SEOI Capable\n");
  else nk_vc_printf("APIC - SEOI NOT Capable\n");

  if (!(ier_capable | seoi_capable)) return 0;

  // if we want to use the IER or SEOI we may need to first write the extended apic control register
  nk_vc_printf("Writing control register to enable use of IER and SEOI\n");
  apic_write(apic, APIC_REG_EXFC, 3);

  nk_vc_printf("Disabling interrupts through the IER mechansim\n");
  for (int i = 0; i < 8; i++) {
    apic_write(apic, APIC_GET_IER(i), 0);
  }

  wait_for_any_interrupt(apic);

  print_all_irr(apic);

  return 0;

}

int test_ier_handle_interrupt() {
  struct apic_dev * apic = per_cpu_get(apic);
  nk_vc_printf("Writing control register to enable use of IER and SEOI\n");
  apic_write(apic, APIC_REG_EXFC, 3);
  nk_vc_printf("Disabling interrupts through the IER mechansim\n");
  for (int i = 0; i < 8; i++) {
    apic_write(apic, APIC_GET_IER(i), 0);
  }

  // We can typically count on the apic timer interrupt (vec 240/0xF0) to fire first
  // So we will put our special code in Mr. apic_timer_handler()
  wait_for_any_interrupt(apic);

  // Show that the timer interrupt has fired (output should be 00010000)
  uint32_t this_irr = apic_read(apic, APIC_GET_IRR(7));
  printf("%08x\n", this_irr);

  // Enable the timer interrupt
  apic_write(apic, APIC_GET_IER(7), 0x00010000);

  // Show that the timer interrupt has been serviced (output should be 00000000)
  // If this is not 0 it could just mean that the interrupt has not had time to fire yet
  this_irr = apic_read(apic, APIC_GET_IRR(7));
  printf("%08x\n", this_irr);

  // Show that all interrupts have been disabled again
  nk_vc_printf("Printing IERs\n");
  for (int i = 0; i < 8; i++) {
    uint32_t this_ier = apic_read(apic, APIC_GET_IER(i));
    printf ("%08x\n", this_ier);
  }

  // Show that no interrupts are being handled
  nk_vc_printf("Printing ISRs\n");
  for (int i = 0; i < 8; i++) {
    uint32_t this_isr = apic_read(apic, APIC_GET_ISR(i));
    printf ("%08x\n", this_isr);
  }

  // Another check to make sure the interrupt was serviced, since it may not have fired before the previous check
  this_irr = apic_read(apic, APIC_GET_IRR(7));
  printf("Relevant irr: %08x\n", this_irr);

  return 0;

}

int test_ier_index_computation() {
  struct apic_dev * apic = per_cpu_get(apic);
  nk_vc_printf("Writing control register to enable use of IER and SEOI\n");
  apic_write(apic, APIC_REG_EXFC, 3);
  nk_vc_printf("Disabling interrupts through the IER mechansim\n");
  for (int i = 0; i < 8; i++) {
    apic_write(apic, APIC_GET_IER(i), 0);
  }
  // We can typically count on the apic timer interrupt (vec 240/0xF0) to fire first
  // So we will put our special code in Mr. apic_timer_handler()
  wait_for_any_interrupt(apic);

  print_all_irr(apic);

  // Show that the timer interrupt has fired (output should be 00010000)
  // uint32_t this_irr = apic_read(apic, APIC_GET_IRR(7));
  // nk_vc_printf("%08x\n", this_irr);

  // uint8_t bit_pos = __builtin_ctz(this_irr);
  // uint8_t interrupt_vector_index = bit_pos + 32*7;
  // nk_vc_printf("bit_pos: %d\n", bit_pos);
  // nk_vc_printf("sizeof(this_irr)*7: %d\n", 32*7);
  // nk_vc_printf("interrupt_vector_index: %d\nIt should be 240\n", interrupt_vector_index);
}

/******************* Test Handlers *******************/

static int handle_clear_interrupt(char * buf, void * prive) {
  test_clear_interrupt_seoi();
  return 0;
}

static int handle_disable_interrupt(char * buf, void * prive) {
  test_ier_disable_interrupts();
  return 0;
}

static int handle_ier_interrupt(char * buf, void * prive) {
  test_ier_handle_interrupt();
  return 0;
}

static int handle_straight_pmio(char * buf, void * prive) {
  test_straight_PMIO();
  return 0;
}

static int handle_straight_mmio_lapic(char * buf, void * prive) {
  test_straight_MMIOLapic();
  return 0;
}

static int handle_pmio_arithmetic(char * buf, void * prive){
  test_pmio_arithmetic();
  return 0;
}

static int handle_mmio_lapic_arithmetic(char * buf, void * prive){
  test_mmio_lapic_arithmetic();
  return 0;
}

static int handle_pmio_loop(char * buf, void * prive) {
  test_pmio_simple_loop();
  return 0;
}

static int handle_mmio_lapic_loop(char * buf, void * prive) {
  test_mmio_lapic_simple_loop();
  return 0;
}

static int handle_pmio_matrix_mul(char * buf, void * prive) {
  test_pmio_matrix_mul();
  return 0;
}

static int handle_mmio_lapic_matrix_mul(char * buf, void * prive) {
  test_mmio_lapic_matrix_mul();
  return 0;
}

static int handle_pmio(char * buf, void *priv) {
  test_straight_PMIO();
  test_pmio_arithmetic();
  test_pmio_simple_loop();
  test_pmio_matrix_mul();
  return 0;
}

static int handle_mmio_lapic(char * buf, void *priv) {
  test_straight_MMIOLapic();
  test_mmio_lapic_arithmetic();
  test_mmio_lapic_simple_loop();
  test_mmio_lapic_matrix_mul();
  return 0;
}

static int handle_all(char * buf, void *priv) {
  test_straight_PMIO();
  test_pmio_arithmetic();
  test_pmio_simple_loop();
  test_pmio_matrix_mul();
  test_straight_MMIOLapic();
  test_mmio_lapic_arithmetic();
  test_mmio_lapic_simple_loop();
  test_mmio_lapic_matrix_mul();
  return 0;
}

static int handle_stats_lapic(char * buf, void *priv) {
  test_lapic_rdtsc_wrap_loop();
  test_lapic_rdtsc_wrap_loop_asm();

  test_lapic_rdtsc_within_loop();
  test_lapic_rdtsc_within_loop_asm();

  test_lapic_rdtscp_wrap_loop();
  test_lapic_rdtscp_wrap_loop_asm();

  test_lapic_rdtscp_within_loop();
  test_lapic_rdtscp_within_loop_asm();

  return 0;
}

static int handle_stats_port_80(char * buf, void *priv) {
  // test_port_80_rdtsc_wrap_loop();
  test_port_80_rdtsc_wrap_loop_asm();

  // test_port_80_rdtsc_within_loop();
  test_port_80_rdtsc_within_loop_asm();

  // test_port_80_rdtscp_wrap_loop();
  test_port_80_rdtscp_wrap_loop_asm();

  // test_port_80_rdtscp_within_loop();
  test_port_80_rdtscp_within_loop_asm();

  return 0;
}

static int handle_stats_pci_config(char * buf, void *priv) {
  test_pci_config_rdtsc_wrap_loop_asm();
  test_pci_config_rdtsc_within_loop_asm();
  test_pci_config_rdtscp_wrap_loop_asm();
  test_pci_config_rdtscp_within_loop_asm();

  return 0;
}

static int handle_stats_apic_msr(char * buf, void *priv) {
  test_apic_msr_rdtsc_wrap_loop();
  test_apic_msr_rdtsc_within_loop();
  test_apic_msr_rdtscp_wrap_loop();
  test_apic_msr_rdtscp_within_loop();

  return 0;
}

// Don't put any more here
static int handle_stats(char * buf, void *priv) {
  test_port_80_rdtsc_wrap_loop();
  test_port_80_rdtsc_within_loop();
  test_port_80_rdtscp_wrap_loop();
  test_port_80_rdtscp_within_loop();

  test_lapic_rdtsc_wrap_loop();
  test_lapic_rdtsc_within_loop();
  test_lapic_rdtscp_wrap_loop();
  test_lapic_rdtscp_within_loop();

  return 0;
}

static int handle_timing_test(char * buf, void *priv) {
  test_rdtsc_within_loop();
  test_rdtscp_within_loop();

  return 0;
}

static int handle_ier_index_computation_test(char * buf, void *priv) {
  test_ier_index_computation();

  return 0;
}



// static int handle_test_port_80_rdtsc_wrap_loop(char * buf, void *priv) {
//   test_rdtsc_within_loop();
//   test_rdtscp_within_loop();

//   return 0;
// }

  
/******************* Shell Structs ********************/

static struct shell_cmd_impl pmio_straight_test = {
  .cmd      = "pmio_straight_test",
  .help_str = "pmio_arithmetic_test",
  .handler  = handle_straight_pmio,
};

static struct shell_cmd_impl pmio_arithmetic_test = {
  .cmd      = "pmio_arithmetic_test",
  .help_str = "pmio_arithmetic_test",
  .handler  = handle_pmio_arithmetic,
};

static struct shell_cmd_impl pmio_loop_test = {
  .cmd      = "pmio_loop_test",
  .help_str = "pmio_loop_test",
  .handler  = handle_pmio_loop,
};

static struct shell_cmd_impl pmio_matrix_mul_test = {
  .cmd      = "pmio_matrix_mul_test",
  .help_str = "pmio_matrix_mul_test",
  .handler  = handle_pmio_matrix_mul,
};

static struct shell_cmd_impl pmio_all_test = {
  .cmd      = "pmio_all_test",
  .help_str = "pmio_all_test",
  .handler  = handle_pmio,
};

static struct shell_cmd_impl io_all_test = {
  .cmd      = "io_all_test",
  .help_str = "io_all_test",
  .handler  = handle_all,
};

static struct shell_cmd_impl io_stats_port_80_test = {
  .cmd      = "io_stats_port_80_test",
  .help_str = "io_stats_port_80_test",
  .handler  = handle_stats_port_80,
};

static struct shell_cmd_impl io_stats_lapic_test = {
  .cmd      = "io_stats_lapic_test",
  .help_str = "io_stats_lapic_test",
  .handler  = handle_stats_lapic,
};

static struct shell_cmd_impl io_stats_pci_config_test = {
  .cmd      = "io_stats_pci_config_test",
  .help_str = "io_stats_pci_config_test",
  .handler  = handle_stats_pci_config,
};

static struct shell_cmd_impl io_stats_test = {
  .cmd      = "io_stats_test",
  .help_str = "io_stats_test",
  .handler  = handle_stats,
};

static struct shell_cmd_impl timing_test = {
  .cmd      = "timing_test",
  .help_str = "timing_test",
  .handler  = handle_timing_test,
};

static struct shell_cmd_impl io_stats_apic_msr_test = {
  .cmd      = "io_stats_apic_msr_test",
  .help_str = "io_stats_apic_msr_test",
  .handler  = handle_stats_apic_msr,
};

static struct shell_cmd_impl clear_int_test = {
  .cmd      = "clear_int_test",
  .help_str = "clear_int_test",
  .handler  = handle_clear_interrupt,
};

static struct shell_cmd_impl disable_int_test = {
  .cmd      = "disable_int_test",
  .help_str = "disable_int_test",
  .handler  = handle_disable_interrupt,
};

static struct shell_cmd_impl handle_int_test = {
  .cmd      = "handle_int_test",
  .help_str = "handle_int_test",
  .handler  = handle_ier_interrupt,
};

static struct shell_cmd_impl ier_index_computation_test = {
  .cmd      = "ier_index_computation_test",
  .help_str = "ier_index_computation_test",
  .handler  = handle_ier_index_computation_test,
};

/******************* Shell Commands *******************/

nk_register_shell_cmd(pmio_arithmetic_test);
nk_register_shell_cmd(pmio_loop_test);
nk_register_shell_cmd(pmio_matrix_mul_test);
nk_register_shell_cmd(pmio_all_test);
nk_register_shell_cmd(io_all_test);
nk_register_shell_cmd(io_stats_test);
nk_register_shell_cmd(io_stats_port_80_test);
nk_register_shell_cmd(io_stats_lapic_test);
nk_register_shell_cmd(io_stats_pci_config_test);
nk_register_shell_cmd(timing_test);
nk_register_shell_cmd(io_stats_apic_msr_test);
nk_register_shell_cmd(clear_int_test);
nk_register_shell_cmd(disable_int_test);
nk_register_shell_cmd(handle_int_test);
nk_register_shell_cmd(ier_index_computation_test);
