#include <stdint.h>
#include <immintrin.h>
#include "params.h"
#include "cbd.h"

/* All four parameter sets use AVX2 sampling: eta=5 for mode 1,
 * eta=2 for modes 3/5, eta=3 for modes 7. */
#if KYBER_ETA1 == 2
static void cbd_sample(poly *r, const uint8_t buf[2*KYBER_N/4])
{
    const __m256i mask55 = _mm256_set1_epi8(0x55);
    const __m256i mask03 = _mm256_set1_epi8(0x03);
    unsigned int i;

    for(i = 0; i < KYBER_N / 64; i++) {
        __m256i raw = _mm256_loadu_si256((const __m256i *)&buf[32*i]);
        __m256i shifted1 = _mm256_srli_epi16(raw, 1);
        __m256i pop_lo = _mm256_and_si256(raw, mask55);
        __m256i pop_hi = _mm256_and_si256(shifted1, mask55);
        __m256i d = _mm256_add_epi8(pop_lo, pop_hi);     /* per-byte 4 popcounts in 2-bit slots */

        /* coeff_lo per byte = (d & 0x03) - ((d >> 2) & 0x03)  in {-2..2} */
        __m256i a_lo = _mm256_and_si256(d, mask03);
        __m256i b_lo = _mm256_and_si256(_mm256_srli_epi16(d, 2), mask03);
        __m256i diff_lo = _mm256_sub_epi8(a_lo, b_lo);

        /* coeff_hi per byte = ((d >> 4) & 0x03) - ((d >> 6) & 0x03) */
        __m256i a_hi = _mm256_and_si256(_mm256_srli_epi16(d, 4), mask03);
        __m256i b_hi = _mm256_and_si256(_mm256_srli_epi16(d, 6), mask03);
        __m256i diff_hi = _mm256_sub_epi8(a_hi, b_hi);

        /* Interleave so output is [diff_lo[0], diff_hi[0], diff_lo[1], ...] */
        __m256i interlo = _mm256_unpacklo_epi8(diff_lo, diff_hi);
        __m256i interhi = _mm256_unpackhi_epi8(diff_lo, diff_hi);

        /* AVX2 unpack works per-128-bit lane:
         *   interlo low 128 = bytes coming from diff_lo[0..7], diff_hi[0..7]  (coeffs 0..15)
         *   interhi low 128 = bytes coming from diff_lo[8..15], diff_hi[8..15] (coeffs 16..31)
         *   interlo high 128 = bytes from diff_lo[16..23], diff_hi[16..23]    (coeffs 32..47)
         *   interhi high 128 = bytes from diff_lo[24..31], diff_hi[24..31]    (coeffs 48..63)
         */
        __m256i c0 = _mm256_cvtepi8_epi16(_mm256_castsi256_si128(interlo));
        __m256i c1 = _mm256_cvtepi8_epi16(_mm256_castsi256_si128(interhi));
        __m256i c2 = _mm256_cvtepi8_epi16(_mm256_extracti128_si256(interlo, 1));
        __m256i c3 = _mm256_cvtepi8_epi16(_mm256_extracti128_si256(interhi, 1));

        _mm256_storeu_si256((__m256i *)&r->coeffs[64*i +  0], c0);
        _mm256_storeu_si256((__m256i *)&r->coeffs[64*i + 16], c1);
        _mm256_storeu_si256((__m256i *)&r->coeffs[64*i + 32], c2);
        _mm256_storeu_si256((__m256i *)&r->coeffs[64*i + 48], c3);
    }
}
#elif KYBER_ETA1 == 3
static void cbd_sample(poly *r, const uint8_t buf[6*KYBER_N/8])
{
  unsigned int i;
  __m256i f0, f1, f2, f3;
  const __m256i mask249 = _mm256_set1_epi32(0x249249);
  const __m256i mask6DB = _mm256_set1_epi32(0x6DB6DB);
  const __m256i mask07 = _mm256_set1_epi32(7);
  const __m256i mask70 = _mm256_set1_epi32(7 << 16);
  const __m256i mask3 = _mm256_set1_epi16(3);
  const __m256i shufbidx = _mm256_set_epi8(-1,15,14,13,-1,12,11,10,-1, 9, 8, 7,-1, 6, 5, 4,
                                           -1,11,10, 9,-1, 8, 7, 6,-1, 5, 4, 3,-1, 2, 1, 0);

  for(i = 0; i < KYBER_N/32; i++) {
    f0 = _mm256_loadu_si256((__m256i *)&buf[24*i]);
    f0 = _mm256_permute4x64_epi64(f0,0x94);
    f0 = _mm256_shuffle_epi8(f0,shufbidx);

    f1 = _mm256_srli_epi32(f0,1);
    f2 = _mm256_srli_epi32(f0,2);
    f0 = _mm256_and_si256(mask249,f0);
    f1 = _mm256_and_si256(mask249,f1);
    f2 = _mm256_and_si256(mask249,f2);
    f0 = _mm256_add_epi32(f0,f1);
    f0 = _mm256_add_epi32(f0,f2);

    f1 = _mm256_srli_epi32(f0,3);
    f0 = _mm256_add_epi32(f0,mask6DB);
    f0 = _mm256_sub_epi32(f0,f1);

    f1 = _mm256_slli_epi32(f0,10);
    f2 = _mm256_srli_epi32(f0,12);
    f3 = _mm256_srli_epi32(f0, 2);
    f0 = _mm256_and_si256(f0,mask07);
    f1 = _mm256_and_si256(f1,mask70);
    f2 = _mm256_and_si256(f2,mask07);
    f3 = _mm256_and_si256(f3,mask70);
    f0 = _mm256_add_epi16(f0,f1);
    f1 = _mm256_add_epi16(f2,f3);
    f0 = _mm256_sub_epi16(f0,mask3);
    f1 = _mm256_sub_epi16(f1,mask3);

    f2 = _mm256_unpacklo_epi32(f0,f1);
    f3 = _mm256_unpackhi_epi32(f0,f1);

    f0 = _mm256_permute2x128_si256(f2,f3,0x20);
    f1 = _mm256_permute2x128_si256(f2,f3,0x31);

    _mm256_store_si256((__m256i *)&r->coeffs[32*i+ 0], f0);
    _mm256_store_si256((__m256i *)&r->coeffs[32*i+16], f1);
  }
}

