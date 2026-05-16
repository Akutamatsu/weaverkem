#include <stdint.h>
#if defined(__AVX2__)
#include <immintrin.h>
#endif
#include "params.h"
#include "cbd.h"

/*************************************************
* Name:        load32_littleendian
*
* Description: load 4 bytes into a 32-bit integer
*              in little-endian order
*
* Arguments:   - const uint8_t *x: pointer to input byte array
*
* Returns 32-bit unsigned integer loaded from x
**************************************************/
static uint32_t load32_littleendian(const uint8_t x[4])
{
  uint32_t r;
  r  = (uint32_t)x[0];
  r |= (uint32_t)x[1] << 8;
  r |= (uint32_t)x[2] << 16;
  r |= (uint32_t)x[3] << 24;
  return r;
}

/*************************************************
* Name:        load24_littleendian
*
* Description: load 3 bytes into a 32-bit integer
*              in little-endian order
*              This function is only needed for Kyber-512
*
* Arguments:   - const uint8_t *x: pointer to input byte array
*
* Returns 32-bit unsigned integer loaded from x (most significant byte is zero)
**************************************************/
#if KYBER_ETA1 == 3
static uint32_t load24_littleendian(const uint8_t x[3])
{
  uint32_t r;
  r  = (uint32_t)x[0];
  r |= (uint32_t)x[1] << 8;
  r |= (uint32_t)x[2] << 16;
  return r;
}
#endif


/*************************************************
* Name:        cbd2
*
* Description: Given an array of uniformly random bytes, compute
*              polynomial with coefficients distributed according to
*              a centered binomial distribution with parameter eta=2
*
* Arguments:   - poly *r:            pointer to output polynomial
*              - const uint8_t *buf: pointer to input byte array
**************************************************/
#if defined(__AVX2__)
/*
 * AVX2 vectorized cbd2 (CT-safe).
 *
 * Per input byte b (8 random bits = 2 coefficients):
 *   coeff_lo = popcount(b[0,1]) - popcount(b[2,3])
 *   coeff_hi = popcount(b[4,5]) - popcount(b[6,7])
 * Result range {-2,-1,0,1,2}.
 *
 * Processes 32 input bytes (= 64 output coefficients) per iteration.
 */
static void cbd2_avx(poly *r, const uint8_t buf[2*KYBER_N/4])
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
#endif

static void cbd2(poly *r, const uint8_t buf[2*KYBER_N/4])
{
#if defined(__AVX2__)
  cbd2_avx(r, buf);
#else
  unsigned int i,j;
  uint32_t t,d;
  int16_t a,b;

  for(i=0;i<KYBER_N/8;i++) {
    t  = load32_littleendian(buf+4*i);
    d  = t & 0x55555555;
    d += (t>>1) & 0x55555555;

    for(j=0;j<8;j++) {
      a = (d >> (4*j+0)) & 0x3;
      b = (d >> (4*j+2)) & 0x3;
      r->coeffs[8*i+j] = a - b;
    }
  }
#endif
}

/*************************************************
* Name:        cbd3
*
* Description: Given an array of uniformly random bytes, compute
*              polynomial with coefficients distributed according to
*              a centered binomial distribution with parameter eta=3
*              This function is only needed for Kyber-512
*
* Arguments:   - poly *r:            pointer to output polynomial
*              - const uint8_t *buf: pointer to input byte array
**************************************************/
#if KYBER_ETA1 == 3
static void cbd3(poly *r, const uint8_t buf[3*KYBER_N/4])
{
  unsigned int i,j;
  uint32_t t,d;
  int16_t a,b;

  for(i=0;i<KYBER_N/4;i++) {
    t  = load24_littleendian(buf+3*i);
    d  = t & 0x00249249;
    d += (t>>1) & 0x00249249;
    d += (t>>2) & 0x00249249;

    for(j=0;j<4;j++) {
      a = (d >> (6*j+0)) & 0x7;
      b = (d >> (6*j+3)) & 0x7;
      r->coeffs[4*i+j] = a - b;
    }
  }
}
#endif

