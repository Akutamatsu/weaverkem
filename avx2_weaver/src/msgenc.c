#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#if defined(__AVX2__)
#include <immintrin.h>
#endif
#include "params.h"
#include "poly.h"
#include "msgenc.h"
#include "reduce.h"
#include "bch.h"

#if defined(__AVX2__)
/*
 * Constant-time AVX2 helper used by all WEAVER modes inside poly_frommsg.
 *
 * Expands WEAVER_N/8 input bytes into WEAVER_N coefficients such that
 *   r[8*i + j] = ((mu_tilde[i] >> (7 - j)) & 1) ? WEAVER_HALFQ : 0
 *
 * MSB-first per byte. Only uses arithmetic / shuffle with constant indices
 * (no data-dependent loads), preserving constant-time behaviour.
 */
static void frommsg_high_bits_avx(int16_t r[WEAVER_N], const uint8_t *mu_tilde)
{
    /* MSB-first per byte: coeff (7-j) uses bit (7-j), lane j mask is 1<<(7-j). */
    const __m256i bit_mask = _mm256_set_epi8(
        0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, (char)0x80,
        0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, (char)0x80,
        0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, (char)0x80,
        0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, (char)0x80
    );
    const __m256i replicate_idx = _mm256_set_epi8(
        3,3,3,3,3,3,3,3,
        2,2,2,2,2,2,2,2,
        1,1,1,1,1,1,1,1,
        0,0,0,0,0,0,0,0
    );
    const __m256i halfq16 = _mm256_set1_epi16(WEAVER_HALFQ);
    unsigned int i;

    for(i = 0; i < WEAVER_N / 32; i++) {
        uint32_t b4 = ((uint32_t)mu_tilde[4*i + 0])
                    | ((uint32_t)mu_tilde[4*i + 1] << 8)
                    | ((uint32_t)mu_tilde[4*i + 2] << 16)
                    | ((uint32_t)mu_tilde[4*i + 3] << 24);
        __m256i src = _mm256_set1_epi32((int32_t)b4);
        __m256i bytes = _mm256_shuffle_epi8(src, replicate_idx);
        __m256i anded = _mm256_and_si256(bytes, bit_mask);
        __m256i cmp   = _mm256_cmpeq_epi8(anded, bit_mask);
        __m256i lo16  = _mm256_cvtepi8_epi16(_mm256_castsi256_si128(cmp));
        __m256i hi16  = _mm256_cvtepi8_epi16(_mm256_extracti128_si256(cmp, 1));

        lo16 = _mm256_and_si256(lo16, halfq16);
        hi16 = _mm256_and_si256(hi16, halfq16);

        _mm256_storeu_si256((__m256i *)&r[32*i],     lo16);
        _mm256_storeu_si256((__m256i *)&r[32*i + 16], hi16);
    }
}
#endif

#if 0
/*************************************************
* Name:        poly_frommsg
*
* Description: Convert 32-byte message to polynomial
*
* Arguments:   - poly *r:            pointer to output polynomial
*              - const uint8_t *msg: pointer to input message
**************************************************/
/* kyber原代码: 不使用纠错, 直接编一层高位 */
void poly_frommsg(poly *r, const uint8_t msg[WEAVER_INDCPA_MSGBYTES])
{
  unsigned int i,j;
  int16_t mask;

#if (WEAVER_INDCPA_MSGBYTES > WEAVER_N/8)
#error "WEAVER_INDCPA_MSGBYTES must be less than WEAVER_N/8 bytes!"
#endif

  for(i=0;i<WEAVER_N/8;i++) {
    for(j=0;j<8;j++) {
      mask = -(int16_t)((msg[i] >> j)&1);
      r->coeffs[8*i+j] = mask & ((WEAVER_Q+1)/2);
    }
  }
}
/* kyber原代码: 不使用纠错, 直接编一层高位 */
void poly_tomsg(uint8_t msg[WEAVER_INDCPA_MSGBYTES], const poly *a)
{
  unsigned int i,j;
  uint16_t t;

  //poly_csubq(a); /* modified barret_reduce */

  for(i=0;i<WEAVER_N/8;i++) {
    msg[i] = 0;
    for(j=0;j<8;j++) {
      t  = a->coeffs[8*i+j];
      // map to positive standard representatives
      t += ((int16_t)t >> 15) & WEAVER_Q;
      t  = (((t << 1) + WEAVER_Q/2)/WEAVER_Q) & 1;
      msg[i] |= t << j;
    }
  }
}
#else

