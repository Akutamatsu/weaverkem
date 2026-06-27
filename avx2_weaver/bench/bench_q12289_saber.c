/*
 * Standalone Section-5-style experiment:
 *   - q=12289, n=256 full negacyclic NTT baseline
 *   - Saber-style exact negacyclic multiplication modulo 2^13
 *
 * This file intentionally does not modify the main Weaver/Kyber code path.
 * It provides a controlled, reproducible baseline for the q=12289 and
 * power-of-two multiplication questions.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <immintrin.h>

#include "cpucycles.h"
#include "speed_print.h"

#ifndef NTESTS
#define NTESTS 1000
#endif

#define N 256
#define Q12289 12289
#define SABER_Q 8192

static uint64_t t[NTESTS];
static volatile uint64_t sink;

static int32_t q12289_psi;
static int32_t q12289_omega;
static int32_t q12289_psi_inv;
static int32_t q12289_omega_inv;
static int32_t q12289_n_inv;
static int32_t q12289_twist[N] __attribute__((aligned(32)));
static int32_t q12289_untwist[N] __attribute__((aligned(32)));
static int32_t q12289_stage_twiddles[8][N / 2] __attribute__((aligned(32)));
static int32_t q12289_stage_twiddles_inv[8][N / 2] __attribute__((aligned(32)));

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

static int32_t mod_q(int64_t a, int32_t q)
{
  a %= q;
  if(a < 0)
    a += q;
  return (int32_t)a;
}

static int32_t reduce_q12289_u32(uint32_t a)
{
  int32_t r;
  uint32_t t;

  /* floor(2^32 / 12289), valid for the products used here (< q^2). */
  t = (uint32_t)(((uint64_t)a * 349496U) >> 32);
  r = (int32_t)(a - t * Q12289);

  if(r >= Q12289)
    r -= Q12289;
  if(r >= Q12289)
    r -= Q12289;
  if(r < 0)
    r += Q12289;

  return r;
}

static int32_t mul_q12289(int32_t a, int32_t b)
{
  return reduce_q12289_u32((uint32_t)(a * b));
}

static __m256i reduce_q12289_x8(__m256i a)
{
  const __m256i barrett = _mm256_set1_epi32(349496);
  const __m256i q = _mm256_set1_epi32(Q12289);
  const __m256i q_minus_1 = _mm256_set1_epi32(Q12289 - 1);
  __m256i even_prod, odd_prod, t, tq, r, mask;

  even_prod = _mm256_mul_epu32(a, barrett);
  odd_prod = _mm256_mul_epu32(_mm256_srli_epi64(a, 32), barrett);
  t = _mm256_or_si256(_mm256_srli_epi64(even_prod, 32),
                      _mm256_slli_epi64(_mm256_srli_epi64(odd_prod, 32), 32));

  tq = _mm256_mullo_epi32(t, q);
  r = _mm256_sub_epi32(a, tq);

  mask = _mm256_cmpgt_epi32(r, q_minus_1);
  r = _mm256_sub_epi32(r, _mm256_and_si256(mask, q));
  mask = _mm256_cmpgt_epi32(r, q_minus_1);
  r = _mm256_sub_epi32(r, _mm256_and_si256(mask, q));

  return r;
}

static __m256i mul_q12289_x8(__m256i a, __m256i b)
{
  return reduce_q12289_x8(_mm256_mullo_epi32(a, b));
}

static __m256i add_q12289_x8(__m256i a, __m256i b)
{
  const __m256i q = _mm256_set1_epi32(Q12289);
  const __m256i q_minus_1 = _mm256_set1_epi32(Q12289 - 1);
  __m256i r = _mm256_add_epi32(a, b);
  __m256i mask = _mm256_cmpgt_epi32(r, q_minus_1);

  return _mm256_sub_epi32(r, _mm256_and_si256(mask, q));
}

