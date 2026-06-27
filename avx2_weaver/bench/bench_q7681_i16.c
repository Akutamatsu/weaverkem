/*
 * q=7681, n=256 full negacyclic NTT using 16-bit AVX2 lanes.
 *
 * This is a standalone experiment.  It keeps coefficients in canonical
 * [0,q) form and uses 16-lane Montgomery multiplication for twiddle products.
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
#ifndef CSELFTESTS
#define CSELFTESTS 1000
#endif

#define N 256
#define Q7681 7681

static uint64_t t[NTESTS];
static volatile uint64_t sink;

static uint16_t q7681_qinv;
static uint16_t q7681_r2;
static int32_t q7681_psi;
static int32_t q7681_omega;
static int32_t q7681_psi_inv;
static int32_t q7681_omega_inv;
static int32_t q7681_n_inv;
static uint16_t q7681_twist[N] __attribute__((aligned(32)));
static uint16_t q7681_untwist[N] __attribute__((aligned(32)));
static uint16_t q7681_invscale[N] __attribute__((aligned(32)));
static uint16_t q7681_stage_twiddles[8][N / 2] __attribute__((aligned(32)));
static uint16_t q7681_stage_twiddles_inv[8][N / 2] __attribute__((aligned(32)));

static uint16_t bench_a[N] __attribute__((aligned(32)));
static uint16_t bench_b[N] __attribute__((aligned(32)));
static uint16_t bench_a_ntt[N] __attribute__((aligned(32)));
static uint16_t bench_b_ntt[N] __attribute__((aligned(32)));
static uint16_t bench_c[N] __attribute__((aligned(32)));

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

static int32_t inv_mod_prime(int32_t a, int32_t q)
{
  return pow_mod(a, q - 2, q);
}

static uint16_t mul_q7681(uint16_t a, uint16_t b)
{
  uint32_t prod = (uint32_t)a * b;
  uint32_t m = (uint16_t)((uint16_t)prod * q7681_qinv);
  uint32_t u = (prod + m * Q7681) >> 16;

  if(u >= Q7681)
    u -= Q7681;

  return (uint16_t)u;
}

static uint16_t mul_std_q7681(uint16_t a, uint16_t b)
{
  return (uint16_t)(((uint32_t)a * b) % Q7681);
}

static uint16_t to_mont_q7681(uint16_t a)
{
  return (uint16_t)(((uint32_t)a * (65536U % Q7681)) % Q7681);
}

static __m256i montgomery_q7681_x16(__m256i a, __m256i b)
{
  const __m256i q = _mm256_set1_epi16(Q7681);
  const __m256i q_minus_1 = _mm256_set1_epi16(Q7681 - 1);
  const __m256i qinv = _mm256_set1_epi16((int16_t)q7681_qinv);
  const __m256i zero = _mm256_setzero_si256();
  const __m256i one = _mm256_set1_epi16(1);
  __m256i prod_lo, prod_hi, m, mq_lo, mq_hi, carry, u, mask;

  prod_lo = _mm256_mullo_epi16(a, b);
  prod_hi = _mm256_mulhi_epu16(a, b);
  m = _mm256_mullo_epi16(prod_lo, qinv);
  mq_lo = _mm256_mullo_epi16(m, q);
  mq_hi = _mm256_mulhi_epu16(m, q);

  (void)mq_lo;
  carry = _mm256_andnot_si256(_mm256_cmpeq_epi16(prod_lo, zero), one);
  u = _mm256_add_epi16(_mm256_add_epi16(prod_hi, mq_hi), carry);

  mask = _mm256_cmpgt_epi16(u, q_minus_1);
  u = _mm256_sub_epi16(u, _mm256_and_si256(mask, q));

  return u;
}

static __m128i montgomery_q7681_x8(__m128i a, __m128i b)
{
  const __m128i q = _mm_set1_epi16(Q7681);
  const __m128i q_minus_1 = _mm_set1_epi16(Q7681 - 1);
  const __m128i qinv = _mm_set1_epi16((int16_t)q7681_qinv);
  const __m128i zero = _mm_setzero_si128();
  const __m128i one = _mm_set1_epi16(1);
  __m128i prod_lo, prod_hi, m, mq_hi, carry, u, mask;

  prod_lo = _mm_mullo_epi16(a, b);
  prod_hi = _mm_mulhi_epu16(a, b);
  m = _mm_mullo_epi16(prod_lo, qinv);
  mq_hi = _mm_mulhi_epu16(m, q);
  carry = _mm_andnot_si128(_mm_cmpeq_epi16(prod_lo, zero), one);
  u = _mm_add_epi16(_mm_add_epi16(prod_hi, mq_hi), carry);

  mask = _mm_cmpgt_epi16(u, q_minus_1);
  u = _mm_sub_epi16(u, _mm_and_si128(mask, q));

  return u;
}

static __m256i to_mont_q7681_x16(__m256i a)
{
  return montgomery_q7681_x16(a, _mm256_set1_epi16((int16_t)q7681_r2));
}

static __m256i add_q7681_x16(__m256i a, __m256i b)
{
  const __m256i q = _mm256_set1_epi16(Q7681);
  const __m256i q_minus_1 = _mm256_set1_epi16(Q7681 - 1);
  __m256i r = _mm256_add_epi16(a, b);
  __m256i mask = _mm256_cmpgt_epi16(r, q_minus_1);

  return _mm256_sub_epi16(r, _mm256_and_si256(mask, q));
}

static __m128i add_q7681_x8(__m128i a, __m128i b)
{
  const __m128i q = _mm_set1_epi16(Q7681);
  const __m128i q_minus_1 = _mm_set1_epi16(Q7681 - 1);
  __m128i r = _mm_add_epi16(a, b);
  __m128i mask = _mm_cmpgt_epi16(r, q_minus_1);

  return _mm_sub_epi16(r, _mm_and_si128(mask, q));
}

static __m256i sub_q7681_x16(__m256i a, __m256i b)
{
  const __m256i q = _mm256_set1_epi16(Q7681);
  const __m256i zero = _mm256_setzero_si256();
  __m256i r = _mm256_sub_epi16(a, b);
  __m256i mask = _mm256_cmpgt_epi16(zero, r);

  return _mm256_add_epi16(r, _mm256_and_si256(mask, q));
}

static __m128i sub_q7681_x8(__m128i a, __m128i b)
{
  const __m128i q = _mm_set1_epi16(Q7681);
  const __m128i zero = _mm_setzero_si128();
  __m128i r = _mm_sub_epi16(a, b);
  __m128i mask = _mm_cmpgt_epi16(zero, r);

  return _mm_add_epi16(r, _mm_and_si128(mask, q));
}

static __m256i swap_adjacent_u16_x16(__m256i x)
{
  x = _mm256_shufflelo_epi16(x, 0xB1);
  x = _mm256_shufflehi_epi16(x, 0xB1);
  return x;
}

static __m256i len1_butterfly_q7681_x16(__m256i x)
{
  const __m256i even_lanes = _mm256_set1_epi32(0x0000FFFF);
  __m256i y = swap_adjacent_u16_x16(x);
  __m256i sum = add_q7681_x16(x, y);
  __m256i diff = sub_q7681_x16(x, y);

  return _mm256_or_si256(_mm256_and_si256(sum, even_lanes),
                         _mm256_slli_epi32(_mm256_and_si256(diff, even_lanes), 16));
}

static int is_primitive_root_q7681(int32_t g)
{
  return pow_mod(g, (Q7681 - 1) / 2, Q7681) != 1 &&
         pow_mod(g, (Q7681 - 1) / 3, Q7681) != 1 &&
         pow_mod(g, (Q7681 - 1) / 5, Q7681) != 1;
}

static int32_t find_primitive_root_q7681(void)
{
  int32_t g;

  for(g = 2; g < Q7681; g++)
    if(is_primitive_root_q7681(g))
      return g;

  return 0;
}

static void q7681_forward_negacyclic_i16_avx2(uint16_t a[N])
{
  unsigned len, i, j;

  for(i = 0; i < N; i += 16) {
    __m256i x = _mm256_loadu_si256((const __m256i *)&a[i]);
    __m256i tw = _mm256_loadu_si256((const __m256i *)&q7681_twist[i]);
    x = montgomery_q7681_x16(x, tw);
    _mm256_storeu_si256((__m256i *)&a[i], x);
  }

  for(len = N / 2; len >= 1; len >>= 1) {
    const uint16_t *twiddles = q7681_stage_twiddles[__builtin_ctz(len * 2) - 1];

    if(len == 1) {
      for(i = 0; i < N; i += 16) {
        __m256i x = _mm256_loadu_si256((const __m256i *)&a[i]);
        x = len1_butterfly_q7681_x16(x);
        _mm256_storeu_si256((__m256i *)&a[i], x);
      }
      continue;
    }

    if(len == 4) {
      for(i = 0; i < N; i += 8) {
        __m128i u = _mm_loadl_epi64((const __m128i *)&a[i]);
        __m128i v = _mm_loadl_epi64((const __m128i *)&a[i + 4]);
        __m128i w = _mm_loadl_epi64((const __m128i *)twiddles);
        __m128i sum = add_q7681_x8(u, v);
        __m128i diff = sub_q7681_x8(u, v);

        _mm_storel_epi64((__m128i *)&a[i], sum);
        _mm_storel_epi64((__m128i *)&a[i + 4], montgomery_q7681_x8(diff, w));
      }
      continue;
    }

    if(len == 2) {
      for(i = 0; i < N; i += 4) {
        uint32_t u32, v32, s32, p32;
        __m128i u, v, w, sum, diff, prod;

        memcpy(&u32, &a[i], sizeof(u32));
        memcpy(&v32, &a[i + 2], sizeof(v32));
        u = _mm_cvtsi32_si128((int)u32);
        v = _mm_cvtsi32_si128((int)v32);
        w = _mm_cvtsi32_si128(*(const int *)twiddles);
        sum = add_q7681_x8(u, v);
        diff = sub_q7681_x8(u, v);
        prod = montgomery_q7681_x8(diff, w);
        s32 = (uint32_t)_mm_cvtsi128_si32(sum);
        p32 = (uint32_t)_mm_cvtsi128_si32(prod);
        memcpy(&a[i], &s32, sizeof(s32));
        memcpy(&a[i + 2], &p32, sizeof(p32));
      }
      continue;
    }

    for(i = 0; i < N; i += 2 * len) {
      for(j = 0; j + 16 <= len; j += 16) {
        __m256i u = _mm256_loadu_si256((const __m256i *)&a[i + j]);
        __m256i v = _mm256_loadu_si256((const __m256i *)&a[i + j + len]);
        __m256i w = _mm256_loadu_si256((const __m256i *)&twiddles[j]);
        __m256i sum = add_q7681_x16(u, v);
        __m256i diff = sub_q7681_x16(u, v);

        _mm256_storeu_si256((__m256i *)&a[i + j], sum);
        _mm256_storeu_si256((__m256i *)&a[i + j + len],
                            montgomery_q7681_x16(diff, w));
      }

      for(; j + 8 <= len; j += 8) {
        __m128i u = _mm_loadu_si128((const __m128i *)&a[i + j]);
        __m128i v = _mm_loadu_si128((const __m128i *)&a[i + j + len]);
        __m128i w = _mm_loadu_si128((const __m128i *)&twiddles[j]);
        __m128i sum = add_q7681_x8(u, v);
        __m128i diff = sub_q7681_x8(u, v);

        _mm_storeu_si128((__m128i *)&a[i + j], sum);
        _mm_storeu_si128((__m128i *)&a[i + j + len],
                         montgomery_q7681_x8(diff, w));
      }

      for(; j < len; j++) {
        uint16_t u = a[i + j];
        uint16_t v = a[i + j + len];
        uint16_t sum = (uint16_t)(u + v);
        uint16_t diff = (u >= v) ? (uint16_t)(u - v)
                                 : (uint16_t)(u + Q7681 - v);

        if(sum >= Q7681)
          sum -= Q7681;

        a[i + j] = sum;
        a[i + j + len] = mul_q7681(diff, twiddles[j]);
      }
    }
  }
}

static void q7681_inverse_negacyclic_i16_avx2(uint16_t a[N])
{
  unsigned len, i, j;

  for(len = 1; len < N; len <<= 1) {
    const uint16_t *twiddles = q7681_stage_twiddles_inv[__builtin_ctz(len * 2) - 1];

    if(len == 1) {
      for(i = 0; i < N; i += 16) {
        __m256i x = _mm256_loadu_si256((const __m256i *)&a[i]);
        x = len1_butterfly_q7681_x16(x);
        _mm256_storeu_si256((__m256i *)&a[i], x);
      }
      continue;
    }

    if(len == 2) {
      for(i = 0; i < N; i += 4) {
        uint32_t u32, v32, s32, d32;
        __m128i u, v, w, sum, diff;

        memcpy(&u32, &a[i], sizeof(u32));
        memcpy(&v32, &a[i + 2], sizeof(v32));
        u = _mm_cvtsi32_si128((int)u32);
        v = _mm_cvtsi32_si128((int)v32);
        w = _mm_cvtsi32_si128(*(const int *)twiddles);
        v = montgomery_q7681_x8(v, w);
        sum = add_q7681_x8(u, v);
        diff = sub_q7681_x8(u, v);
        s32 = (uint32_t)_mm_cvtsi128_si32(sum);
        d32 = (uint32_t)_mm_cvtsi128_si32(diff);
        memcpy(&a[i], &s32, sizeof(s32));
        memcpy(&a[i + 2], &d32, sizeof(d32));
      }
      continue;
    }

    if(len == 4) {
      for(i = 0; i < N; i += 8) {
        __m128i u = _mm_loadl_epi64((const __m128i *)&a[i]);
        __m128i v = _mm_loadl_epi64((const __m128i *)&a[i + 4]);
        __m128i w = _mm_loadl_epi64((const __m128i *)twiddles);

        v = montgomery_q7681_x8(v, w);
        _mm_storel_epi64((__m128i *)&a[i], add_q7681_x8(u, v));
        _mm_storel_epi64((__m128i *)&a[i + 4], sub_q7681_x8(u, v));
      }
      continue;
    }

    for(i = 0; i < N; i += 2 * len) {
      for(j = 0; j + 16 <= len; j += 16) {
        __m256i u = _mm256_loadu_si256((const __m256i *)&a[i + j]);
        __m256i v = _mm256_loadu_si256((const __m256i *)&a[i + j + len]);
        __m256i w = _mm256_loadu_si256((const __m256i *)&twiddles[j]);

        v = montgomery_q7681_x16(v, w);
        _mm256_storeu_si256((__m256i *)&a[i + j], add_q7681_x16(u, v));
        _mm256_storeu_si256((__m256i *)&a[i + j + len], sub_q7681_x16(u, v));
      }

      for(; j + 8 <= len; j += 8) {
        __m128i u = _mm_loadu_si128((const __m128i *)&a[i + j]);
        __m128i v = _mm_loadu_si128((const __m128i *)&a[i + j + len]);
        __m128i w = _mm_loadu_si128((const __m128i *)&twiddles[j]);

        v = montgomery_q7681_x8(v, w);
        _mm_storeu_si128((__m128i *)&a[i + j], add_q7681_x8(u, v));
        _mm_storeu_si128((__m128i *)&a[i + j + len], sub_q7681_x8(u, v));
      }

      for(; j < len; j++) {
        uint16_t u = a[i + j];
        uint16_t v = mul_q7681(a[i + j + len], twiddles[j]);
        uint16_t sum = (uint16_t)(u + v);
        uint16_t diff = (u >= v) ? (uint16_t)(u - v)
                                 : (uint16_t)(u + Q7681 - v);

        if(sum >= Q7681)
          sum -= Q7681;

        a[i + j] = sum;
        a[i + j + len] = diff;
      }
    }
  }

  for(i = 0; i < N; i += 16) {
    __m256i x = _mm256_loadu_si256((const __m256i *)&a[i]);
    __m256i scale = _mm256_loadu_si256((const __m256i *)&q7681_invscale[i]);
    x = montgomery_q7681_x16(x, scale);
    _mm256_storeu_si256((__m256i *)&a[i], x);
  }
}

static void q7681_basemul_i16_avx2(uint16_t r[N], const uint16_t a[N], const uint16_t b[N])
{
  unsigned i;

  for(i = 0; i < N; i += 16) {
    __m256i va = _mm256_loadu_si256((const __m256i *)&a[i]);
    __m256i vb = _mm256_loadu_si256((const __m256i *)&b[i]);
    __m256i vc = montgomery_q7681_x16(va, to_mont_q7681_x16(vb));
    _mm256_storeu_si256((__m256i *)&r[i], vc);
  }
}

static void q7681_schoolbook(uint16_t r[N], const uint16_t a[N], const uint16_t b[N])
{
  unsigned i, j;
  int64_t acc[2 * N - 1];

  memset(acc, 0, sizeof(acc));

  for(i = 0; i < N; i++)
    for(j = 0; j < N; j++)
      acc[i + j] += (int64_t)a[i] * b[j];

  for(i = 0; i < N; i++) {
    int64_t high = (i + N < 2 * N - 1) ? acc[i + N] : 0;
    r[i] = (uint16_t)mod_q(acc[i] - high, Q7681);
  }
}

static void q7681_canonical_reduce_i16(uint16_t r[N])
{
  unsigned i;
  const __m256i q = _mm256_set1_epi16(Q7681);
  const __m256i q_minus_1 = _mm256_set1_epi16(Q7681 - 1);

  for(i = 0; i < N; i += 16) {
    __m256i x = _mm256_loadu_si256((const __m256i *)&r[i]);
    __m256i mask = _mm256_cmpgt_epi16(x, q_minus_1);
    x = _mm256_sub_epi16(x, _mm256_and_si256(mask, q));
    _mm256_storeu_si256((__m256i *)&r[i], x);
  }
}

static void q7681_poly_mul_ntt_i16_avx2(uint16_t r[N], const uint16_t a[N], const uint16_t b[N])
{
  uint16_t ta[N] __attribute__((aligned(32)));
  uint16_t tb[N] __attribute__((aligned(32)));

  memcpy(ta, a, sizeof(ta));
  memcpy(tb, b, sizeof(tb));

  q7681_forward_negacyclic_i16_avx2(ta);
  q7681_forward_negacyclic_i16_avx2(tb);
  q7681_basemul_i16_avx2(r, ta, tb);
  q7681_inverse_negacyclic_i16_avx2(r);
}

static void random_q7681(uint16_t a[N])
{
  unsigned i;

  for(i = 0; i < N; i++)
    a[i] = (uint16_t)(xorshift32() % Q7681);
}

static void random_centered_q7681(uint16_t a[N])
{
  unsigned i;

  for(i = 0; i < N; i++) {
    int32_t x = (int32_t)(xorshift32() % Q7681) - Q7681 / 2;
    a[i] = (uint16_t)mod_q(x, Q7681);
  }
}

static void fill_edge_case(uint16_t a[N], unsigned kind)
{
  unsigned i;

  switch(kind) {
  case 0:
    memset(a, 0, N * sizeof(uint16_t));
    break;
  case 1:
    for(i = 0; i < N; i++)
      a[i] = 1;
    break;
  case 2:
    memset(a, 0, N * sizeof(uint16_t));
    a[0] = 1;
    break;
  case 3:
    for(i = 0; i < N; i++)
      a[i] = (i & 1) ? Q7681 - 1 : 0;
    break;
  case 4:
    for(i = 0; i < N; i++)
      a[i] = Q7681 - 1;
    break;
  case 5:
    for(i = 0; i < N; i++)
      a[i] = (uint16_t)(i % Q7681);
    break;
  default:
    random_q7681(a);
    break;
  }
}

static void assert_canonical(const char *label, const uint16_t a[N])
{
  unsigned i;

  for(i = 0; i < N; i++) {
    if(a[i] >= Q7681) {
      fprintf(stderr, "%s not canonical at %u: %u\n", label, i, a[i]);
      exit(1);
    }
  }
}

static void check_roundtrip(const uint16_t in[N])
{
  unsigned i;
  uint16_t rt[N] __attribute__((aligned(32)));

  memcpy(rt, in, sizeof(rt));
  q7681_forward_negacyclic_i16_avx2(rt);
  q7681_inverse_negacyclic_i16_avx2(rt);
  assert_canonical("q7681 round-trip output", rt);
  for(i = 0; i < N; i++) {
    if(rt[i] != in[i]) {
      fprintf(stderr, "q7681 i16 AVX2 NTT round-trip failed at %u: %u != %u\n",
              i, rt[i], in[i]);
      exit(1);
    }
  }
}

static void check_mul(const uint16_t a[N], const uint16_t b[N])
{
  unsigned i;
  uint16_t c_ref[N], c_ntt[N] __attribute__((aligned(32)));

  q7681_schoolbook(c_ref, a, b);
  q7681_poly_mul_ntt_i16_avx2(c_ntt, a, b);
  assert_canonical("q7681 poly_mul output", c_ntt);
  for(i = 0; i < N; i++) {
    if(c_ref[i] != c_ntt[i]) {
      fprintf(stderr, "q7681 i16 AVX2 mul failed at %u: %u != %u\n", i, c_ntt[i], c_ref[i]);
      exit(1);
    }
  }
}

static void init_q7681(void)
{
  unsigned i, j, len, stage;
  int32_t g = find_primitive_root_q7681();

  q7681_qinv = 7679; /* -7681^{-1} mod 2^16 */
  q7681_r2 = 5569;  /* (2^16)^2 mod 7681 */
  q7681_psi = pow_mod(g, (Q7681 - 1) / (2 * N), Q7681);
  q7681_omega = mul_std_q7681((uint16_t)q7681_psi, (uint16_t)q7681_psi);
  q7681_psi_inv = inv_mod_prime(q7681_psi, Q7681);
  q7681_omega_inv = inv_mod_prime(q7681_omega, Q7681);
  q7681_n_inv = inv_mod_prime(N, Q7681);

  {
    uint16_t twist = 1;
    uint16_t untwist = 1;
    q7681_twist[0] = to_mont_q7681(twist);
    q7681_untwist[0] = to_mont_q7681(untwist);
    q7681_invscale[0] = to_mont_q7681((uint16_t)q7681_n_inv);
    for(i = 1; i < N; i++) {
      twist = mul_std_q7681(twist, (uint16_t)q7681_psi);
      untwist = mul_std_q7681(untwist, (uint16_t)q7681_psi_inv);
      q7681_twist[i] = to_mont_q7681(twist);
      q7681_untwist[i] = to_mont_q7681(untwist);
      q7681_invscale[i] = to_mont_q7681(mul_std_q7681(untwist, (uint16_t)q7681_n_inv));
    }
  }

  for(len = 2, stage = 0; len <= N; len <<= 1, stage++) {
    uint16_t wlen = (uint16_t)pow_mod(q7681_omega, N / len, Q7681);
    uint16_t wlen_inv = (uint16_t)pow_mod(q7681_omega_inv, N / len, Q7681);
    uint16_t w = 1;
    uint16_t winv = 1;

    q7681_stage_twiddles[stage][0] = to_mont_q7681(w);
    q7681_stage_twiddles_inv[stage][0] = to_mont_q7681(winv);
    for(j = 1; j < len / 2; j++) {
      w = mul_std_q7681(w, wlen);
      winv = mul_std_q7681(winv, wlen_inv);
      q7681_stage_twiddles[stage][j] = to_mont_q7681(w);
      q7681_stage_twiddles_inv[stage][j] = to_mont_q7681(winv);
    }
  }

  if(g == 0 ||
     pow_mod(q7681_psi, N, Q7681) != Q7681 - 1 ||
     pow_mod(q7681_omega, N, Q7681) != 1 ||
     pow_mod(q7681_omega, N / 2, Q7681) == 1) {
    fprintf(stderr, "q7681 root initialization failed\n");
    exit(1);
  }
}