#if WEAVER_MODE == 1 
// Algorithm 3: MsgEncode
/*************************************************
* Name:        poly_frommsg
* Description: Convert message to polynomial using WEAVER multi-level coding
**************************************************/
void poly_frommsg(poly *r, const uint8_t msg[WEAVER_INDCPA_MSGBYTES])
{
    unsigned int i, j;
    int16_t mask;
    uint8_t mu_tilde[WEAVER_N / 8] = { 0 };

    for (i = 0; i < WEAVER_N; i++) {
        r->coeffs[i] = 0;
    }

    // ==========================================================
    // Step 1: Encode to Higher bits
    // ==========================================================
    memcpy(mu_tilde, msg, ELL_BAR_BYTES);
    encode_bch_high(msg, ELL_BAR_BYTES, mu_tilde + ELL_BAR_BYTES);

#if defined(__AVX2__) && defined(WEAVER_EXPERIMENTAL_MSGENC_AVX)
    (void)j; (void)mask;
    frommsg_high_bits_avx(r->coeffs, mu_tilde);
#else
    for (i = 0; i < WEAVER_N / 8; i++) {
        for (j = 0; j < 8; j++) {
            mask = -(int16_t)((mu_tilde[i] >> (7 - j)) & 1);
            r->coeffs[8 * i + j] = mask & WEAVER_HALFQ;
        }
    }
#endif
}
// Algorithm 5: MsgDecode
void poly_tomsg(uint8_t msg[WEAVER_INDCPA_MSGBYTES], const poly *a)
{
    unsigned int i, j;
    int16_t w_bar[WEAVER_N];
    uint8_t mu_tilde[WEAVER_N / 8] = { 0 };
    memset(msg, 0, WEAVER_INDCPA_MSGBYTES);

    for (i = 0; i < WEAVER_N; i++) { // COPY coeffs into w_bar
        int16_t t = a->coeffs[i];
        // map to positive standard representatives: [0, q-1]
        w_bar[i] = t + ((t >> 15) & WEAVER_Q);
    }
    // ==========================================================
    // Phase 3: Decode Higher bits
    // ==========================================================
    for (i = 0; i < WEAVER_N / 8; i++) {
        for (j = 0; j < 8; j++) {
            int16_t t = w_bar[8 * i + j];
            t += ((int16_t)t >> 15) & WEAVER_Q; // map to positive
            t = ((((uint32_t)t << 1) + WEAVER_Q / 2) / WEAVER_Q) & 1;
            mu_tilde[i] |= t << (7 -j);
        }
    }

    // BCH 纠错高位，并存入输出区
    decode_bch_high(mu_tilde, ELL_BAR_BYTES, mu_tilde + ELL_BAR_BYTES);
    memcpy(msg, mu_tilde, ELL_BAR_BYTES);
}

#elif WEAVER_MODE == 3 || WEAVER_MODE == 5

/*************************************************
* Name:        flipabs
*
* Description: Computes |(x mod+ q/2) - q/4|
*
* Arguments:   uint16_t x: input coefficient
*
* Returns |(x mod+ q/2) - q/4|
**************************************************/
static uint16_t flipabs_ex(int16_t x)
{
    int16_t r, m;
    r = barrett_reduce_ex(x);

    r = r - WEAVER_Q / 4;
    m = r >> 15;
    return (r + m) ^ m; // turn to positive
}

#if defined(__AVX2__)
/*
 * D4 encoding helper (CT-safe, AVX2): for each input byte b in src[0..n_bytes),
 * compute v[j] = ((b >> (7-j)) & 1) ? WEAVER_Q/4 : 0 for j in [0,8), then add v
 * to r at offsets 0, D4_STEP_LEN, 2*D4_STEP_LEN, 3*D4_STEP_LEN.
 */