static __m256i sub_q12289_x8(__m256i a, __m256i b)
{
  const __m256i q = _mm256_set1_epi32(Q12289);
  const __m256i zero = _mm256_setzero_si256();
  __m256i r = _mm256_sub_epi32(a, b);
  __m256i mask = _mm256_cmpgt_epi32(zero, r);

  return _mm256_add_epi32(r, _mm256_and_si256(mask, q));
}

static int32_t pow_mod(int32_t a, int32_t e, int32_t q)
{
  int64_t r = 1;
  int64_t b = mod_q(a, q);

  while(e > 0) {
    if(e & 1)
      r = (r * b) % q;
    b = (b * b) % q;
    e >>= 1;
  }

  return (int32_t)r;
}

static int32_t inv_mod(int32_t a, int32_t q)
{
  return pow_mod(a, q - 2, q);
}

static int is_primitive_root(int32_t g, int32_t q)
{
  /* q - 1 = 2^12 * 3 for q=12289. */
  return pow_mod(g, (q - 1) / 2, q) != 1 &&
         pow_mod(g, (q - 1) / 3, q) != 1;
}

static int32_t find_primitive_root(int32_t q)
{
  int32_t g;

  for(g = 2; g < q; g++)
    if(is_primitive_root(g, q))
      return g;

  return 0;
}

static void bit_reverse(int32_t a[N])
{
  unsigned i, j, bit;

  j = 0;
  for(i = 1; i < N; i++) {
    bit = N >> 1;
    while(j & bit) {
      j ^= bit;
      bit >>= 1;
    }
    j ^= bit;

    if(i < j) {
      int32_t tmp = a[i];
      a[i] = a[j];
      a[j] = tmp;
    }
  }
}

static void ntt_cyclic_q12289(int32_t a[N], int inverse)
{
  unsigned len, i, j;
  int32_t root = inverse ? q12289_omega_inv : q12289_omega;

  bit_reverse(a);

  for(len = 2; len <= N; len <<= 1) {
    int32_t wlen = pow_mod(root, N / len, Q12289);

    for(i = 0; i < N; i += len) {
      int32_t w = 1;

      for(j = 0; j < len / 2; j++) {
        int32_t u = a[i + j];
        int32_t v = mul_q12289(a[i + j + len / 2], w);

        a[i + j] = u + v;
        if(a[i + j] >= Q12289)
          a[i + j] -= Q12289;

        a[i + j + len / 2] = u - v;
        if(a[i + j + len / 2] < 0)
          a[i + j + len / 2] += Q12289;

        w = mul_q12289(w, wlen);
      }
    }
  }

  if(inverse) {
    for(i = 0; i < N; i++)
    a[i] = mul_q12289(a[i], q12289_n_inv);
  }
}

static void q12289_forward_negacyclic(int32_t a[N])
{
  unsigned i;

  for(i = 0; i < N; i++)
    a[i] = mul_q12289(a[i], q12289_twist[i]);

  ntt_cyclic_q12289(a, 0);
}

static void q12289_inverse_negacyclic(int32_t a[N])
{
  unsigned i;

  ntt_cyclic_q12289(a, 1);

  for(i = 0; i < N; i++)
    a[i] = mul_q12289(a[i], q12289_untwist[i]);
}

static void ntt_cyclic_q12289_avx2(int32_t a[N], int inverse)
{
  unsigned len, i, j, stage;

  bit_reverse(a);

  for(len = 2, stage = 0; len <= N; len <<= 1, stage++) {
    const int32_t *twiddles = inverse ? q12289_stage_twiddles_inv[stage]
                                      : q12289_stage_twiddles[stage];

    for(i = 0; i < N; i += len) {
      unsigned half = len / 2;

      for(j = 0; j + 8 <= half; j += 8) {
        __m256i u = _mm256_loadu_si256((const __m256i *)&a[i + j]);
        __m256i b = _mm256_loadu_si256((const __m256i *)&a[i + j + half]);
        __m256i w = _mm256_load_si256((const __m256i *)&twiddles[j]);
        __m256i v = mul_q12289_x8(b, w);

        _mm256_storeu_si256((__m256i *)&a[i + j], add_q12289_x8(u, v));
        _mm256_storeu_si256((__m256i *)&a[i + j + half], sub_q12289_x8(u, v));
      }

      for(; j < half; j++) {
        int32_t u = a[i + j];
        int32_t v = mul_q12289(a[i + j + half], twiddles[j]);

        a[i + j] = u + v;
        if(a[i + j] >= Q12289)
          a[i + j] -= Q12289;

        a[i + j + half] = u - v;
        if(a[i + j + half] < 0)
          a[i + j + half] += Q12289;
      }
    }
  }

  if(inverse) {
    __m256i ninv = _mm256_set1_epi32(q12289_n_inv);

    for(i = 0; i < N; i += 8) {
      __m256i x = _mm256_loadu_si256((const __m256i *)&a[i]);
      x = mul_q12289_x8(x, ninv);
      _mm256_storeu_si256((__m256i *)&a[i], x);
    }
  }
}

