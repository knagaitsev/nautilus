
#ifndef _NAS_MATH_H_
#define _NAS_MATH_H_

#include <nautilus/nautilus.h>
#include "../common.h"

//#define EXIT_FAILURE -1
//#define EXIT_SUCCESS 0

static inline double __math_invalid(double x) {
  return (x - x) / (x - x);
}

/* returns a*b*2^-32 - e, with error 0 <= e < 1.  */
static inline uint32_t mul32(uint32_t a, uint32_t b) { return (uint64_t)a * b >> 32; }

/* returns a*b*2^-64 - e, with error 0 <= e < 3.  */
static inline uint64_t mul64(uint64_t a, uint64_t b) {
	uint64_t ahi = a >> 32;
	uint64_t alo = a & 0xffffffff;
	uint64_t bhi = b >> 32;
	uint64_t blo = b & 0xffffffff;
	return ahi * bhi + (ahi * blo >> 32) + (alo * bhi >> 32);
}


const uint16_t __rsqrt_tab[128] = {
    0xb451,
    0xb2f0,
    0xb196,
    0xb044,
    0xaef9,
    0xadb6,
    0xac79,
    0xab43,
    0xaa14,
    0xa8eb,
    0xa7c8,
    0xa6aa,
    0xa592,
    0xa480,
    0xa373,
    0xa26b,
    0xa168,
    0xa06a,
    0x9f70,
    0x9e7b,
    0x9d8a,
    0x9c9d,
    0x9bb5,
    0x9ad1,
    0x99f0,
    0x9913,
    0x983a,
    0x9765,
    0x9693,
    0x95c4,
    0x94f8,
    0x9430,
    0x936b,
    0x92a9,
    0x91ea,
    0x912e,
    0x9075,
    0x8fbe,
    0x8f0a,
    0x8e59,
    0x8daa,
    0x8cfe,
    0x8c54,
    0x8bac,
    0x8b07,
    0x8a64,
    0x89c4,
    0x8925,
    0x8889,
    0x87ee,
    0x8756,
    0x86c0,
    0x862b,
    0x8599,
    0x8508,
    0x8479,
    0x83ec,
    0x8361,
    0x82d8,
    0x8250,
    0x81c9,
    0x8145,
    0x80c2,
    0x8040,
    0xff02,
    0xfd0e,
    0xfb25,
    0xf947,
    0xf773,
    0xf5aa,
    0xf3ea,
    0xf234,
    0xf087,
    0xeee3,
    0xed47,
    0xebb3,
    0xea27,
    0xe8a3,
    0xe727,
    0xe5b2,
    0xe443,
    0xe2dc,
    0xe17a,
    0xe020,
    0xdecb,
    0xdd7d,
    0xdc34,
    0xdaf1,
    0xd9b3,
    0xd87b,
    0xd748,
    0xd61a,
    0xd4f1,
    0xd3cd,
    0xd2ad,
    0xd192,
    0xd07b,
    0xcf69,
    0xce5b,
    0xcd51,
    0xcc4a,
    0xcb48,
    0xca4a,
    0xc94f,
    0xc858,
    0xc764,
    0xc674,
    0xc587,
    0xc49d,
    0xc3b7,
    0xc2d4,
    0xc1f4,
    0xc116,
    0xc03c,
    0xbf65,
    0xbe90,
    0xbdbe,
    0xbcef,
    0xbc23,
    0xbb59,
    0xba91,
    0xb9cc,
    0xb90a,
    0xb84a,
    0xb78c,
    0xb6d0,
    0xb617,
    0xb560,
};

