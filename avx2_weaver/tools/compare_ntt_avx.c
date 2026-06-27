#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "params.h"
#include "ntt.h"
#include "reduce.h"

#if (WEAVER_MODE == 1) && (WEAVER_Q == 3329) && defined(WEAVER_USE_AVX_NTT128)

#include "ntt3329_avx128.h"

#define avx_ntt128 pqcrystals_weaver640_avx2_ntt3329_avx128
#define avx_invntt128 pqcrystals_weaver640_avx2_invntt3329_avx128
#define avx_basemul128 pqcrystals_weaver640_avx2_basemul3329_avx128
#define avx_reduce128 pqcrystals_weaver640_avx2_poly3329_reduce_avx128

#undef WEAVER_NAMESPACE
#define WEAVER_NAMESPACE(s) refcmp##s
#include "reduce.c"
#include "ntt.c"

#define ref_ntt WEAVER_NAMESPACE(_ntt)
#define ref_invntt WEAVER_NAMESPACE(_invntt)
#define ref_barrett_reduce WEAVER_NAMESPACE(_barrett_reduce)

static uint32_t rng_state = 1;

static uint32_t xorshift32(void)
{
  uint32_t x = rng_state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  rng_state = x;
  return x;
}

static int first_diff(const int16_t *a, const int16_t *b, size_t n)
{
  size_t i;
  for(i = 0; i < n; i++) {
    if(a[i] != b[i])
      return (int)i;
  }
  return -1;
}

static int modq(int16_t x)
{
  int v = (int)x % WEAVER_Q;
  if(v < 0)
    v += WEAVER_Q;
  return v;
}

static int first_diff_modq(const int16_t *a, const int16_t *b, size_t n)
{
  size_t i;
  for(i = 0; i < n; i++) {
    if(modq(a[i]) != modq(b[i]))
      return (int)i;
  }
  return -1;
}

static void fill_poly(int16_t *r)
{
  size_t i;
  for(i = 0; i < WEAVER_N; i++)
    r[i] = (int16_t)(xorshift32() % WEAVER_Q);
}

int main(void)
{
  unsigned t;
  int idx;
  int errors = 0;
  int16_t a_ref[WEAVER_N] __attribute__((aligned(32)));
  int16_t a_avx[WEAVER_N] __attribute__((aligned(32)));
  int16_t b_ref[WEAVER_N] __attribute__((aligned(32)));
  int16_t b_avx[WEAVER_N] __attribute__((aligned(32)));
  int16_t r_ref[WEAVER_N] __attribute__((aligned(32)));
  int16_t r_avx[WEAVER_N] __attribute__((aligned(32)));

  printf("compare_ntt_avx: mode=%d n=%d q=%d (q3329 n128 AVX2)\n",
         WEAVER_MODE, WEAVER_N, WEAVER_Q);

  for(t = 0; t < 2000; t++) {
    fill_poly(a_ref);
    memcpy(a_avx, a_ref, sizeof(a_ref));

    ref_ntt(a_ref);
    avx_ntt128(a_avx);
    idx = first_diff(a_ref, a_avx, WEAVER_N);
    if(idx >= 0) {
      printf("FAIL ntt at test %u idx %d: ref=%d avx=%d\n", t, idx, a_ref[idx], a_avx[idx]);
      errors++;
      break;
    }
  }
  if(errors == 0)
    printf("PASS ntt (exact)\n");

  for(t = 0; t < 2000; t++) {
    fill_poly(a_ref);
    memcpy(a_avx, a_ref, sizeof(a_ref));

    ref_ntt(a_ref);
    ref_invntt(a_ref);
    avx_ntt128(a_avx);
    avx_invntt128(a_avx);
    idx = first_diff_modq(a_ref, a_avx, WEAVER_N);
    if(idx >= 0) {
      printf("FAIL invntt at test %u idx %d: ref=%d avx=%d\n", t, idx, a_ref[idx], a_avx[idx]);
      errors++;
      break;
    }
  }
  if(errors == 0)
    printf("PASS invntt roundtrip (mod q)\n");

  for(t = 0; t < 1000; t++) {
    fill_poly(a_ref);
    fill_poly(b_ref);
    memcpy(a_avx, a_ref, sizeof(a_ref));
    memcpy(b_avx, b_ref, sizeof(b_ref));

    ref_ntt(a_ref);
    ref_ntt(b_ref);
    for(unsigned i = 0; i < WEAVER_N; i++)
      r_ref[i] = montgomery_reduce((int32_t)a_ref[i] * b_ref[i]);
    ref_invntt(r_ref);

    avx_ntt128(a_avx);
    avx_ntt128(b_avx);
    avx_basemul128(r_avx, a_avx, b_avx);
    avx_invntt128(r_avx);

    idx = first_diff_modq(r_ref, r_avx, WEAVER_N);
    if(idx >= 0) {
      printf("FAIL full_mul at test %u idx %d: ref=%d avx=%d\n",
             t, idx, r_ref[idx], r_avx[idx]);
      errors++;
      break;
    }
  }
  if(errors == 0)
    printf("PASS full_mul (mod q)\n");

  if(errors == 0) {
    printf("PASS all q3329 n128 AVX2 NTT checks\n");
    return 0;
  }

  printf("FAIL: %d error(s)\n", errors);
  return 1;
}