/*************************************************
* Name:        cbd4
*
* Description: Given an array of uniformly random bytes, compute
*              polynomial with coefficients distributed according to
*              a centered binomial distribution with parameter eta=4
*              This function is needed for WEAVER-512
*
* Arguments:   - poly *r:            pointer to output polynomial
*              - const uint8_t *buf: pointer to input byte array
**************************************************/
#if KYBER_ETA1 == 4
static void cbd4(poly *r, const uint8_t buf[4*KYBER_N/4])
{
  unsigned int i,j;
  uint32_t t,d;
  int16_t a,b;

  for(i=0;i<KYBER_N/4;i++) {
    t  = load32_littleendian(buf+4*i);
    d  = t & 0x11111111;
    d += (t>>1) & 0x11111111;
    d += (t>>2) & 0x11111111;
    d += (t>>3) & 0x11111111;

    for(j=0;j<4;j++) {
      a = (d >> (8*j+0)) & 0xf;
      b = (d >> (8*j+4)) & 0xf;
      r->coeffs[4*i+j] = a - b;
    }
  }
}
#endif

/* cbd1: eta=1, 1 bit per sample, 2 bits per coefficient */
#if KYBER_ETA1 == 1
#if defined(__AVX2__)
/*
 * AVX2 vectorized cbd1.
 *
 * Each input byte b produces 4 output coefficients:
 *   c[4k+m] = ((b >> (2m+0)) & 1) - ((b >> (2m+1)) & 1)  for m in 0..3
 * Result is in {-1, 0, 1}.
 *
 * Constant-time: only AVX2 arithmetic + shuffle with constant indices, no
 * data-dependent loads. We process 8 input bytes (= 32 coefficients) per
 * iteration via byte-broadcast + bit-mask compare, then sign-extend to int16.
 */
static void cbd1_avx(poly *r, const uint8_t buf[KYBER_N/4])
{
    const __m256i a_mask = _mm256_set_epi8(
        0x40, 0x10, 0x04, 0x01,
        0x40, 0x10, 0x04, 0x01,
        0x40, 0x10, 0x04, 0x01,
        0x40, 0x10, 0x04, 0x01,
        0x40, 0x10, 0x04, 0x01,
        0x40, 0x10, 0x04, 0x01,
        0x40, 0x10, 0x04, 0x01,
        0x40, 0x10, 0x04, 0x01
    );
    const __m256i b_mask = _mm256_set_epi8(
        (char)0x80, 0x20, 0x08, 0x02,
        (char)0x80, 0x20, 0x08, 0x02,
        (char)0x80, 0x20, 0x08, 0x02,
        (char)0x80, 0x20, 0x08, 0x02,
        (char)0x80, 0x20, 0x08, 0x02,
        (char)0x80, 0x20, 0x08, 0x02,
        (char)0x80, 0x20, 0x08, 0x02,
        (char)0x80, 0x20, 0x08, 0x02
    );
    /* Replicate each of 8 input bytes into 4 lanes; high 128 picks bytes 4..7,
       low 128 picks bytes 0..3 (because shuffle is per-128-bit lane). */
    const __m256i replicate_idx = _mm256_set_epi8(
        7,7,7,7, 6,6,6,6, 5,5,5,5, 4,4,4,4,
        3,3,3,3, 2,2,2,2, 1,1,1,1, 0,0,0,0
    );
    unsigned int i;

    for(i = 0; i < KYBER_N / 32; i++) {
        uint64_t b8 = ((uint64_t)buf[8*i + 0])
                    | ((uint64_t)buf[8*i + 1] << 8)
                    | ((uint64_t)buf[8*i + 2] << 16)
                    | ((uint64_t)buf[8*i + 3] << 24)
                    | ((uint64_t)buf[8*i + 4] << 32)
                    | ((uint64_t)buf[8*i + 5] << 40)
                    | ((uint64_t)buf[8*i + 6] << 48)
                    | ((uint64_t)buf[8*i + 7] << 56);
        __m256i src   = _mm256_set1_epi64x((long long)b8);
        __m256i bytes = _mm256_shuffle_epi8(src, replicate_idx);
        __m256i a_and = _mm256_and_si256(bytes, a_mask);
        __m256i b_and = _mm256_and_si256(bytes, b_mask);
        __m256i cmp_a = _mm256_cmpeq_epi8(a_and, a_mask);  /* 0xFF if a-bit set */
        __m256i cmp_b = _mm256_cmpeq_epi8(b_and, b_mask);  /* 0xFF if b-bit set */

        /* coeff = cmp_b - cmp_a (gives {-1, 0, +1} as int8) */
        __m256i c8 = _mm256_sub_epi8(cmp_b, cmp_a);
        __m256i c16_lo = _mm256_cvtepi8_epi16(_mm256_castsi256_si128(c8));
        __m256i c16_hi = _mm256_cvtepi8_epi16(_mm256_extracti128_si256(c8, 1));

        _mm256_storeu_si256((__m256i *)&r->coeffs[32*i +  0], c16_lo);
        _mm256_storeu_si256((__m256i *)&r->coeffs[32*i + 16], c16_hi);
    }
}
#endif

