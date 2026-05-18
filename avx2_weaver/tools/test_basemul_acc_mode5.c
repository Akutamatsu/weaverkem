/* Differential test: polyvec_basemul_acc_montgomery (fused AVX) vs scalar reference. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "params.h"
#include "polyvec.h"
#include "poly.h"
#include "ntt.h"
#include "reduce.h"

#if WEAVER_MODE != 5
#error "test_basemul_acc_mode5 requires WEAVER_MODE=5"
#endif

extern const int16_t zetas[128];

static uint32_t rng = 1;

static int16_t rand16(void)
{
  rng ^= rng << 13;
  rng ^= rng >> 17;
  rng ^= rng << 5;
  return (int16_t)(rng & 0xFFFF);
}

static void fill_polyvec(polyvec *v)
{
  unsigned int i, j;
  for(i = 0; i < KYBER_K; i++)
    for(j = 0; j < KYBER_N; j++)
      v->vec[i].coeffs[j] = rand16();
}

static void ref_polyvec_basemul_acc(int16_t *r, const polyvec *a, const polyvec *b)
{
  unsigned int i, j, k;
  int16_t t[KYBER_N];

  memset(r, 0, KYBER_N * sizeof(int16_t));
  for(i = 0; i < KYBER_K; i++) {
    for(j = 0; j < KYBER_N / 8; j++) {
      basemul_degree4(t + 8 * j, a->vec[i].coeffs + 8 * j, b->vec[i].coeffs + 8 * j,
                      zetas[64 + j]);
      basemul_degree4(t + 8 * j + 4, a->vec[i].coeffs + 8 * j + 4,
                      b->vec[i].coeffs + 8 * j + 4, -zetas[64 + j]);
    }
    for(k = 0; k < KYBER_N; k++)
      r[k] += t[k];
  }
}

static int barrett_diff(int16_t x, int16_t y)
{
  int16_t d = barrett_reduce(x) - barrett_reduce(y);
  if(d > KYBER_Q / 2) d -= KYBER_Q;
  if(d < -KYBER_Q / 2) d += KYBER_Q;
  return d != 0;
}

int main(void)
{
  unsigned int t;
  polyvec a, b;
  poly r_avx;
  int16_t r_ref[KYBER_N];
  int errors = 0;

  for(t = 0; t < 2000; t++) {
    fill_polyvec(&a);
    fill_polyvec(&b);
    ref_polyvec_basemul_acc(r_ref, &a, &b);
    polyvec_basemul_acc_montgomery(&r_avx, &a, &b);
    for(unsigned int i = 0; i < KYBER_N; i++) {
      if(barrett_diff(r_ref[i], r_avx.coeffs[i])) {
        printf("FAIL test %u idx %u ref=%d avx=%d\n",
               t, i, r_ref[i], r_avx.coeffs[i]);
        errors++;
        goto done;
      }
    }
  }

done:
  if(errors == 0)
    printf("PASS polyvec_basemul_acc_montgomery (%u trials, mod q)\n", t);
  return errors != 0;
}