static void q12289_forward_negacyclic_avx2(int32_t a[N])
{
  unsigned i;

  for(i = 0; i < N; i += 8) {
    __m256i x = _mm256_loadu_si256((const __m256i *)&a[i]);
    __m256i tw = _mm256_load_si256((const __m256i *)&q12289_twist[i]);
    x = mul_q12289_x8(x, tw);
    _mm256_storeu_si256((__m256i *)&a[i], x);
  }

  ntt_cyclic_q12289_avx2(a, 0);
}

static void q12289_inverse_negacyclic_avx2(int32_t a[N])
{
  unsigned i;

  ntt_cyclic_q12289_avx2(a, 1);

  for(i = 0; i < N; i += 8) {
    __m256i x = _mm256_loadu_si256((const __m256i *)&a[i]);
    __m256i tw = _mm256_load_si256((const __m256i *)&q12289_untwist[i]);
    x = mul_q12289_x8(x, tw);
    _mm256_storeu_si256((__m256i *)&a[i], x);
  }
}

static void ntt_cyclic_q12289_dif_avx2_from(int32_t a[N], unsigned first_len)
{
  unsigned len, i, j;

  for(len = first_len; len >= 1; len >>= 1) {
    const int32_t *twiddles = q12289_stage_twiddles[__builtin_ctz(len * 2) - 1];

    for(i = 0; i < N; i += 2 * len) {
      for(j = 0; j + 8 <= len; j += 8) {
        __m256i u = _mm256_loadu_si256((const __m256i *)&a[i + j]);
        __m256i v = _mm256_loadu_si256((const __m256i *)&a[i + j + len]);
        __m256i w = _mm256_load_si256((const __m256i *)&twiddles[j]);
        __m256i sum = add_q12289_x8(u, v);
        __m256i diff = sub_q12289_x8(u, v);

        _mm256_storeu_si256((__m256i *)&a[i + j], sum);
        _mm256_storeu_si256((__m256i *)&a[i + j + len], mul_q12289_x8(diff, w));
      }

      for(; j < len; j++) {
        int32_t u = a[i + j];
        int32_t v = a[i + j + len];
        int32_t sum = u + v;
        int32_t diff = u - v;

        if(sum >= Q12289)
          sum -= Q12289;
        if(diff < 0)
          diff += Q12289;

        a[i + j] = sum;
        a[i + j + len] = mul_q12289(diff, twiddles[j]);
      }
    }
  }
}