static void cbd1(poly *r, const uint8_t buf[1*KYBER_N/4])
{
#if defined(__AVX2__)
  cbd1_avx(r, buf);
#else
  unsigned int i,j;
  uint32_t t;
  int16_t a,b;

  for(i=0;i<KYBER_N/16;i++) {
    t = load32_littleendian(buf + 4*i);
    for(j=0;j<16;j++) {
      a = (t >> (2*j+0)) & 0x1;
      b = (t >> (2*j+1)) & 0x1;
      r->coeffs[16*i+j] = a - b;
    }
  }
#endif
}
#endif

#if KYBER_ETA1 == 5
static unsigned int popcount5(uint16_t x)
{
  x &= 0x1F;
  return (x & 1u) + ((x >> 1) & 1u) + ((x >> 2) & 1u) + ((x >> 3) & 1u) + ((x >> 4) & 1u);
}

#if defined(__AVX2__)
/*
 * AVX2 vectorized cbd5 (CT-safe).
 *
 * We keep the same 5-byte -> 4 coefficients bit mapping as the scalar code,
 * batch 16 coefficients at a time, and compute popcount5 via nibble-popcount
 * shuffle with constant lookup tables.
 */
static void cbd5_avx(poly *r, const uint8_t buf[5*KYBER_N/4])
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
#endif

static void cbd5(poly *r, const uint8_t buf[5*KYBER_N/4])
{
  unsigned int i, j;

  for(i = 0; i < KYBER_N/4; i++) {
    uint64_t t = (uint64_t)buf[5*i + 0]
               | ((uint64_t)buf[5*i + 1] << 8)
               | ((uint64_t)buf[5*i + 2] << 16)
               | ((uint64_t)buf[5*i + 3] << 24)
               | ((uint64_t)buf[5*i + 4] << 32);

    for(j = 0; j < 4; j++) {
      uint16_t v = (t >> (10*j)) & 0x3FF;
      int16_t a = (int16_t)popcount5(v);
      int16_t b = (int16_t)popcount5(v >> 5);
      r->coeffs[4*i + j] = a - b;
    }
  }
}
#endif

void cbd_eta1(poly *r, const uint8_t buf[KYBER_ETA1*KYBER_N/4])
{
#if KYBER_ETA1 == 1
  cbd1(r, buf);
#elif KYBER_ETA1 == 2
  cbd2(r, buf);
#elif KYBER_ETA1 == 3
  cbd3(r, buf);
#elif KYBER_ETA1 == 4
  cbd4(r, buf);
#elif KYBER_ETA1 == 5
  cbd5(r, buf);
#else
#error "This implementation requires eta1 in {1,2,3,4,5}"
#endif
}