double sqrt(double x) {
  uint64_t ix, top, m;

	/* special case handling.  */
	ix = (uint64_t)x;
	top = ix >> 52;
	if (unlikely(top - 0x001 >= 0x7ff - 0x001)) {
		/* x < 0x1p-1022 or inf or nan.  */
		if (ix * 2 == 0) return x;
		if (ix == 0x7ff0000000000000) return x;
		if (ix > 0x7ff0000000000000) return __math_invalid(x);
		/* x is subnormal, normalize it.  */
		ix = (uint64_t)(x * 0x1p52);
		top = ix >> 52;
		top -= 52;
	}
	int even = top & 1;
	m = (ix << 11) | 0x8000000000000000;
	if (even) m >>= 1;
	top = (top + 0x3ff) >> 1;
	static const uint64_t three = 0xc0000000;
	uint64_t r, s, d, u, i;

	i = (ix >> 46) % 128;
	r = (uint32_t)__rsqrt_tab[i] << 16;
	/* |r sqrt(m) - 1| < 0x1.fdp-9 */
	s = mul32(m >> 32, r);
	/* |s/sqrt(m) - 1| < 0x1.fdp-9 */
	d = mul32(s, r);
	u = three - d;
	r = mul32(r, u) << 1;
	/* |r sqrt(m) - 1| < 0x1.7bp-16 */
	s = mul32(s, u) << 1;
	/* |s/sqrt(m) - 1| < 0x1.7bp-16 */
	d = mul32(s, r);
	u = three - d;
	r = mul32(r, u) << 1;
	/* |r sqrt(m) - 1| < 0x1.3704p-29 (measured worst-case) */
	r = r << 32;
	s = mul64(m, r);
	d = mul64(s, r);
	u = (three << 32) - d;
	s = mul64(s, u); /* repr: 3.61 */
	/* -0x1p-57 < s - sqrt(m) < 0x1.8001p-61 */
	s = (s - 2) >> 9; /* repr: 12.52 */
	uint64_t d0, d1, d2;
	double y, t;
	d0 = (m << 42) - s * s;
	d1 = s - d0;
	d2 = d1 + s + 1;
	s += d1 >> 63;
	s &= 0x000fffffffffffff;
	s |= top << 52;
	y = (double)(s);
  	return y;
}

#define fabs(x) __builtin_fabs(x)
//#define pow(a,b) ___powl(a,b)

/* #if __HAVE_BUILTIN_TGMATH */

/* #  if __HAVE_FLOAT16 && __GLIBC_USE (IEC_60559_TYPES_EXT) */
/* #   define __TG_F16_ARG(X) X ## f16, */
/* #  else */
/* #   define __TG_F16_ARG(X) */
/* #  endif */
/* #  if __HAVE_FLOAT32 && __GLIBC_USE (IEC_60559_TYPES_EXT) */
/* #   define __TG_F32_ARG(X) X ## f32, */
/* #  else */
/* #   define __TG_F32_ARG(X) */
/* #  endif */
/* #  if __HAVE_FLOAT64 && __GLIBC_USE (IEC_60559_TYPES_EXT) */
/* #   define __TG_F64_ARG(X) X ## f64, */
/* #  else */
/* #   define __TG_F64_ARG(X) */
/* #  endif */
/* #  if __HAVE_FLOAT128 && __GLIBC_USE (IEC_60559_TYPES_EXT) */
/* #   define __TG_F128_ARG(X) X ## f128, */
/* #  else */
/* #   define __TG_F128_ARG(X) */
/* #  endif */
/* #  if __HAVE_FLOAT32X && __GLIBC_USE (IEC_60559_TYPES_EXT) */
/* #   define __TG_F32X_ARG(X) X ## f32x, */
/* #  else */
/* #   define __TG_F32X_ARG(X) */
/* #  endif */
/* #  if __HAVE_FLOAT64X && __GLIBC_USE (IEC_60559_TYPES_EXT) */
/* #   define __TG_F64X_ARG(X) X ## f64x, */
/* #  else */
/* #   define __TG_F64X_ARG(X) */
/* #  endif */
/* #  if __HAVE_FLOAT128X && __GLIBC_USE (IEC_60559_TYPES_EXT) */
/* #   define __TG_F128X_ARG(X) X ## f128x, */
/* #  else */
/* #  define __TG_F128X_ARG(X) */
/* #  endif */
/* #endif  */

/* #define __TGMATH_FUNCS(X) X ## f, X, X ## l,                          \ */
/*     __TG_F16_ARG (X) __TG_F32_ARG (X) __TG_F64_ARG (X) __TG_F128_ARG (X) \ */
/*     __TG_F32X_ARG (X) __TG_F64X_ARG (X) __TG_F128X_ARG (X) */

/* #define fabs(x) __builtin_fabs(x) */
/* #define __TGMATH_RCFUNCS(F, C) __TGMATH_FUNCS (F) __TGMATH_FUNCS (C) */
/* #define __TGMATH_2C(F, C, X, Y) __builtin_tgmath (__TGMATH_RCFUNCS (F, C) \ */
/*                                                     (X), (Y)) */
/* #define __TGMATH_BINARY_REAL_IMAG(Val1, Val2, Fct, Cfct)      \ */
/*   __TGMATH_2C (Fct, Cfct, (Val1), (Val2)) */

/* #define pow(Val1, Val2) __TGMATH_BINARY_REAL_IMAG (Val1, Val2, pow, cpow) */



#endif //COMMON_H