static void intt_cyclic_q12289_dit_avx2_until(int32_t a[N], unsigned last_len)
{
  unsigned len, i, j;

  for(len = 1; len <= last_len; len <<= 1) {
    const int32_t *twiddles = q12289_stage_twiddles_inv[__builtin_ctz(len * 2) - 1];

    for(i = 0; i < N; i += 2 * len) {
      for(j = 0; j + 8 <= len; j += 8) {
        __m256i u = _mm256_loadu_si256((const __m256i *)&a[i + j]);
        __m256i v = _mm256_loadu_si256((const __m256i *)&a[i + j + len]);
        __m256i w = _mm256_load_si256((const __m256i *)&twiddles[j]);

        v = mul_q12289_x8(v, w);
        _mm256_storeu_si256((__m256i *)&a[i + j], add_q12289_x8(u, v));
        _mm256_storeu_si256((__m256i *)&a[i + j + len], sub_q12289_x8(u, v));
      }

      for(; j < len; j++) {
        int32_t u = a[i + j];
        int32_t v = mul_q12289(a[i + j + len], twiddles[j]);

        a[i + j] = u + v;
        if(a[i + j] >= Q12289)
          a[i + j] -= Q12289;

        a[i + j + len] = u - v;
        if(a[i + j + len] < 0)
          a[i + j + len] += Q12289;
      }
    }
  }
}

static void intt_cyclic_q12289_dit_avx2(int32_t a[N])
{
  unsigned i;
  __m256i ninv = _mm256_set1_epi32(q12289_n_inv);

  intt_cyclic_q12289_dit_avx2_until(a, N / 2);

  for(i = 0; i < N; i += 8) {
    __m256i x = _mm256_loadu_si256((const __m256i *)&a[i]);
    x = mul_q12289_x8(x, ninv);
    _mm256_storeu_si256((__m256i *)&a[i], x);
  }
}

static void q12289_forward_negacyclic_avx2_nobr(int32_t a[N])
{
  unsigned i;

  for(i = 0; i < N; i += 8) {
    __m256i x = _mm256_loadu_si256((const __m256i *)&a[i]);
    __m256i tw = _mm256_load_si256((const __m256i *)&q12289_twist[i]);
    x = mul_q12289_x8(x, tw);
    _mm256_storeu_si256((__m256i *)&a[i], x);
  }

  ntt_cyclic_q12289_dif_avx2_from(a, N / 2);
}

static void q12289_inverse_negacyclic_avx2_nobr(int32_t a[N])
{
  unsigned i;

  intt_cyclic_q12289_dit_avx2(a);

  for(i = 0; i < N; i += 8) {
    __m256i x = _mm256_loadu_si256((const __m256i *)&a[i]);
    __m256i tw = _mm256_load_si256((const __m256i *)&q12289_untwist[i]);
    x = mul_q12289_x8(x, tw);
    _mm256_storeu_si256((__m256i *)&a[i], x);
  }
}

static void q12289_basemul(int32_t r[N], const int32_t a[N], const int32_t b[N])
{
  unsigned i;

  for(i = 0; i < N; i++)
    r[i] = mul_q12289(a[i], b[i]);
}

static void q12289_basemul_avx2(int32_t r[N], const int32_t a[N], const int32_t b[N])
{
  unsigned i;

  for(i = 0; i < N; i += 8) {
    __m256i va = _mm256_loadu_si256((const __m256i *)&a[i]);
    __m256i vb = _mm256_loadu_si256((const __m256i *)&b[i]);
    __m256i vc = mul_q12289_x8(va, vb);
    _mm256_storeu_si256((__m256i *)&r[i], vc);
  }
}

static void q12289_schoolbook(int32_t r[N], const int32_t a[N], const int32_t b[N])
{
  unsigned i, j;
  int64_t acc[2 * N - 1];

  memset(acc, 0, sizeof(acc));

  for(i = 0; i < N; i++)
    for(j = 0; j < N; j++)
      acc[i + j] += (int64_t)a[i] * b[j];

  for(i = 0; i < N; i++) {
    int64_t high = (i + N < 2 * N - 1) ? acc[i + N] : 0;
    r[i] = mod_q(acc[i] - high, Q12289);
  }
}

static void q12289_poly_mul_ntt(int32_t r[N], const int32_t a[N], const int32_t b[N])
{
  int32_t ta[N], tb[N];

  memcpy(ta, a, sizeof(ta));
  memcpy(tb, b, sizeof(tb));

  q12289_forward_negacyclic(ta);
  q12289_forward_negacyclic(tb);
  q12289_basemul(r, ta, tb);
  q12289_inverse_negacyclic(r);
}