#elif KYBER_ETA1 == 5
static void cbd_sample(poly *r, const uint8_t buf[5*KYBER_N/4])
{
  const __m256i nibble_mask = _mm256_set1_epi8(0x0F);
  const __m256i popcnt_nibble = _mm256_setr_epi8(
      0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4,
      0,1,1,2,1,2,2,3,1,2,2,3,2,3,3,4
  );
  unsigned int i, k;

  for(i = 0; i < KYBER_N/16; i++) {
    uint8_t a5[32] = {0};
    uint8_t b5[32] = {0};
    const uint8_t *in = buf + 20*i;

    for(k = 0; k < 4; k++) {
      uint64_t t = (uint64_t)in[0]
                 | ((uint64_t)in[1] << 8)
                 | ((uint64_t)in[2] << 16)
                 | ((uint64_t)in[3] << 24)
                 | ((uint64_t)in[4] << 32);
      uint16_t v0 = (t >> 0)  & 0x3FF;
      uint16_t v1 = (t >> 10) & 0x3FF;
      uint16_t v2 = (t >> 20) & 0x3FF;
      uint16_t v3 = (t >> 30) & 0x3FF;
      unsigned int o = 4*k;

      a5[o + 0] = (uint8_t)(v0 & 0x1F);
      a5[o + 1] = (uint8_t)(v1 & 0x1F);
      a5[o + 2] = (uint8_t)(v2 & 0x1F);
      a5[o + 3] = (uint8_t)(v3 & 0x1F);
      b5[o + 0] = (uint8_t)(v0 >> 5);
      b5[o + 1] = (uint8_t)(v1 >> 5);
      b5[o + 2] = (uint8_t)(v2 >> 5);
      b5[o + 3] = (uint8_t)(v3 >> 5);
      in += 5;
    }

    {
      __m256i av = _mm256_loadu_si256((const __m256i *)a5);
      __m256i bv = _mm256_loadu_si256((const __m256i *)b5);
      __m256i av_lo = _mm256_and_si256(av, nibble_mask);
      __m256i av_hi = _mm256_and_si256(_mm256_srli_epi16(av, 4), nibble_mask);
      __m256i bv_lo = _mm256_and_si256(bv, nibble_mask);
      __m256i bv_hi = _mm256_and_si256(_mm256_srli_epi16(bv, 4), nibble_mask);
      __m256i ap = _mm256_add_epi8(_mm256_shuffle_epi8(popcnt_nibble, av_lo),
                                   _mm256_shuffle_epi8(popcnt_nibble, av_hi));
      __m256i bp = _mm256_add_epi8(_mm256_shuffle_epi8(popcnt_nibble, bv_lo),
                                   _mm256_shuffle_epi8(popcnt_nibble, bv_hi));
      __m128i d8 = _mm256_castsi256_si128(_mm256_sub_epi8(ap, bp));
      __m256i d16 = _mm256_cvtepi8_epi16(d8);
      _mm256_storeu_si256((__m256i *)&r->coeffs[16*i], d16);
    }
  }
}
#else
#error "WEAVER requires eta1 in {2,5}"
#endif

void cbd_eta1(poly *r, const uint8_t buf[KYBER_ETA1*KYBER_N/4])
{
  cbd_sample(r, buf);
}
