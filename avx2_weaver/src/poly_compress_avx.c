/* AVX2 compress/decompress for n=256 (10-bit pk/ct polyvec, 4-bit v). */
#include <stdint.h>
#include <immintrin.h>
#include "params.h"

#if defined(WEAVER_USE_AVX_COMPRESS) && (KYBER_N == 256)
#include "poly.h"
#include "poly_compress_avx.h"
#include "avx/consts.h"

void poly_compress10_avx(uint8_t r[(KYBER_N * 10) / 8], const poly * restrict a)
{
  unsigned int i;
  __m256i f0, f1, f2;
  __m128i t0, t1;
  const __m256i v = _mm256_load_si256((__m256i *)&qdata[_16XV]);
  const __m256i v8 = _mm256_slli_epi16(v, 3);
  const __m256i off = _mm256_set1_epi16(15);
  const __m256i shift1 = _mm256_set1_epi16(1 << 12);
  const __m256i mask = _mm256_set1_epi16(1023);
  const __m256i shift2 = _mm256_set1_epi64x((1024LL << 48) + (1LL << 32) + (1024 << 16) + 1);
  const __m256i sllvdidx = _mm256_set1_epi64x(12);
  const __m256i shufbidx = _mm256_set_epi8( 8, 4, 3, 2, 1, 0,-1,-1,-1,-1,-1,-1,12,11,10, 9,
                                           -1,-1,-1,-1,-1,-1,12,11,10, 9, 8, 4, 3, 2, 1, 0);

  for(i = 0; i < KYBER_N / 16; i++) {
    f0 = _mm256_load_si256((__m256i *)&a->coeffs[16 * i]);
    f1 = _mm256_mullo_epi16(f0, v8);
    f2 = _mm256_add_epi16(f0, off);
    f0 = _mm256_slli_epi16(f0, 3);
    f0 = _mm256_mulhi_epi16(f0, v);
    f2 = _mm256_sub_epi16(f1, f2);
    f1 = _mm256_andnot_si256(f1, f2);
    f1 = _mm256_srli_epi16(f1, 15);
    f0 = _mm256_sub_epi16(f0, f1);
    f0 = _mm256_mulhrs_epi16(f0, shift1);
    f0 = _mm256_and_si256(f0, mask);
    f0 = _mm256_madd_epi16(f0, shift2);
    f0 = _mm256_sllv_epi32(f0, sllvdidx);
    f0 = _mm256_srli_epi64(f0, 12);
    f0 = _mm256_shuffle_epi8(f0, shufbidx);
    t0 = _mm256_castsi256_si128(f0);
    t1 = _mm256_extracti128_si256(f0, 1);
    t0 = _mm_blend_epi16(t0, t1, 0xE0);
    _mm_storeu_si128((__m128i *)&r[20 * i + 0], t0);
    _mm_store_ss((float *)&r[20 * i + 16], _mm_castsi128_ps(t1));
  }
}

void poly_decompress10_avx(poly * restrict r, const uint8_t a[(KYBER_N * 10) / 8])
{
  unsigned int i;
  __m256i f;
  const __m256i q = _mm256_set1_epi32((KYBER_Q << 16) + 4 * KYBER_Q);
  const __m256i shufbidx = _mm256_set_epi8(11,10,10, 9, 9, 8, 8, 7,
                                            6, 5, 5, 4, 4, 3, 3, 2,
                                            9, 8, 8, 7, 7, 6, 6, 5,
                                            4, 3, 3, 2, 2, 1, 1, 0);
  const __m256i sllvdidx = _mm256_set1_epi64x(4);
  const __m256i mask = _mm256_set1_epi32((32736 << 16) + 8184);

  for(i = 0; i < KYBER_N / 16; i++) {
    f = _mm256_loadu_si256((__m256i *)&a[20 * i]);
    f = _mm256_permute4x64_epi64(f, 0x94);
    f = _mm256_shuffle_epi8(f, shufbidx);
    f = _mm256_sllv_epi32(f, sllvdidx);
    f = _mm256_srli_epi16(f, 1);
    f = _mm256_and_si256(f, mask);
    f = _mm256_mulhrs_epi16(f, q);
    _mm256_store_si256((__m256i *)&r->coeffs[16 * i], f);
  }
}

#if (KYBER_POLYCOMPRESSEDBYTES == (KYBER_N * 4 / 8))

