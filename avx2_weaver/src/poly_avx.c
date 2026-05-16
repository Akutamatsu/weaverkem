/* AVX2 polynomial arithmetic (NTT domain) for Weaver n=256 parameter sets. */
#include <stdint.h>
#include <immintrin.h>
#include "params.h"
#include "poly.h"
#include "avx/consts.h"
#include "ntt_avx.h"
#include "reduce.h"

#if defined(WEAVER_AVX256_NTT)

void poly_ntt(poly *r)
{
  unsigned int i;
  ntt_avx(r->coeffs, qdata);
  for(i = 0; i < KYBER_N; i++)
    r->coeffs[i] = barrett_reduce(r->coeffs[i]);
}

void poly_invntt_tomont(poly *r)
{
  invntt_avx(r->coeffs, qdata);
}

void poly_basemul_montgomery(poly *r, const poly *a, const poly *b)
{
  basemul_avx(r->coeffs, a->coeffs, b->coeffs, qdata);
}

void poly_tomont(poly *r)
{
  tomont_avx(r->coeffs, qdata);
}

void poly_reduce(poly *r)
{
  reduce_avx(r->coeffs, qdata);
}

void poly_add(poly *r, const poly *a, const poly *b)
{
  unsigned int i;
  __m256i f0, f1;

  for(i = 0; i < KYBER_N; i += 16) {
    f0 = _mm256_load_si256((__m256i *)&a->coeffs[i]);
    f1 = _mm256_load_si256((__m256i *)&b->coeffs[i]);
    f0 = _mm256_add_epi16(f0, f1);
    _mm256_store_si256((__m256i *)&r->coeffs[i], f0);
  }
}

void poly_sub(poly *r, const poly *a, const poly *b)
{
  unsigned int i;
  __m256i f0, f1;

  for(i = 0; i < KYBER_N; i += 16) {
    f0 = _mm256_load_si256((__m256i *)&a->coeffs[i]);
    f1 = _mm256_load_si256((__m256i *)&b->coeffs[i]);
    f0 = _mm256_sub_epi16(f0, f1);
    _mm256_store_si256((__m256i *)&r->coeffs[i], f0);
  }
}

#endif /* WEAVER_AVX256_NTT */