static void frommsg_d4_add_avx(int16_t *r, const uint8_t *src, unsigned int n_bytes)
{
    const __m128i bit_mask = _mm_set_epi8(
        0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, (char)0x80,
        0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, (char)0x80
    );
    const __m128i qquarter = _mm_set1_epi16(WEAVER_Q / 4);
    unsigned int i;

    for(i = 0; i < n_bytes; i++) {
        __m128i b8 = _mm_set1_epi8((char)src[i]);
        __m128i anded = _mm_and_si128(b8, bit_mask);
        __m128i cmp = _mm_cmpeq_epi8(anded, bit_mask);
        __m128i v8 = _mm_cvtepi8_epi16(cmp);
        v8 = _mm_and_si128(v8, qquarter);

        __m128i o0 = _mm_loadu_si128((const __m128i *)&r[8*i + 0]);
        __m128i o1 = _mm_loadu_si128((const __m128i *)&r[8*i + D4_STEP_LEN]);
        __m128i o2 = _mm_loadu_si128((const __m128i *)&r[8*i + 2*D4_STEP_LEN]);
        __m128i o3 = _mm_loadu_si128((const __m128i *)&r[8*i + 3*D4_STEP_LEN]);
        o0 = _mm_add_epi16(o0, v8);
        o1 = _mm_add_epi16(o1, v8);
        o2 = _mm_add_epi16(o2, v8);
        o3 = _mm_add_epi16(o3, v8);
        _mm_storeu_si128((__m128i *)&r[8*i + 0], o0);
        _mm_storeu_si128((__m128i *)&r[8*i + D4_STEP_LEN], o1);
        _mm_storeu_si128((__m128i *)&r[8*i + 2*D4_STEP_LEN], o2);
        _mm_storeu_si128((__m128i *)&r[8*i + 3*D4_STEP_LEN], o3);
    }
}

/* D4 cancellation helper used by poly_tomsg: same shape as above but subtract. */
static void tomsg_d4_sub_avx(int16_t *r, const uint8_t *src, unsigned int n_bytes)
{
    const __m128i bit_mask = _mm_set_epi8(
        0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, (char)0x80,
        0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, (char)0x80
    );
    const __m128i qquarter = _mm_set1_epi16(WEAVER_Q / 4);
    unsigned int i;

    for(i = 0; i < n_bytes; i++) {
        __m128i b8 = _mm_set1_epi8((char)src[i]);
        __m128i anded = _mm_and_si128(b8, bit_mask);
        __m128i cmp = _mm_cmpeq_epi8(anded, bit_mask);
        __m128i v8 = _mm_cvtepi8_epi16(cmp);
        v8 = _mm_and_si128(v8, qquarter);

        __m128i o0 = _mm_loadu_si128((const __m128i *)&r[8*i + 0]);
        __m128i o1 = _mm_loadu_si128((const __m128i *)&r[8*i + D4_STEP_LEN]);
        __m128i o2 = _mm_loadu_si128((const __m128i *)&r[8*i + 2*D4_STEP_LEN]);
        __m128i o3 = _mm_loadu_si128((const __m128i *)&r[8*i + 3*D4_STEP_LEN]);
        o0 = _mm_sub_epi16(o0, v8);
        o1 = _mm_sub_epi16(o1, v8);
        o2 = _mm_sub_epi16(o2, v8);
        o3 = _mm_sub_epi16(o3, v8);
        _mm_storeu_si128((__m128i *)&r[8*i + 0], o0);
        _mm_storeu_si128((__m128i *)&r[8*i + D4_STEP_LEN], o1);
        _mm_storeu_si128((__m128i *)&r[8*i + 2*D4_STEP_LEN], o2);
        _mm_storeu_si128((__m128i *)&r[8*i + 3*D4_STEP_LEN], o3);
    }
}
#endif