static void q12289_poly_mul_ntt_avx2(int32_t r[N], const int32_t a[N], const int32_t b[N])
{
  int32_t ta[N] __attribute__((aligned(32)));
  int32_t tb[N] __attribute__((aligned(32)));

  memcpy(ta, a, sizeof(ta));
  memcpy(tb, b, sizeof(tb));

  q12289_forward_negacyclic_avx2(ta);
  q12289_forward_negacyclic_avx2(tb);
  q12289_basemul_avx2(r, ta, tb);
  q12289_inverse_negacyclic_avx2(r);
}

static void q12289_poly_mul_ntt_avx2_nobr(int32_t r[N], const int32_t a[N], const int32_t b[N])
{
  int32_t ta[N] __attribute__((aligned(32)));
  int32_t tb[N] __attribute__((aligned(32)));

  memcpy(ta, a, sizeof(ta));
  memcpy(tb, b, sizeof(tb));

  q12289_forward_negacyclic_avx2_nobr(ta);
  q12289_forward_negacyclic_avx2_nobr(tb);
  q12289_basemul_avx2(r, ta, tb);
  q12289_inverse_negacyclic_avx2_nobr(r);
}

static void saber_schoolbook_exact(uint16_t r[N], const uint16_t a[N], const uint16_t b[N])
{
  unsigned i, j;
  int32_t acc[2 * N - 1];

  memset(acc, 0, sizeof(acc));

  for(i = 0; i < N; i++)
    for(j = 0; j < N; j++)
      acc[i + j] += (int32_t)a[i] * b[j];

  for(i = 0; i < N; i++) {
    int32_t high = (i + N < 2 * N - 1) ? acc[i + N] : 0;
    r[i] = (uint16_t)((acc[i] - high) & (SABER_Q - 1));
  }
}

static void saber_schoolbook_exact_avx2(uint16_t r[N], const uint16_t a[N], const uint16_t b[N])
{
  unsigned i, j;
  uint16_t acc[N] __attribute__((aligned(32)));
  const __m256i mask = _mm256_set1_epi16(SABER_Q - 1);

  memset(acc, 0, sizeof(acc));

  for(i = 0; i < N; i++) {
    __m256i ai = _mm256_set1_epi16((int16_t)a[i]);
    unsigned positive = N - i;

    for(j = 0; j + 16 <= positive; j += 16) {
      __m256i bv = _mm256_loadu_si256((const __m256i *)&b[j]);
      __m256i prod = _mm256_mullo_epi16(ai, bv);
      __m256i cur = _mm256_loadu_si256((const __m256i *)&acc[i + j]);
      cur = _mm256_add_epi16(cur, prod);
      _mm256_storeu_si256((__m256i *)&acc[i + j], cur);
    }
    for(; j < positive; j++)
      acc[i + j] = (uint16_t)(acc[i + j] + a[i] * b[j]);

    for(j = positive; j + 16 <= N; j += 16) {
      __m256i bv = _mm256_loadu_si256((const __m256i *)&b[j]);
      __m256i prod = _mm256_mullo_epi16(ai, bv);
      __m256i cur = _mm256_loadu_si256((const __m256i *)&acc[i + j - N]);
      cur = _mm256_sub_epi16(cur, prod);
      _mm256_storeu_si256((__m256i *)&acc[i + j - N], cur);
    }
    for(; j < N; j++)
      acc[i + j - N] = (uint16_t)(acc[i + j - N] - a[i] * b[j]);
  }

  for(i = 0; i < N; i += 16) {
    __m256i x = _mm256_load_si256((const __m256i *)&acc[i]);
    x = _mm256_and_si256(x, mask);
    _mm256_storeu_si256((__m256i *)&r[i], x);
  }
}

static void random_q12289(int32_t a[N])
{
  unsigned i;

  for(i = 0; i < N; i++)
    a[i] = (int32_t)(xorshift32() % Q12289);
}

static void random_saber(uint16_t a[N])
{
  unsigned i;

  for(i = 0; i < N; i++)
    a[i] = (uint16_t)(xorshift32() & (SABER_Q - 1));
}