static void selftest(void)
{
  unsigned i, k;
  uint16_t a[N], b[N];
  uint16_t vx[16] __attribute__((aligned(32)));
  uint16_t vy[16] __attribute__((aligned(32)));
  uint16_t vz[16] __attribute__((aligned(32)));

  for(i = 0; i < 16; i++) {
    vx[i] = (uint16_t)(xorshift32() % Q7681);
    vy[i] = to_mont_q7681((uint16_t)(xorshift32() % Q7681));
  }
  _mm256_storeu_si256((__m256i *)vz,
                      montgomery_q7681_x16(_mm256_loadu_si256((const __m256i *)vx),
                                           _mm256_loadu_si256((const __m256i *)vy)));
  for(i = 0; i < 16; i++) {
    uint16_t expected = mul_q7681(vx[i], vy[i]);
    if(vz[i] != expected) {
      fprintf(stderr, "q7681 vector Montgomery failed at %u: %u != %u\n",
              i, vz[i], expected);
      exit(1);
    }
  }

  for(k = 0; k < 6; k++) {
    fill_edge_case(a, k);
    fill_edge_case(b, 5 - k);
    check_roundtrip(a);
    check_mul(a, b);
  }

  for(k = 0; k < CSELFTESTS; k++) {
    if(k & 1)
      random_centered_q7681(a);
    else
      random_q7681(a);
    random_q7681(b);
    check_roundtrip(a);
    check_mul(a, b);
  }

  for(k = 0; k < 16; k++) {
    random_q7681(a);
    memcpy(b, a, sizeof(b));
    for(i = 0; i < 32; i++) {
      q7681_forward_negacyclic_i16_avx2(b);
      q7681_inverse_negacyclic_i16_avx2(b);
    }
    for(i = 0; i < N; i++) {
      if(a[i] != b[i]) {
        fprintf(stderr, "q7681 repeated round-trip failed at poly %u coeff %u: %u != %u\n",
                k, i, b[i], a[i]);
        exit(1);
      }
    }
  }
}