// Algorithm 3: MsgEncode
/*************************************************
* Name:        poly_frommsg
* Description: Convert message to polynomial using WEAVER multi-level coding
**************************************************/
void poly_frommsg(poly *r, const uint8_t msg[WEAVER_INDCPA_MSGBYTES])
{
  unsigned int i, j;
  int16_t mask;
  uint8_t mu_tilde[WEAVER_N / 8] = { 0 };
  uint8_t mu_ddot_buf[LOW_CODEWORD_BYTES] = { 0 };

  for (i = 0; i < WEAVER_N; i++) {
      r->coeffs[i] = 0;
  }

  // ==========================================================
  // Step 1: Encode to Higher bits (MSB-first)
  // ==========================================================
#if WEAVER_MODE == 3 
  /* mu_tilde = msg[0] || msg[1] || ... ||(msg[27] = 1111xxxx) */
  memcpy(mu_tilde, msg, 28);
  mu_tilde[27] &= 0xF0; // leave 4 bits empty (0)
  encode_bch_high_nibbles(mu_tilde, ELL_BAR_NIBBLES, mu_tilde + ELL_BAR_BYTES); // just fit in 32 Bytes

#elif WEAVER_MODE == 5
  memcpy(mu_tilde, msg, ELL_BAR_BYTES);
  encode_bch_high(msg, ELL_BAR_BYTES, mu_tilde + ELL_BAR_BYTES);

#endif

#if defined(__AVX2__) && defined(WEAVER_EXPERIMENTAL_MSGENC_AVX)
  (void)j; (void)mask;
  frommsg_high_bits_avx(r->coeffs, mu_tilde);
#else
  for(i = 0; i < WEAVER_N/8; i++) {
    for(j = 0; j < 8; j++) {
      mask = -(int16_t)((mu_tilde[i] >> (7 - j)) & 1); // MSB-first
      r->coeffs[8*i+j] = mask & WEAVER_HALFQ;
    }
  }
#endif

  // ==========================================================
  // Step 2: Encode to Lower bits
  // ==========================================================
#if WEAVER_MODE == 3 
  /* mu_ddot_buf = msg[28:31] || (msg[27] = xxxx1111 << 4) */
  memcpy(mu_ddot_buf, msg + 28, 4);
  mu_ddot_buf[4] = (msg[27] << 4);
  encode_bch_low_nibbles(mu_ddot_buf, ELL_DDOT_NIBBLES, mu_ddot_buf + ELL_DDOT_BYTES);

#elif WEAVER_MODE == 5
  memcpy(mu_ddot_buf, msg + ELL_BAR_BYTES, ELL_DDOT_BYTES);
  encode_bch_low(mu_ddot_buf, ELL_DDOT_BYTES, mu_ddot_buf + ELL_DDOT_BYTES);

#endif
  // D4 encoding
#if defined(__AVX2__) && defined(WEAVER_EXPERIMENTAL_MSGENC_AVX)
  (void)j; (void)mask;
  frommsg_d4_add_avx(r->coeffs, mu_ddot_buf, LOW_CODEWORD_BYTES);
#else
  for (i = 0; i < LOW_CODEWORD_BYTES; i++) {
      for (j = 0; j < 8; j++) {
          mask = -(int16_t)((mu_ddot_buf[i] >> (7 - j)) & 1);
          r->coeffs[8 * i + j + 0] = r->coeffs[8 * i + j + 0] + (mask & (WEAVER_Q / 4));
          r->coeffs[8 * i + j + D4_STEP_LEN] = r->coeffs[8 * i + j + D4_STEP_LEN] + (mask & (WEAVER_Q / 4));
          r->coeffs[8 * i + j + 2 * D4_STEP_LEN] = r->coeffs[8 * i + j + 2 * D4_STEP_LEN] + (mask & (WEAVER_Q / 4));
          r->coeffs[8 * i + j + 3 * D4_STEP_LEN] = r->coeffs[8 * i + j + 3 * D4_STEP_LEN] + (mask & (WEAVER_Q / 4));
      }
  }
#endif
}