#elif (WEAVER_MODE == 3 || WEAVER_MODE == 5) && WEAVER_Q == 7681

#include "ntt7681_avx.h"

#if WEAVER_MODE == 3
#define avx_ntt7681 pqcrystals_weaver1024_avx2_ntt7681_avx
#define avx_invntt7681 pqcrystals_weaver1024_avx2_invntt7681_avx
#define avx_basemul7681 pqcrystals_weaver1024_avx2_basemul7681_avx
#else
#define avx_ntt7681 pqcrystals_weaver2048_avx2_ntt7681_avx
#define avx_invntt7681 pqcrystals_weaver2048_avx2_invntt7681_avx
#define avx_basemul7681 pqcrystals_weaver2048_avx2_basemul7681_avx
#endif

#undef WEAVER_NAMESPACE
#define WEAVER_NAMESPACE(s) refcmp##s
#include "reduce.c"
#include "ntt.c"

#define ref_ntt WEAVER_NAMESPACE(_ntt)
#define ref_invntt WEAVER_NAMESPACE(_invntt)
#define ref_basemul WEAVER_NAMESPACE(_basemul)
#define ref_barrett_reduce WEAVER_NAMESPACE(_barrett_reduce)

static uint32_t rng_state = 1;

static uint32_t xorshift32(void)
{
  uint32_t x = rng_state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  rng_state = x;
  return x;
}

static int first_diff(const int16_t *a, const int16_t *b, size_t n)
{
  size_t i;
  for(i = 0; i < n; i++) {
    if(a[i] != b[i])
      return (int)i;
  }
  return -1;
}

static int modq(int16_t x)
{
  int v = (int)x % WEAVER_Q;
  if(v < 0)
    v += WEAVER_Q;
  return v;
}

static int first_diff_modq(const int16_t *a, const int16_t *b, size_t n)
{
  size_t i;
  for(i = 0; i < n; i++) {
    if(modq(a[i]) != modq(b[i]))
      return (int)i;
  }
  return -1;
}

static void fill_poly(int16_t *r)
{
  size_t i;
  for(i = 0; i < WEAVER_N; i++)
    r[i] = (int16_t)(xorshift32() % WEAVER_Q);
}

static void poly_reduce_ref(int16_t r[WEAVER_N])
{
  unsigned i;
  for(i = 0; i < WEAVER_N; i++)
    r[i] = ref_barrett_reduce(r[i]);
}