static void bench_once(const char *label, void (*fn)(void))
{
  unsigned i;

  for(i = 0; i < NTESTS; i++) {
    t[i] = cpucycles();
    fn();
  }

  print_results_stats(label, t, NTESTS);
}

static void do_q7681_ntt_i16_avx2(void)
{
  uint16_t a[N] __attribute__((aligned(32)));

  memcpy(a, bench_a, sizeof(a));
  q7681_forward_negacyclic_i16_avx2(a);
  sink ^= (uint64_t)a[0];
}

static void do_q7681_intt_i16_avx2(void)
{
  uint16_t a[N] __attribute__((aligned(32)));

  memcpy(a, bench_a_ntt, sizeof(a));
  q7681_inverse_negacyclic_i16_avx2(a);
  sink ^= (uint64_t)a[1];
}

static void do_q7681_basemul_i16_avx2(void)
{
  q7681_basemul_i16_avx2(bench_c, bench_a_ntt, bench_b_ntt);
  sink ^= (uint64_t)bench_c[2];
}

static void do_q7681_poly_mul_ntt_i16_avx2(void)
{
  q7681_poly_mul_ntt_i16_avx2(bench_c, bench_a, bench_b);
  sink ^= (uint64_t)bench_c[3];
}

static void do_q7681_schoolbook(void)
{
  q7681_schoolbook(bench_c, bench_a, bench_b);
  sink ^= (uint64_t)bench_c[4];
}