static void init_q12289(void)
{
  unsigned i, j, len, stage;
  int32_t g = find_primitive_root(Q12289);

  q12289_psi = pow_mod(g, (Q12289 - 1) / (2 * N), Q12289);
  q12289_omega = (int32_t)(((int64_t)q12289_psi * q12289_psi) % Q12289);
  q12289_psi_inv = inv_mod(q12289_psi, Q12289);
  q12289_omega_inv = inv_mod(q12289_omega, Q12289);
  q12289_n_inv = inv_mod(N, Q12289);

  q12289_twist[0] = 1;
  q12289_untwist[0] = 1;
  for(i = 1; i < N; i++) {
    q12289_twist[i] = mul_q12289(q12289_twist[i - 1], q12289_psi);
    q12289_untwist[i] = mul_q12289(q12289_untwist[i - 1], q12289_psi_inv);
  }

  for(len = 2, stage = 0; len <= N; len <<= 1, stage++) {
    int32_t wlen = pow_mod(q12289_omega, N / len, Q12289);
    int32_t wlen_inv = pow_mod(q12289_omega_inv, N / len, Q12289);

    q12289_stage_twiddles[stage][0] = 1;
    q12289_stage_twiddles_inv[stage][0] = 1;
    for(j = 1; j < len / 2; j++) {
      q12289_stage_twiddles[stage][j] =
        mul_q12289(q12289_stage_twiddles[stage][j - 1], wlen);
      q12289_stage_twiddles_inv[stage][j] =
        mul_q12289(q12289_stage_twiddles_inv[stage][j - 1], wlen_inv);
    }
  }

  if(g == 0 ||
     pow_mod(q12289_psi, N, Q12289) != Q12289 - 1 ||
     pow_mod(q12289_omega, N, Q12289) != 1 ||
     pow_mod(q12289_omega, N / 2, Q12289) == 1) {
    fprintf(stderr, "q12289 root initialization failed\n");
    exit(1);
  }
}

static void selftest(void)
{
  unsigned i;
  int32_t a[N], b[N], c0[N], c1[N], c2[N], c3[N], rt[N], rt_avx[N], rt_nobr[N];
  uint16_t sa[N], sb[N], sr[N], sr_avx[N];

  random_q12289(a);
  memcpy(rt, a, sizeof(rt));
  q12289_forward_negacyclic(rt);
  q12289_inverse_negacyclic(rt);
  for(i = 0; i < N; i++) {
    if(rt[i] != a[i]) {
      fprintf(stderr, "q12289 NTT round-trip failed at %u: %d != %d\n", i, rt[i], a[i]);
      exit(1);
    }
  }

  memcpy(rt_avx, a, sizeof(rt_avx));
  q12289_forward_negacyclic_avx2(rt_avx);
  q12289_inverse_negacyclic_avx2(rt_avx);
  for(i = 0; i < N; i++) {
    if(rt_avx[i] != a[i]) {
      fprintf(stderr, "q12289 AVX2 NTT round-trip failed at %u: %d != %d\n",
              i, rt_avx[i], a[i]);
      exit(1);
    }
  }

  memcpy(rt_nobr, a, sizeof(rt_nobr));
  q12289_forward_negacyclic_avx2_nobr(rt_nobr);
  q12289_inverse_negacyclic_avx2_nobr(rt_nobr);
  for(i = 0; i < N; i++) {
    if(rt_nobr[i] != a[i]) {
      fprintf(stderr, "q12289 AVX2 no-bitrev NTT round-trip failed at %u: %d != %d\n",
              i, rt_nobr[i], a[i]);
      exit(1);
    }
  }

  random_q12289(b);
  q12289_schoolbook(c0, a, b);
  q12289_poly_mul_ntt(c1, a, b);
  q12289_poly_mul_ntt_avx2(c2, a, b);
  q12289_poly_mul_ntt_avx2_nobr(c3, a, b);
  for(i = 0; i < N; i++) {
    if(c0[i] != c1[i]) {
      fprintf(stderr, "q12289 mul failed at %u: %d != %d\n", i, c1[i], c0[i]);
      exit(1);
    }
    if(c0[i] != c2[i]) {
      fprintf(stderr, "q12289 AVX2 mul failed at %u: %d != %d\n", i, c2[i], c0[i]);
      exit(1);
    }
    if(c0[i] != c3[i]) {
      fprintf(stderr, "q12289 AVX2 no-bitrev mul failed at %u: %d != %d\n", i, c3[i], c0[i]);
      exit(1);
    }
  }

  random_saber(sa);
  random_saber(sb);
  saber_schoolbook_exact(sr, sa, sb);
  saber_schoolbook_exact_avx2(sr_avx, sa, sb);
  for(i = 0; i < N; i++) {
    if(sr[i] != sr_avx[i]) {
      fprintf(stderr, "Saber AVX2 exact mul failed at %u: %u != %u\n",
              i, sr_avx[i], sr[i]);
      exit(1);
    }
    sink ^= sr[i];
  }
}

