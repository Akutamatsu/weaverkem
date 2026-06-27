/*
 * Standalone q=7681 experiment for n=256 full negacyclic NTT.
 *
 * This uses the same 8-lane int32 AVX2 strategy as the q=12289 no-bitrev
 * benchmark, so it isolates the effect of changing q before introducing a
 * separate 16-bit Harvey implementation.
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
#define Q7681 7681
#define Q7681_BARRETT ((uint32_t)(4294967296ULL / Q7681))

static uint64_t t[NTESTS];
static volatile uint64_t sink;

static int32_t q7681_psi;
static int32_t q7681_omega;
static int32_t q7681_psi_inv;
static int32_t q7681_omega_inv;
static int32_t q7681_n_inv;
static int32_t q7681_twist[N] __attribute__((aligned(32)));
static int32_t q7681_untwist[N] __attribute__((aligned(32)));
static int32_t q7681_stage_twiddles[8][N / 2] __attribute__((aligned(32)));
static int32_t q7681_stage_twiddles_inv[8][N / 2] __attribute__((aligned(32)));

static int32_t bench_a[N] __attribute__((aligned(32)));
static int32_t bench_b[N] __attribute__((aligned(32)));
static int32_t bench_a_ntt[N] __attribute__((aligned(32)));
static int32_t bench_b_ntt[N] __attribute__((aligned(32)));
static int32_t bench_c[N] __attribute__((aligned(32)));

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

static int32_t reduce_q7681_u32(uint32_t a)
{
  uint32_t t = (uint32_t)(((uint64_t)a * Q7681_BARRETT) >> 32);
  int32_t r = (int32_t)(a - t * Q7681);

  if(r >= Q7681)
    r -= Q7681;
  if(r >= Q7681)
    r -= Q7681;
  if(r < 0)
    r += Q7681;

  return r;
}

static int32_t mul_q7681(int32_t a, int32_t b)
{
  return reduce_q7681_u32((uint32_t)(a * b));
}

static __m256i reduce_q7681_x8(__m256i a)
{
  const __m256i barrett = _mm256_set1_epi32((int32_t)Q7681_BARRETT);
  const __m256i q = _mm256_set1_epi32(Q7681);
  const __m256i q_minus_1 = _mm256_set1_epi32(Q7681 - 1);
  __m256i even_prod, odd_prod, tq, r, mask, t;

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

static __m256i mul_q7681_x8(__m256i a, __m256i b)
{
  return reduce_q7681_x8(_mm256_mullo_epi32(a, b));
}

static __m256i add_q7681_x8(__m256i a, __m256i b)
{
  const __m256i q = _mm256_set1_epi32(Q7681);
  const __m256i q_minus_1 = _mm256_set1_epi32(Q7681 - 1);
  __m256i r = _mm256_add_epi32(a, b);
  __m256i mask = _mm256_cmpgt_epi32(r, q_minus_1);

  return _mm256_sub_epi32(r, _mm256_and_si256(mask, q));
}

static __m256i sub_q7681_x8(__m256i a, __m256i b)
{
  const __m256i q = _mm256_set1_epi32(Q7681);
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

static void q7681_forward_negacyclic_avx2(int32_t a[N])
{
  unsigned len, i, j;

  for(i = 0; i < N; i += 8) {
    __m256i x = _mm256_loadu_si256((const __m256i *)&a[i]);
    __m256i tw = _mm256_load_si256((const __m256i *)&q7681_twist[i]);
    x = mul_q7681_x8(x, tw);
    _mm256_storeu_si256((__m256i *)&a[i], x);
  }

  for(len = N / 2; len >= 1; len >>= 1) {
    const int32_t *twiddles = q7681_stage_twiddles[__builtin_ctz(len * 2) - 1];

    for(i = 0; i < N; i += 2 * len) {
      for(j = 0; j + 8 <= len; j += 8) {
        __m256i u = _mm256_loadu_si256((const __m256i *)&a[i + j]);
        __m256i v = _mm256_loadu_si256((const __m256i *)&a[i + j + len]);
        __m256i w = _mm256_load_si256((const __m256i *)&twiddles[j]);
        __m256i sum = add_q7681_x8(u, v);
        __m256i diff = sub_q7681_x8(u, v);

        _mm256_storeu_si256((__m256i *)&a[i + j], sum);
        _mm256_storeu_si256((__m256i *)&a[i + j + len], mul_q7681_x8(diff, w));
      }

      for(; j < len; j++) {
        int32_t u = a[i + j];
        int32_t v = a[i + j + len];
        int32_t sum = u + v;
        int32_t diff = u - v;

        if(sum >= Q7681)
          sum -= Q7681;
        if(diff < 0)
          diff += Q7681;

        a[i + j] = sum;
        a[i + j + len] = mul_q7681(diff, twiddles[j]);
      }
    }
  }
}

static void q7681_inverse_negacyclic_avx2(int32_t a[N])
{
  unsigned len, i, j;
  __m256i ninv = _mm256_set1_epi32(q7681_n_inv);

  for(len = 1; len < N; len <<= 1) {
    const int32_t *twiddles = q7681_stage_twiddles_inv[__builtin_ctz(len * 2) - 1];

    for(i = 0; i < N; i += 2 * len) {
      for(j = 0; j + 8 <= len; j += 8) {
        __m256i u = _mm256_loadu_si256((const __m256i *)&a[i + j]);
        __m256i v = _mm256_loadu_si256((const __m256i *)&a[i + j + len]);
        __m256i w = _mm256_load_si256((const __m256i *)&twiddles[j]);

        v = mul_q7681_x8(v, w);
        _mm256_storeu_si256((__m256i *)&a[i + j], add_q7681_x8(u, v));
        _mm256_storeu_si256((__m256i *)&a[i + j + len], sub_q7681_x8(u, v));
      }

      for(; j < len; j++) {
        int32_t u = a[i + j];
        int32_t v = mul_q7681(a[i + j + len], twiddles[j]);

        a[i + j] = u + v;
        if(a[i + j] >= Q7681)
          a[i + j] -= Q7681;

        a[i + j + len] = u - v;
        if(a[i + j + len] < 0)
          a[i + j + len] += Q7681;
      }
    }
  }

  for(i = 0; i < N; i += 8) {
    __m256i x = _mm256_loadu_si256((const __m256i *)&a[i]);
    __m256i tw = _mm256_load_si256((const __m256i *)&q7681_untwist[i]);
    x = mul_q7681_x8(mul_q7681_x8(x, ninv), tw);
    _mm256_storeu_si256((__m256i *)&a[i], x);
  }
}

static void q7681_basemul_avx2(int32_t r[N], const int32_t a[N], const int32_t b[N])
{
  unsigned i;

  for(i = 0; i < N; i += 8) {
    __m256i va = _mm256_loadu_si256((const __m256i *)&a[i]);
    __m256i vb = _mm256_loadu_si256((const __m256i *)&b[i]);
    __m256i vc = mul_q7681_x8(va, vb);
    _mm256_storeu_si256((__m256i *)&r[i], vc);
  }
}

static void q7681_schoolbook(int32_t r[N], const int32_t a[N], const int32_t b[N])
{
  unsigned i, j;
  int64_t acc[2 * N - 1];

  memset(acc, 0, sizeof(acc));

  for(i = 0; i < N; i++)
    for(j = 0; j < N; j++)
      acc[i + j] += (int64_t)a[i] * b[j];

  for(i = 0; i < N; i++) {
    int64_t high = (i + N < 2 * N - 1) ? acc[i + N] : 0;
    r[i] = mod_q(acc[i] - high, Q7681);
  }
}

static void q7681_poly_mul_ntt_avx2(int32_t r[N], const int32_t a[N], const int32_t b[N])
{
  int32_t ta[N] __attribute__((aligned(32)));
  int32_t tb[N] __attribute__((aligned(32)));

  memcpy(ta, a, sizeof(ta));
  memcpy(tb, b, sizeof(tb));

  q7681_forward_negacyclic_avx2(ta);
  q7681_forward_negacyclic_avx2(tb);
  q7681_basemul_avx2(r, ta, tb);
  q7681_inverse_negacyclic_avx2(r);
}

static void random_q7681(int32_t a[N])
{
  unsigned i;

  for(i = 0; i < N; i++)
    a[i] = (int32_t)(xorshift32() % Q7681);
}

static void init_q7681(void)
{
  unsigned i, j, len, stage;
  int32_t g = find_primitive_root_q7681();

  q7681_psi = pow_mod(g, (Q7681 - 1) / (2 * N), Q7681);
  q7681_omega = mul_q7681(q7681_psi, q7681_psi);
  q7681_psi_inv = inv_mod(q7681_psi, Q7681);
  q7681_omega_inv = inv_mod(q7681_omega, Q7681);
  q7681_n_inv = inv_mod(N, Q7681);

  q7681_twist[0] = 1;
  q7681_untwist[0] = 1;
  for(i = 1; i < N; i++) {
    q7681_twist[i] = mul_q7681(q7681_twist[i - 1], q7681_psi);
    q7681_untwist[i] = mul_q7681(q7681_untwist[i - 1], q7681_psi_inv);
  }

  for(len = 2, stage = 0; len <= N; len <<= 1, stage++) {
    int32_t wlen = pow_mod(q7681_omega, N / len, Q7681);
    int32_t wlen_inv = pow_mod(q7681_omega_inv, N / len, Q7681);

    q7681_stage_twiddles[stage][0] = 1;
    q7681_stage_twiddles_inv[stage][0] = 1;
    for(j = 1; j < len / 2; j++) {
      q7681_stage_twiddles[stage][j] =
        mul_q7681(q7681_stage_twiddles[stage][j - 1], wlen);
      q7681_stage_twiddles_inv[stage][j] =
        mul_q7681(q7681_stage_twiddles_inv[stage][j - 1], wlen_inv);
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
  unsigned i;
  int32_t a[N], b[N], c_ref[N], c_ntt[N], rt[N];

  random_q7681(a);
  memcpy(rt, a, sizeof(rt));
  q7681_forward_negacyclic_avx2(rt);
  q7681_inverse_negacyclic_avx2(rt);
  for(i = 0; i < N; i++) {
    if(rt[i] != a[i]) {
      fprintf(stderr, "q7681 AVX2 NTT round-trip failed at %u: %d != %d\n",
              i, rt[i], a[i]);
      exit(1);
    }
  }

  random_q7681(b);
  q7681_schoolbook(c_ref, a, b);
  q7681_poly_mul_ntt_avx2(c_ntt, a, b);
  for(i = 0; i < N; i++) {
    if(c_ref[i] != c_ntt[i]) {
      fprintf(stderr, "q7681 AVX2 mul failed at %u: %d != %d\n", i, c_ntt[i], c_ref[i]);
      exit(1);
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

static void do_q7681_ntt_avx2(void)
{
  int32_t a[N] __attribute__((aligned(32)));

  memcpy(a, bench_a, sizeof(a));
  q7681_forward_negacyclic_avx2(a);
  sink ^= (uint64_t)a[0];
}

static void do_q7681_intt_avx2(void)
{
  int32_t a[N] __attribute__((aligned(32)));

  memcpy(a, bench_a_ntt, sizeof(a));
  q7681_inverse_negacyclic_avx2(a);
  sink ^= (uint64_t)a[1];
}

static void do_q7681_basemul_avx2(void)
{
  q7681_basemul_avx2(bench_c, bench_a_ntt, bench_b_ntt);
  sink ^= (uint64_t)bench_c[2];
}

static void do_q7681_poly_mul_ntt_avx2(void)
{
  q7681_poly_mul_ntt_avx2(bench_c, bench_a, bench_b);
  sink ^= (uint64_t)bench_c[3];
}

static void do_q7681_schoolbook(void)
{
  q7681_schoolbook(bench_c, bench_a, bench_b);
  sink ^= (uint64_t)bench_c[4];
}

int main(void)
{
  init_q7681();
  selftest();

  random_q7681(bench_a);
  random_q7681(bench_b);
  memcpy(bench_a_ntt, bench_a, sizeof(bench_a_ntt));
  memcpy(bench_b_ntt, bench_b, sizeof(bench_b_ntt));
  q7681_forward_negacyclic_avx2(bench_a_ntt);
  q7681_forward_negacyclic_avx2(bench_b_ntt);

  printf("========== q7681 standalone benchmark (N=%d, NTESTS=%d) ==========\n", N, NTESTS);
  printf("q7681: full negacyclic NTT, AVX2 int32 no-bitrev baseline, q=%d\n\n", Q7681);

  bench_once("q7681_ntt_full_negacyclic_avx2_nobr: ", do_q7681_ntt_avx2);
  bench_once("q7681_invntt_full_negacyclic_avx2_nobr: ", do_q7681_intt_avx2);
  bench_once("q7681_basemul_pointwise_avx2: ", do_q7681_basemul_avx2);
  bench_once("q7681_poly_mul_ntt_avx2_nobr: ", do_q7681_poly_mul_ntt_avx2);
  bench_once("q7681_poly_mul_schoolbook: ", do_q7681_schoolbook);

  printf("sink: %llu\n", (unsigned long long)sink);

  return 0;
}