// Algorithm 5: MsgDecode
void poly_tomsg(uint8_t msg[WEAVER_INDCPA_MSGBYTES], const poly *a)
{
  unsigned int i, j;
  int16_t w_bar[WEAVER_N];
  uint8_t mu_tilde[WEAVER_N / 8] = { 0 };
  uint8_t mu_ddot_noisy[LOW_CODEWORD_BYTES] = { 0 };
  uint8_t mu_ddot_clean[LOW_CODEWORD_BYTES] = { 0 };

  memset(msg, 0, WEAVER_INDCPA_MSGBYTES);

  for(i = 0; i < WEAVER_N; i++) { 
      int16_t t = a->coeffs[i];
      w_bar[i] = t + ( (t >> 15) & WEAVER_Q );
  }

  // ==========================================================
  // Phase 1: Decode Lower bits
  // ==========================================================
  for(i = 0; i < 8 * LOW_CODEWORD_BYTES; i++) {
    uint16_t ee = 0;
    ee =  flipabs_ex(w_bar[i + 0  ]);
    ee += flipabs_ex(w_bar[i + D4_STEP_LEN]);
    ee += flipabs_ex(w_bar[i + 2 * D4_STEP_LEN]);
    ee += flipabs_ex(w_bar[i + 3 * D4_STEP_LEN]);
    ee = (ee - WEAVER_HALFQ);
    ee >>= 15;
    mu_ddot_noisy[i>>3] |= ee << (7 - (i&7)); /* Here: we need bits to be packed continuously w/o interleaving 0s */
  }

  // ==========================================================
  // Phase 2: Cancel Interference from Lower bits
  // ==========================================================
#if WEAVER_MODE == 3 
  decode_bch_low_nibbles(mu_ddot_noisy, 9, mu_ddot_noisy + 5);
  memcpy(mu_ddot_clean, mu_ddot_noisy, ELL_DDOT_BYTES);
  encode_bch_low_nibbles(mu_ddot_clean, ELL_DDOT_NIBBLES, mu_ddot_clean + ELL_DDOT_BYTES);

#elif WEAVER_MODE == 5
  decode_bch_low(mu_ddot_noisy, ELL_DDOT_BYTES, mu_ddot_noisy + ELL_DDOT_BYTES);
  memcpy(mu_ddot_clean, mu_ddot_noisy, ELL_DDOT_BYTES);
  encode_bch_low(mu_ddot_clean, ELL_DDOT_BYTES, mu_ddot_clean + ELL_DDOT_BYTES);

#endif

#if defined(__AVX2__) && defined(WEAVER_EXPERIMENTAL_MSGENC_AVX)
  tomsg_d4_sub_avx(w_bar, mu_ddot_clean, LOW_CODEWORD_BYTES);
#else
  for(i = 0; i < LOW_CODEWORD_BYTES; i++) { // 不保证为正
    for(j = 0; j < 8; j++) {
        int16_t mask = -((mu_ddot_clean[i] >> (7 - j)) & 1); 
        w_bar[8*i + j + 0  ] -= (mask & (WEAVER_Q/4));
        w_bar[8*i + j + D4_STEP_LEN] -= (mask & (WEAVER_Q/4));
        w_bar[8*i + j + 2 * D4_STEP_LEN] -= (mask & (WEAVER_Q/4));
        w_bar[8*i + j + 3 * D4_STEP_LEN] -= (mask & (WEAVER_Q/4));
    }
  }
#endif

  // ==========================================================
  // Phase 3: Decode Higher bits
  // ==========================================================
  for(i = 0; i < WEAVER_N/8; i++) {
    for(j=0;j<8;j++) {
      int16_t t = w_bar[8*i+j];
      t += ((int16_t)t >> 15) & WEAVER_Q;  // map to positive
      t = ((((uint32_t)t << 1) + WEAVER_Q/2) / WEAVER_Q) & 1;
      mu_tilde[i] |= t << (7 - j); 
      //mu_tilde[i] |= t << j;
    }
  }

#if WEAVER_MODE == 3 
  decode_bch_high_nibbles(mu_tilde, ELL_BAR_NIBBLES, mu_tilde + ELL_BAR_BYTES);
  // all high bits and 4 of low bits
  memcpy(msg, mu_tilde, 27);
  msg[27] = (mu_tilde[27] & 0xF0) | ((mu_ddot_clean[4] >> 4) & 0xF);
  // low bits
  memcpy(msg + ELL_BAR_BYTES, mu_ddot_clean, ELL_DDOT_BYTES - 1); /* mu_ddot_clean[0:3] */

#elif WEAVER_MODE == 5
  decode_bch_high(mu_tilde, ELL_BAR_BYTES, mu_tilde + ELL_BAR_BYTES);
  memcpy(msg, mu_tilde, ELL_BAR_BYTES);
  memcpy(msg + ELL_BAR_BYTES, mu_ddot_clean, ELL_DDOT_BYTES);

#endif
}
#endif
#endif