static int32_t bench_a[N] __attribute__((aligned(32)));
static int32_t bench_b[N] __attribute__((aligned(32)));
static int32_t bench_a_ntt[N] __attribute__((aligned(32)));
static int32_t bench_b_ntt[N] __attribute__((aligned(32)));
static int32_t bench_a_ntt_nobr[N] __attribute__((aligned(32)));
static int32_t bench_b_ntt_nobr[N] __attribute__((aligned(32)));
static int32_t bench_c[N] __attribute__((aligned(32)));
static uint16_t saber_a[N] __attribute__((aligned(32)));
static uint16_t saber_b[N] __attribute__((aligned(32)));
static uint16_t saber_c[N] __attribute__((aligned(32)));

static void bench_once(const char *label, void (*fn)(void))
{
  unsigned i;

  for(i = 0; i < NTESTS; i++) {
    t[i] = cpucycles();
    fn();
  }

  print_results_stats(label, t, NTESTS);
}

static void do_q12289_ntt(void)
{
  int32_t a[N];

  memcpy(a, bench_a, sizeof(a));
  q12289_forward_negacyclic(a);
  sink ^= (uint64_t)a[0];
}

static void do_q12289_intt(void)
{
  int32_t a[N];

  memcpy(a, bench_a_ntt, sizeof(a));
  q12289_inverse_negacyclic(a);
  sink ^= (uint64_t)a[1];
}

static void do_q12289_basemul(void)
{
  q12289_basemul(bench_c, bench_a_ntt, bench_b_ntt);
  sink ^= (uint64_t)bench_c[2];
}

static void do_q12289_ntt_avx2(void)
{
  int32_t a[N] __attribute__((aligned(32)));

  memcpy(a, bench_a, sizeof(a));
  q12289_forward_negacyclic_avx2(a);
  sink ^= (uint64_t)a[0];
}

static void do_q12289_intt_avx2(void)
{
  int32_t a[N] __attribute__((aligned(32)));

  memcpy(a, bench_a_ntt, sizeof(a));
  q12289_inverse_negacyclic_avx2(a);
  sink ^= (uint64_t)a[1];
}

static void do_q12289_ntt_avx2_nobr(void)
{
  int32_t a[N] __attribute__((aligned(32)));

  memcpy(a, bench_a, sizeof(a));
  q12289_forward_negacyclic_avx2_nobr(a);
  sink ^= (uint64_t)a[0];
}

static void do_q12289_intt_avx2_nobr(void)
{
  int32_t a[N] __attribute__((aligned(32)));

  memcpy(a, bench_a_ntt_nobr, sizeof(a));
  q12289_inverse_negacyclic_avx2_nobr(a);
  sink ^= (uint64_t)a[1];
}

static void do_q12289_basemul_avx2(void)
{
  q12289_basemul_avx2(bench_c, bench_a_ntt, bench_b_ntt);
  sink ^= (uint64_t)bench_c[2];
}