int main(void)
{
  unsigned t;
  int idx;
  int errors = 0;
  int16_t a_ref[WEAVER_N] __attribute__((aligned(32)));
  int16_t a_avx[WEAVER_N] __attribute__((aligned(32)));
  int16_t b_ref[WEAVER_N] __attribute__((aligned(32)));
  int16_t b_avx[WEAVER_N] __attribute__((aligned(32)));
  int16_t r_ref[WEAVER_N] __attribute__((aligned(32)));
  int16_t r_avx[WEAVER_N] __attribute__((aligned(32)));

  printf("compare_ntt_avx: mode=%d n=%d q=%d (q7681 AVX2 Route A)\n",
         WEAVER_MODE, WEAVER_N, WEAVER_Q);

  for(t = 0; t < 2000; t++) {
    fill_poly(a_ref);
    memcpy(a_avx, a_ref, sizeof(a_ref));

    ref_ntt(a_ref);
    avx_ntt7681(a_avx);
    idx = first_diff(a_ref, a_avx, WEAVER_N);
    if(idx >= 0) {
      printf("FAIL ntt at test %u idx %d: ref=%d avx=%d\n", t, idx, a_ref[idx], a_avx[idx]);
      errors++;
      break;
    }
  }
  if(errors == 0)
    printf("PASS ntt (exact)\n");

  for(t = 0; t < 2000; t++) {
    fill_poly(a_ref);
    memcpy(a_avx, a_ref, sizeof(a_ref));

    ref_ntt(a_ref);
    ref_invntt(a_ref);
    avx_ntt7681(a_avx);
    avx_invntt7681(a_avx);
    idx = first_diff_modq(a_ref, a_avx, WEAVER_N);
    if(idx >= 0) {
      printf("FAIL invntt at test %u idx %d: ref=%d avx=%d\n", t, idx, a_ref[idx], a_avx[idx]);
      errors++;
      break;
    }
  }
  if(errors == 0)
    printf("PASS invntt roundtrip (mod q)\n");

  for(t = 0; t < 1000; t++) {
    fill_poly(a_ref);
    fill_poly(b_ref);
    memcpy(a_avx, a_ref, sizeof(a_ref));
    memcpy(b_avx, b_ref, sizeof(b_ref));

    ref_ntt(a_ref);
    ref_ntt(b_ref);
#if WEAVER_N == 512
    for(unsigned i = 0; i < WEAVER_N / 4; i++) {
      ref_basemul(&r_ref[4 * i], &a_ref[4 * i], &b_ref[4 * i], zetas[128 + i]);
      ref_basemul(&r_ref[4 * i + 2], &a_ref[4 * i + 2], &b_ref[4 * i + 2], (int16_t)-zetas[128 + i]);
    }
#else
    for(unsigned i = 0; i < WEAVER_N; i++)
      r_ref[i] = montgomery_reduce((int32_t)a_ref[i] * b_ref[i]);
#endif
    ref_invntt(r_ref);

    avx_ntt7681(a_avx);
    avx_ntt7681(b_avx);
    avx_basemul7681(r_avx, a_avx, b_avx);
    avx_invntt7681(r_avx);

    idx = first_diff_modq(r_ref, r_avx, WEAVER_N);
    if(idx >= 0) {
      printf("FAIL full_mul at test %u idx %d: ref=%d avx=%d\n",
             t, idx, r_ref[idx], r_avx[idx]);
      errors++;
      break;
    }
  }
  if(errors == 0)
    printf("PASS full_mul (mod q)\n");

  if(errors == 0) {
    printf("PASS all q7681 AVX2 NTT checks\n");
    return 0;
  }

  printf("FAIL: %d error(s)\n", errors);
  return 1;
}

#else
#error "compare_ntt_avx requires WEAVER_USE_AVX_NTT128 (mode 1) or q=7681 AVX NTT (modes 3/5)"
#endif
