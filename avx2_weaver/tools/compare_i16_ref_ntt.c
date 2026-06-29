/*
 * KAT gate: i16 negacyclic NTT must match ref/ntt.c coefficient-exact before
 * any USE_AVX_NTT7681_I16 integration into the KEM.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "params.h"
#include "ntt.h"
#include "reduce.h"
#include "ntt7681_i16_avx.h"

#undef WEAVER_NAMESPACE
#define WEAVER_NAMESPACE(s) i16cmp##s
#include "reduce.c"
#include "ntt.c"

#define ref_ntt WEAVER_NAMESPACE(_ntt)
#define ref_invntt WEAVER_NAMESPACE(_invntt)
#define ref_basemul WEAVER_NAMESPACE(_basemul)

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

static void int16_to_u16(const int16_t *in, uint16_t *out, size_t n)
{
  size_t i;
  for(i = 0; i < n; i++) {
    int32_t v = in[i];
    if(v < 0)
      v += WEAVER_Q;
    out[i] = (uint16_t)v;
  }
}

static void u16_to_int16(const uint16_t *in, int16_t *out, size_t n)
{
  size_t i;
  for(i = 0; i < n; i++) {
    int32_t v = in[i];
    if(v > WEAVER_Q / 2)
      v -= WEAVER_Q;
    out[i] = (int16_t)v;
  }
}

int main(void)
{
  unsigned t;
  int idx;
  int errors = 0;
  int ntt_exact_fail = 0;
  int ntt_modq_fail = 0;
  int16_t a_ref[WEAVER_N] __attribute__((aligned(32)));
  int16_t a_i16[WEAVER_N] __attribute__((aligned(32)));
  int16_t b_ref[WEAVER_N] __attribute__((aligned(32)));
  int16_t b_i16[WEAVER_N] __attribute__((aligned(32)));
  int16_t r_ref[WEAVER_N] __attribute__((aligned(32)));
  int16_t r_i16[WEAVER_N] __attribute__((aligned(32)));
  uint16_t ua[WEAVER_N] __attribute__((aligned(32)));
  uint16_t ub[WEAVER_N] __attribute__((aligned(32)));
  uint16_t ur[WEAVER_N] __attribute__((aligned(32)));

  if(WEAVER_N != NTT7681_I16_N || WEAVER_Q != 7681) {
    printf("compare_i16_ref: only mode 3 (n=256 q=7681) supported\n");
    return 1;
  }

  ntt7681_i16_init();

  printf("compare_i16_ref_ntt: mode=%d n=%d q=%d (i16 vs ref/ntt.c)\n",
         WEAVER_MODE, WEAVER_N, WEAVER_Q);
  printf("KAT gate: forward NTT must be coefficient-exact with ref for integration.\n\n");

  for(t = 0; t < 2000; t++) {
    fill_poly(a_ref);
    memcpy(a_i16, a_ref, sizeof(a_ref));

    ref_ntt(a_ref);
    int16_to_u16(a_i16, ua, WEAVER_N);
    ntt7681_i16_ntt(ua);
    u16_to_int16(ua, a_i16, WEAVER_N);

    idx = first_diff(a_ref, a_i16, WEAVER_N);
    if(idx >= 0) {
      if(ntt_exact_fail == 0) {
        printf("FAIL ntt exact at test %u idx %d: ref=%d i16=%d (modq ref=%d i16=%d)\n",
               t, idx, a_ref[idx], a_i16[idx], modq(a_ref[idx]), modq(a_i16[idx]));
      }
      ntt_exact_fail++;
      if(ntt_modq_fail == 0 &&
         modq(a_ref[idx]) != modq(a_i16[idx]))
        ntt_modq_fail++;
      if(ntt_exact_fail >= 5)
        break;
    }
  }

  if(ntt_exact_fail == 0)
    printf("PASS ntt (exact) — i16 matches ref; safe to wire into KEM\n");
  else {
    printf("BLOCK ntt (exact): %d mismatch(es) in 2000 trials — do NOT integrate i16 into KEM\n",
           ntt_exact_fail);
    errors++;
  }

  for(t = 0; t < 2000; t++) {
    fill_poly(a_ref);
    memcpy(a_i16, a_ref, sizeof(a_ref));

    ref_ntt(a_ref);
    ref_invntt(a_ref);
    int16_to_u16(a_i16, ua, WEAVER_N);
    ntt7681_i16_ntt(ua);
    ntt7681_i16_invntt(ua);
    u16_to_int16(ua, a_i16, WEAVER_N);

    idx = first_diff_modq(a_ref, a_i16, WEAVER_N);
    if(idx >= 0) {
      printf("FAIL invntt roundtrip mod q at test %u idx %d: ref=%d i16=%d\n",
             t, idx, a_ref[idx], a_i16[idx]);
      errors++;
      break;
    }
  }
  if(errors == 0 || ntt_exact_fail > 0)
    printf("%s invntt roundtrip (mod q)\n",
           errors ? "FAIL" : "PASS");

  for(t = 0; t < 1000; t++) {
    fill_poly(a_ref);
    fill_poly(b_ref);
    memcpy(a_i16, a_ref, sizeof(a_ref));
    memcpy(b_i16, b_ref, sizeof(b_ref));

    ref_ntt(a_ref);
    ref_ntt(b_ref);
    for(unsigned i = 0; i < WEAVER_N; i++)
      r_ref[i] = montgomery_reduce((int32_t)a_ref[i] * b_ref[i]);
    ref_invntt(r_ref);

    int16_to_u16(a_i16, ua, WEAVER_N);
    int16_to_u16(b_i16, ub, WEAVER_N);
    ntt7681_i16_ntt(ua);
    ntt7681_i16_ntt(ub);
    ntt7681_i16_basemul(ur, ua, ub);
    ntt7681_i16_invntt(ur);
    u16_to_int16(ur, r_i16, WEAVER_N);

    idx = first_diff_modq(r_ref, r_i16, WEAVER_N);
    if(idx >= 0) {
      printf("FAIL full_mul mod q at test %u idx %d: ref=%d i16=%d\n",
             t, idx, r_ref[idx], r_i16[idx]);
      errors++;
      break;
    }
  }
  if(errors == 0 || ntt_exact_fail > 0)
    printf("%s full_mul (mod q)\n",
           errors && ntt_exact_fail == 0 ? "FAIL" : (ntt_exact_fail ? "SKIP" : "PASS"));

  if(ntt_exact_fail > 0) {
    printf("\nVerdict: i16 path blocked for KEM (use Route A optimizations instead).\n");
    return 1;
  }

  if(errors == 0) {
    printf("\nPASS all i16 vs ref checks — integration may proceed\n");
    return 0;
  }

  printf("\nFAIL: %d error(s)\n", errors);
  return 1;
}
