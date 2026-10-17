/* AVX2 polynomial arithmetic (NTT domain) for Weaver n=256 parameter sets. */
#include <stdint.h>
#include <immintrin.h>
#include "params.h"
#include "poly.h"
#include "avx/consts.h"
#include "ntt_avx.h"

#if (KYBER_N == 256)

void poly_tobytes(uint8_t r[KYBER_POLYBYTES], const poly *a)
{
  ntttobytes_avx(r, a->coeffs, qdata);
}

void poly_frombytes(poly *r, const uint8_t a[KYBER_POLYBYTES])
{
  nttfrombytes_avx(r->coeffs, a, qdata);
}

void poly_nttunpack(poly *r)
{
  nttunpack_avx(r->coeffs, qdata);
}

void poly_ntt(poly *r)
{
  ntt_avx(r->coeffs, qdata);
  reduce_avx(r->coeffs, qdata);
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

#endif