static void do_q12289_poly_mul_ntt(void)
{
  q12289_poly_mul_ntt(bench_c, bench_a, bench_b);
  sink ^= (uint64_t)bench_c[3];
}

static void do_q12289_poly_mul_ntt_avx2(void)
{
  q12289_poly_mul_ntt_avx2(bench_c, bench_a, bench_b);
  sink ^= (uint64_t)bench_c[3];
}

static void do_q12289_poly_mul_ntt_avx2_nobr(void)
{
  q12289_poly_mul_ntt_avx2_nobr(bench_c, bench_a, bench_b);
  sink ^= (uint64_t)bench_c[3];
}

static void do_q12289_schoolbook(void)
{
  q12289_schoolbook(bench_c, bench_a, bench_b);
  sink ^= (uint64_t)bench_c[4];
}

static void do_saber_schoolbook_exact(void)
{
  saber_schoolbook_exact(saber_c, saber_a, saber_b);
  sink ^= (uint64_t)saber_c[5];
}

static void do_saber_schoolbook_exact_avx2(void)
{
  saber_schoolbook_exact_avx2(saber_c, saber_a, saber_b);
  sink ^= (uint64_t)saber_c[5];
}

int main(void)
{
  init_q12289();
  selftest();

  random_q12289(bench_a);
  random_q12289(bench_b);
  memcpy(bench_a_ntt, bench_a, sizeof(bench_a_ntt));
  memcpy(bench_b_ntt, bench_b, sizeof(bench_b_ntt));
  memcpy(bench_a_ntt_nobr, bench_a, sizeof(bench_a_ntt_nobr));
  memcpy(bench_b_ntt_nobr, bench_b, sizeof(bench_b_ntt_nobr));
  q12289_forward_negacyclic_avx2(bench_a_ntt);
  q12289_forward_negacyclic_avx2(bench_b_ntt);
  q12289_forward_negacyclic_avx2_nobr(bench_a_ntt_nobr);
  q12289_forward_negacyclic_avx2_nobr(bench_b_ntt_nobr);

  random_saber(saber_a);
  random_saber(saber_b);

  printf("========== q12289 / Saber standalone benchmark (N=%d, NTESTS=%d) ==========\n", N, NTESTS);
  printf("q12289: full negacyclic NTT, scalar and AVX2 baselines, q=%d\n", Q12289);
  printf("Saber-exact: schoolbook negacyclic multiplication, scalar and AVX2, modulus 2^13\n\n");

  bench_once("q12289_ntt_full_negacyclic: ", do_q12289_ntt);
  bench_once("q12289_ntt_full_negacyclic_avx2: ", do_q12289_ntt_avx2);
  bench_once("q12289_ntt_full_negacyclic_avx2_nobr: ", do_q12289_ntt_avx2_nobr);
  bench_once("q12289_invntt_full_negacyclic: ", do_q12289_intt);
  bench_once("q12289_invntt_full_negacyclic_avx2: ", do_q12289_intt_avx2);
  bench_once("q12289_invntt_full_negacyclic_avx2_nobr: ", do_q12289_intt_avx2_nobr);
  bench_once("q12289_basemul_pointwise: ", do_q12289_basemul);
  bench_once("q12289_basemul_pointwise_avx2: ", do_q12289_basemul_avx2);
  bench_once("q12289_poly_mul_ntt: ", do_q12289_poly_mul_ntt);
  bench_once("q12289_poly_mul_ntt_avx2: ", do_q12289_poly_mul_ntt_avx2);
  bench_once("q12289_poly_mul_ntt_avx2_nobr: ", do_q12289_poly_mul_ntt_avx2_nobr);
  bench_once("q12289_poly_mul_schoolbook: ", do_q12289_schoolbook);
  bench_once("saber_exact_poly_mul_schoolbook: ", do_saber_schoolbook_exact);
  bench_once("saber_exact_poly_mul_schoolbook_avx2: ", do_saber_schoolbook_exact_avx2);

  printf("sink: %llu\n", (unsigned long long)sink);

  return 0;
}
