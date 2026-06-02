/* AVX2 polynomial arithmetic (NTT domain) for Weaver n=256 parameter sets. */
#include <stdint.h>
#include <immintrin.h>
#include "params.h"
#include "poly.h"
#include "avx/consts.h"
#include "ntt_avx.h"
#include "ntt.h"
#include "reduce.h"

#if defined(WEAVER_AVX256_NTT)

#ifndef WEAVER_AVX_USE_NTT
#define WEAVER_AVX_USE_NTT 1
#endif
#ifndef WEAVER_AVX_USE_INVNTT
#define WEAVER_AVX_USE_INVNTT 1
#endif
#ifndef WEAVER_AVX_USE_BASEMUL
#define WEAVER_AVX_USE_BASEMUL 1
#endif
#ifndef WEAVER_AVX_USE_TOMONT
#define WEAVER_AVX_USE_TOMONT 1
#endif
#ifndef WEAVER_AVX_USE_REDUCE
#define WEAVER_AVX_USE_REDUCE 1
#endif
#ifndef WEAVER_AVX_USE_ADD_SUB
#define WEAVER_AVX_USE_ADD_SUB 1
#endif
#ifndef WEAVER_AVX_POST_NTT_REDUCE
#define WEAVER_AVX_POST_NTT_REDUCE 0
#endif

void poly_tobytes(uint8_t r[WEAVER_POLYBYTES], const poly *a)
{
  ntttobytes_avx(r, a->coeffs, qdata);
}

void poly_frombytes(poly *r, const uint8_t a[WEAVER_POLYBYTES])
{
  nttfrombytes_avx(r->coeffs, a, qdata);
}

void poly_nttunpack(poly *r)
{
  nttunpack_avx(r->coeffs, qdata);
}

void poly_ntt(poly *r)
{
#if WEAVER_AVX_USE_NTT
  ntt_avx(r->coeffs, qdata);
#if WEAVER_AVX_POST_NTT_REDUCE
  reduce_avx(r->coeffs, qdata);
#endif
#else
  ntt(r->coeffs);
  poly_reduce(r);
#endif
}

void poly_invntt_tomont(poly *r)
{
#if WEAVER_AVX_USE_INVNTT
  invntt_avx(r->coeffs, qdata);
#else
  invntt(r->coeffs);
#endif
}

void poly_basemul_montgomery(poly *r, const poly *a, const poly *b)
{
#if WEAVER_AVX_USE_BASEMUL
  basemul_avx(r->coeffs, a->coeffs, b->coeffs, qdata);
#else
  unsigned int i;
  for(i = 0; i < WEAVER_N / 4; i++) {
    basemul(&r->coeffs[4 * i], &a->coeffs[4 * i], &b->coeffs[4 * i], zetas[64 + i]);
    basemul(&r->coeffs[4 * i + 2], &a->coeffs[4 * i + 2], &b->coeffs[4 * i + 2], -zetas[64 + i]);
  }
#endif
}

void poly_tomont(poly *r)
{
#if WEAVER_AVX_USE_TOMONT
  tomont_avx(r->coeffs, qdata);
#else
  unsigned int i;
  const int16_t f = (1ULL << 32) % WEAVER_Q;
  for(i = 0; i < WEAVER_N; i++)
    r->coeffs[i] = montgomery_reduce((int32_t)r->coeffs[i] * f);
#endif
}

void poly_reduce(poly *r)
{
#if WEAVER_AVX_USE_REDUCE
  reduce_avx(r->coeffs, qdata);
#else
  unsigned int i;
  for(i = 0; i < WEAVER_N; i++)
    r->coeffs[i] = barrett_reduce(r->coeffs[i]);
#endif
}

void poly_add(poly *r, const poly *a, const poly *b)
{
#if WEAVER_AVX_USE_ADD_SUB
  unsigned int i;
  __m256i f0, f1;

  for(i = 0; i < WEAVER_N; i += 16) {
    f0 = _mm256_load_si256((__m256i *)&a->coeffs[i]);
    f1 = _mm256_load_si256((__m256i *)&b->coeffs[i]);
    f0 = _mm256_add_epi16(f0, f1);
    _mm256_store_si256((__m256i *)&r->coeffs[i], f0);
  }
#else
  unsigned int i;
  for(i = 0; i < WEAVER_N; i++)
    r->coeffs[i] = a->coeffs[i] + b->coeffs[i];
#endif
}

void poly_sub(poly *r, const poly *a, const poly *b)
{
#if WEAVER_AVX_USE_ADD_SUB
  unsigned int i;
  __m256i f0, f1;

  for(i = 0; i < WEAVER_N; i += 16) {
    f0 = _mm256_load_si256((__m256i *)&a->coeffs[i]);
    f1 = _mm256_load_si256((__m256i *)&b->coeffs[i]);
    f0 = _mm256_sub_epi16(f0, f1);
    _mm256_store_si256((__m256i *)&r->coeffs[i], f0);
  }
#else
  unsigned int i;
  for(i = 0; i < WEAVER_N; i++)
    r->coeffs[i] = a->coeffs[i] - b->coeffs[i];
#endif
}

#endif /* WEAVER_AVX256_NTT */