void poly_compress_d4_avx(uint8_t r[KYBER_POLYCOMPRESSEDBYTES], const poly * restrict a)
{
  unsigned int i;
  __m256i f0, f1, f2, f3;
  const __m256i v = _mm256_load_si256((__m256i *)&qdata[_16XV]);
  const __m256i shift1 = _mm256_set1_epi16(1 << 9);
  const __m256i mask = _mm256_set1_epi16(15);
  const __m256i shift2 = _mm256_set1_epi16((16 << 8) + 1);
  const __m256i permdidx = _mm256_set_epi32(7, 3, 6, 2, 5, 1, 4, 0);

  for(i = 0; i < KYBER_N / 64; i++) {
    f0 = _mm256_load_si256((__m256i *)&a->coeffs[64 * i + 0]);
    f1 = _mm256_load_si256((__m256i *)&a->coeffs[64 * i + 16]);
    f2 = _mm256_load_si256((__m256i *)&a->coeffs[64 * i + 32]);
    f3 = _mm256_load_si256((__m256i *)&a->coeffs[64 * i + 48]);
    f0 = _mm256_mulhi_epi16(f0, v);
    f1 = _mm256_mulhi_epi16(f1, v);
    f2 = _mm256_mulhi_epi16(f2, v);
    f3 = _mm256_mulhi_epi16(f3, v);
    f0 = _mm256_mulhrs_epi16(f0, shift1);
    f1 = _mm256_mulhrs_epi16(f1, shift1);
    f2 = _mm256_mulhrs_epi16(f2, shift1);
    f3 = _mm256_mulhrs_epi16(f3, shift1);
    f0 = _mm256_and_si256(f0, mask);
    f1 = _mm256_and_si256(f1, mask);
    f2 = _mm256_and_si256(f2, mask);
    f3 = _mm256_and_si256(f3, mask);
    f0 = _mm256_packus_epi16(f0, f1);
    f2 = _mm256_packus_epi16(f2, f3);
    f0 = _mm256_maddubs_epi16(f0, shift2);
    f2 = _mm256_maddubs_epi16(f2, shift2);
    f0 = _mm256_packus_epi16(f0, f2);
    f0 = _mm256_permutevar8x32_epi32(f0, permdidx);
    _mm256_storeu_si256((__m256i *)&r[32 * i], f0);
  }
}

void poly_decompress_d4_avx(poly * restrict r, const uint8_t a[KYBER_POLYCOMPRESSEDBYTES])
{
  unsigned int i;
  __m256i f;
  const __m256i q = _mm256_load_si256((__m256i *)&qdata[_16XQ]);
  const __m256i shufbidx = _mm256_set_epi8(7,7,7,7,6,6,6,6,5,5,5,5,4,4,4,4,
                                           3,3,3,3,2,2,2,2,1,1,1,1,0,0,0,0);
  const __m256i mask = _mm256_set1_epi32(0x00F0000F);
  const __m256i shift = _mm256_set1_epi32((128 << 16) + 2048);

  for(i = 0; i < KYBER_N / 16; i++) {
    f = _mm256_broadcastq_epi64(_mm_loadl_epi64((__m128i *)&a[8 * i]));
    f = _mm256_shuffle_epi8(f, shufbidx);
    f = _mm256_and_si256(f, mask);
    f = _mm256_mullo_epi16(f, shift);
    f = _mm256_mulhrs_epi16(f, q);
    _mm256_store_si256((__m256i *)&r->coeffs[16 * i], f);
  }
}

#endif /* 4-bit v */

#if (KYBER_PK_POLYVECBYTES == (KYBER_K * KYBER_N * 9 / 8))

#include "poly_compress9.h"

void poly_compress9_quant_avx(uint16_t t[KYBER_N], const poly *a)
{
  unsigned int i, k;
  const __m256i qv = _mm256_set1_epi32(KYBER_Q);
  const __m256i halfv = _mm256_set1_epi32(KYBER_Q / 2);
  uint32_t num[8];

  for(i = 0; i < KYBER_N; i += 8) {
    __m128i c = _mm_loadu_si128((__m128i *)&a->coeffs[i]);
    __m256i u = _mm256_cvtepi16_epi32(c);
    __m256i neg = _mm256_srai_epi32(u, 31);
    u = _mm256_add_epi32(u, _mm256_and_si256(neg, qv));
    u = _mm256_and_si256(u, _mm256_set1_epi32(0xffff));
    u = _mm256_slli_epi32(u, 9);
    u = _mm256_add_epi32(u, halfv);
    _mm256_storeu_si256((__m256i *)num, u);
    for(k = 0; k < 8; k++)
      t[i + k] = (uint16_t)((num[k] / KYBER_Q) & 0x1ff);
  }
}

void poly_compress9_avx(uint8_t r[(KYBER_N * 9) / 8], const poly *a)
{
  unsigned int j;
  uint16_t t[KYBER_N];

  poly_compress9_quant_avx(t, a);
  for(j = 0; j < KYBER_N / 8; j++)
    poly_compress9_pack8(r + 9 * j, t + 8 * j);
}

#endif /* 9-bit pk */

#endif /* WEAVER_USE_AVX_COMPRESS */
