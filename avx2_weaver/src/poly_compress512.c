/* Exact AVX2 quantization for mode 7, in standard coefficient order. */
#include <stdint.h>
#include <immintrin.h>
#include "params.h"
#include "poly.h"
#include "poly_compress_avx.h"

#if (KYBER_N == 512)
static __m256i quant8(__m128i x, unsigned d)
{
  __m256i u = _mm256_cvtepi16_epi32(x);
  u = _mm256_add_epi32(u, _mm256_and_si256(_mm256_srai_epi32(u,31),
                                         _mm256_set1_epi32(KYBER_Q)));
  u = _mm256_add_epi32(_mm256_sllv_epi32(u,_mm256_set1_epi32(d)),
                       _mm256_set1_epi32(KYBER_Q/2));
  /* ceil(2^32 / 3329): exact quotient for these numerators (< 2^22). */
  const __m256i reciprocal = _mm256_set1_epi32(1290168);
  __m256i even = _mm256_srli_epi64(_mm256_mul_epu32(u,reciprocal),32);
  __m256i odd = _mm256_mul_epu32(_mm256_srli_epi64(u,32),reciprocal);
  odd = _mm256_and_si256(odd,_mm256_set1_epi64x((long long)0xffffffff00000000ULL));
  return _mm256_and_si256(_mm256_or_si256(even,odd),
                           _mm256_set1_epi32((1u<<d)-1));
}

static void compress(uint8_t *out, const poly *a, unsigned d)
{
  for(unsigned i=0;i<KYBER_N;i+=8) {
    uint32_t t[8];
    __m256i v=quant8(_mm_loadu_si128((const __m128i *)(a->coeffs+i)),d);
    _mm256_storeu_si256((__m256i *)t,v);
    unsigned acc=0,bits=0;
    for(unsigned j=0;j<8;j++) {
      acc|=t[j]<<bits; bits+=d;
      while(bits>=8) { *out++=(uint8_t)acc; acc>>=8; bits-=8; }
    }
  }
}

static void decompress(poly *r, const uint8_t *in, unsigned d)
{
  for(unsigned i=0;i<KYBER_N;i+=8) {
    uint32_t t[8];
    unsigned acc=0,bits=0;
    for(unsigned j=0;j<8;j++) {
      while(bits<d) { acc|=(unsigned)*in++<<bits; bits+=8; }
      t[j]=acc&((1u<<d)-1); acc>>=d; bits-=d;
    }
    __m256i v=_mm256_loadu_si256((const __m256i *)t);
    v=_mm256_add_epi32(_mm256_mullo_epi32(v,_mm256_set1_epi32(KYBER_Q)),
                        _mm256_set1_epi32(1u<<(d-1)));
    v=_mm256_srlv_epi32(v,_mm256_set1_epi32(d));
    __m128i packed=_mm_packus_epi32(_mm256_castsi256_si128(v),
                                    _mm256_extracti128_si256(v,1));
    _mm_storeu_si128((__m128i *)(r->coeffs+i),packed);
  }
}

void poly_compress10_avx(uint8_t r[(KYBER_N * 10) / 8], const poly *a) { compress(r,a,10); }
void poly_decompress10_avx(poly *r, const uint8_t a[(KYBER_N * 10) / 8]) { decompress(r,a,10); }
void poly_compress_d6_avx(uint8_t r[KYBER_POLYCOMPRESSEDBYTES], const poly *a) { compress(r,a,6); }
void poly_decompress_d6_avx(poly *r, const uint8_t a[KYBER_POLYCOMPRESSEDBYTES]) { decompress(r,a,6); }
#endif
