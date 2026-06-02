/* Differential test: 9-bit PK compress scalar vs AVX (mode 1 / n=256). */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "params.h"
#include "poly.h"
#include "poly_compress9.h"

#if WEAVER_MODE != 1
#error "test_compress9 requires WEAVER_MODE=1"
#endif

static int test_exhaustive_quant(void)
{
  int16_t x;
  uint16_t t_ref, t_avx[WEAVER_N];
  poly a;

  for(x = 0; x < WEAVER_Q; x++) {
    t_ref = poly_compress9_coeff_scalar(x);
    memset(&a, 0, sizeof(a));
    a.coeffs[0] = x;
    poly_compress9_quant_avx(t_avx, &a);
    if(t_ref != t_avx[0]) {
      fprintf(stderr, "exhaustive quant fail x=%d ref=%u avx=%u\n",
              (int)x, (unsigned)t_ref, (unsigned)t_avx[0]);
      return 1;
    }
  }

  for(x = -WEAVER_Q; x < 0; x++) {
    t_ref = poly_compress9_coeff_scalar(x);
    memset(&a, 0, sizeof(a));
    a.coeffs[0] = x;
    poly_compress9_quant_avx(t_avx, &a);
    if(t_ref != t_avx[0]) {
      fprintf(stderr, "exhaustive quant fail x=%d ref=%u avx=%u\n",
              (int)x, (unsigned)t_ref, (unsigned)t_avx[0]);
      return 1;
    }
  }
  return 0;
}

static int test_random_poly_compress(void)
{
  unsigned int trial;
  uint8_t r_ref[(WEAVER_N * 9) / 8];
  uint8_t r_avx[(WEAVER_N * 9) / 8];
  poly a;

  for(trial = 0; trial < 5000; trial++) {
    unsigned int i;
    for(i = 0; i < WEAVER_N; i++)
      a.coeffs[i] = (int16_t)((trial * 17 + i * 91) % (2 * WEAVER_Q) - WEAVER_Q);
    poly_compress9_scalar(r_ref, &a);
    poly_compress9_avx(r_avx, &a);
    if(memcmp(r_ref, r_avx, sizeof(r_ref)) != 0) {
      fprintf(stderr, "random compress mismatch trial %u\n", trial);
      return 1;
    }
  }
  return 0;
}

int main(void)
{
  if(test_exhaustive_quant() != 0)
    return 1;
  if(test_random_poly_compress() != 0)
    return 1;
  printf("PASS: 9-bit compress scalar == AVX (exhaustive Q + 5000 random polys)\n");
  return 0;
}