static void do_q7681_canonical_reduce(void)
{
  q7681_canonical_reduce_i16(bench_c);
  sink ^= (uint64_t)bench_c[6];
}

int main(void)
{
  init_q7681();
  selftest();

  random_q7681(bench_a);
  random_q7681(bench_b);
  memcpy(bench_a_ntt, bench_a, sizeof(bench_a_ntt));
  memcpy(bench_b_ntt, bench_b, sizeof(bench_b_ntt));
  q7681_forward_negacyclic_i16_avx2(bench_a_ntt);
  q7681_forward_negacyclic_i16_avx2(bench_b_ntt);

  printf("========== q7681 i16 standalone benchmark (N=%d, NTESTS=%d) ==========\n", N, NTESTS);
  printf("q7681: full negacyclic NTT, AVX2 16-bit Montgomery no-bitrev baseline\n");
  printf("selftest: edge cases + %d random uniform/centered mul+roundtrip checks\n\n", CSELFTESTS);

  bench_once("q7681_ntt_full_negacyclic_i16_avx2_nobr: ", do_q7681_ntt_i16_avx2);
  bench_once("q7681_invntt_full_negacyclic_i16_avx2_nobr: ", do_q7681_intt_i16_avx2);
  bench_once("q7681_basemul_pointwise_i16_avx2: ", do_q7681_basemul_i16_avx2);
  bench_once("q7681_poly_mul_ntt_i16_avx2_nobr: ", do_q7681_poly_mul_ntt_i16_avx2);
  bench_once("q7681_canonical_reduce_i16_avx2: ", do_q7681_canonical_reduce);
  bench_once("q7681_poly_mul_schoolbook_i16: ", do_q7681_schoolbook);

  printf("sink: %llu\n", (unsigned long long)sink);

  return 0;
}
