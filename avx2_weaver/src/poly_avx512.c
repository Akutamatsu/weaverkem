/* AVX2 NTT for Weaver n=512 (WEAVER-2048). Standard coefficient order (not Kyber shuffle).
 * All loops are secret-independent (fixed bounds, no branches on secret data). */
#include <stdint.h>
#include <immintrin.h>
#include "params.h"

#if defined(WEAVER_USE_AVX_NTT512) && (KYBER_N == 512)
#include "ntt.h"
#include "reduce.h"
#include "ntt_avx512.h"

extern const int16_t zetas[128];
extern const int16_t zetas_inv[128];

/* Montgomery multiply 16 lanes by one public zeta (constant-time). */
static __m256i fqmul_zeta_ct(__m256i v, int16_t zeta)
{
  int16_t in[16] __attribute__((aligned(32)));
  int16_t out[16] __attribute__((aligned(32)));
  unsigned int i;

  _mm256_store_si256((__m256i *)in, v);
  for(i = 0; i < 16; i++)
    out[i] = montgomery_reduce((int32_t)in[i] * zeta);
  return _mm256_load_si256((__m256i *)out);
}

static __m256i barrett_ct(__m256i v)
{
  int16_t in[16] __attribute__((aligned(32)));
  int16_t out[16] __attribute__((aligned(32)));
  unsigned int i;

  _mm256_store_si256((__m256i *)in, v);
  for(i = 0; i < 16; i++)
    out[i] = barrett_reduce(in[i]);
  return _mm256_load_si256((__m256i *)out);
}

/* Cooley-Tukey butterfly: (u,v) -> (u+t, u-t) with t = fqmul(zeta, v). */
static void butterfly_block(int16_t *r, unsigned start, unsigned len, int16_t zeta)
{
  unsigned int j;

  if(len >= 16) {
    for(j = start; j < start + len; j += 16) {
      __m256i u = _mm256_load_si256((__m256i *)&r[j]);
      __m256i v = _mm256_load_si256((__m256i *)&r[j + len]);
      __m256i t = fqmul_zeta_ct(v, zeta);
      _mm256_store_si256((__m256i *)&r[j], _mm256_add_epi16(u, t));
      _mm256_store_si256((__m256i *)&r[j + len], _mm256_sub_epi16(u, t));
    }
    for(; j < start + len; j++) {
      int16_t t = montgomery_reduce((int32_t)zeta * r[j + len]);
      int16_t u = r[j];
      r[j] = u + t;
      r[j + len] = u - t;
    }
  } else {
    for(j = start; j < start + len; j++) {
      int16_t t = montgomery_reduce((int32_t)zeta * r[j + len]);
      int16_t u = r[j];
      r[j] = u + t;
      r[j + len] = u - t;
    }
  }
}

/* Gentleman-Sande butterfly for inverse NTT. */
static void inv_butterfly_block(int16_t *r, unsigned start, unsigned len, int16_t zeta)
{
  unsigned int j;

  if(len >= 16) {
    for(j = start; j < start + len; j += 16) {
      __m256i u = _mm256_load_si256((__m256i *)&r[j]);
      __m256i v = _mm256_load_si256((__m256i *)&r[j + len]);
      __m256i sum = _mm256_add_epi16(u, v);
      __m256i diff = _mm256_sub_epi16(u, v);
      _mm256_store_si256((__m256i *)&r[j], barrett_ct(sum));
      _mm256_store_si256((__m256i *)&r[j + len], fqmul_zeta_ct(diff, zeta));
    }
    for(; j < start + len; j++) {
      int16_t u = r[j];
      int16_t v = r[j + len];
      r[j] = barrett_reduce(u + v);
      r[j + len] = montgomery_reduce((int32_t)zeta * (u - v));
    }
  } else {
    for(j = start; j < start + len; j++) {
      int16_t u = r[j];
      int16_t v = r[j + len];
      r[j] = barrett_reduce(u + v);
      r[j + len] = montgomery_reduce((int32_t)zeta * (u - v));
    }
  }
}

void ntt512_avx(int16_t *r)
{
  unsigned int len, start, k;
  int16_t zeta;

  k = 1;
  for(len = 256; len >= 4; len >>= 1) {
    for(start = 0; start < KYBER_N; start += 2 * len) {
      zeta = zetas[k++];
      butterfly_block(r, start, len, zeta);
    }
  }
}

void invntt512_avx(int16_t *r)
{
  unsigned int len, start, k, i;
  int16_t zeta;

  k = 0;
  for(len = 4; len <= 256; len <<= 1) {
    for(start = 0; start < KYBER_N; start += 2 * len) {
      zeta = zetas_inv[k++];
      inv_butterfly_block(r, start, len, zeta);
    }
  }

  for(i = 0; i < KYBER_N; i += 16) {
    __m256i v = _mm256_load_si256((__m256i *)&r[i]);
    _mm256_store_si256((__m256i *)&r[i], fqmul_zeta_ct(v, zetas_inv[127]));
  }
}

void basemul512_avx(int16_t *r, const int16_t *a, const int16_t *b)
{
  unsigned int i;

  for(i = 0; i < KYBER_N / 8; i++) {
    basemul_degree4(&r[8 * i], &a[8 * i], &b[8 * i], zetas[64 + i]);
    basemul_degree4(&r[8 * i + 4], &a[8 * i + 4], &b[8 * i + 4], -zetas[64 + i]);
  }
}

void poly_add512_avx(int16_t *r, const int16_t *a, const int16_t *b)
{
  unsigned int i;
  for(i = 0; i < KYBER_N; i += 16) {
    __m256i f0 = _mm256_load_si256((__m256i *)&a[i]);
    __m256i f1 = _mm256_load_si256((__m256i *)&b[i]);
    _mm256_store_si256((__m256i *)&r[i], _mm256_add_epi16(f0, f1));
  }
}

void poly_sub512_avx(int16_t *r, const int16_t *a, const int16_t *b)
{
  unsigned int i;
  for(i = 0; i < KYBER_N; i += 16) {
    __m256i f0 = _mm256_load_si256((__m256i *)&a[i]);
    __m256i f1 = _mm256_load_si256((__m256i *)&b[i]);
    _mm256_store_si256((__m256i *)&r[i], _mm256_sub_epi16(f0, f1));
  }
}

#endif /* WEAVER_USE_AVX_NTT512 */
