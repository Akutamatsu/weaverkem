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

#if defined(__AVX2__) && defined(WEAVER_EXPERIMENTAL_MSGENC_AVX)
static void frommsg_high_bits_avx(int16_t r[KYBER_N], const uint8_t *mu_tilde)
{
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
    const __m256i halfq16 = _mm256_set1_epi16(KYBER_HALFQ);
    unsigned int i;

    for(i = 0; i < KYBER_N / 32; i++) {
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

static void frommsg_d4_add_avx(int16_t *r, const uint8_t *src, unsigned int n_bytes)
{
    const __m128i bit_mask = _mm_set_epi8(
        0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, (char)0x80,
        0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, (char)0x80
    );
    const __m128i qquarter = _mm_set1_epi16(KYBER_Q / 4);
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

static void tomsg_d4_sub_avx(int16_t *r, const uint8_t *src, unsigned int n_bytes)
{
    const __m128i bit_mask = _mm_set_epi8(
        0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, (char)0x80,
        0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, (char)0x80
    );
    const __m128i qquarter = _mm_set1_epi16(KYBER_Q / 4);
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

static uint16_t flipabs_ex(int16_t x)
{
    int16_t r, m;
    r = barrett_reduce_ex(x);

    r = r - KYBER_Q / 4;
    m = r >> 15;
    return (r + m) ^ m;
}

void poly_frommsg(poly *r, const uint8_t msg[KYBER_INDCPA_MSGBYTES])
{
  unsigned int i, j;
  int16_t mask;
  uint8_t mu_tilde[KYBER_N / 8] = { 0 };
  uint8_t mu_ddot_buf[LOW_CODEWORD_BYTES] = { 0 };

  for (i = 0; i < KYBER_N; i++) {
      r->coeffs[i] = 0;
  }

  memcpy(mu_tilde, msg, ELL_BAR_BYTES);
  mu_tilde[ELL_BAR_BYTES - 1] &= 0xF0;
  encode_bch_high_nibbles(mu_tilde, ELL_BAR_NIBBLES, mu_tilde + ELL_BAR_BYTES);

#if defined(__AVX2__) && defined(WEAVER_EXPERIMENTAL_MSGENC_AVX)
  (void)j; (void)mask;
  frommsg_high_bits_avx(r->coeffs, mu_tilde);
#else
  for(i = 0; i < KYBER_N/8; i++) {
    for(j = 0; j < 8; j++) {
      mask = -(int16_t)((mu_tilde[i] >> (7 - j)) & 1);
      r->coeffs[8*i+j] = mask & KYBER_HALFQ;
    }
  }
#endif

  memcpy(mu_ddot_buf, msg + ELL_BAR_BYTES, KYBER_INDCPA_MSGBYTES - ELL_BAR_BYTES);
  mu_ddot_buf[KYBER_INDCPA_MSGBYTES - ELL_BAR_BYTES] = (msg[ELL_BAR_BYTES - 1] << 4);
  encode_bch_low_nibbles(mu_ddot_buf, ELL_DDOT_NIBBLES, mu_ddot_buf + ELL_DDOT_BYTES);

#if defined(__AVX2__) && defined(WEAVER_EXPERIMENTAL_MSGENC_AVX)
  (void)j; (void)mask;
  frommsg_d4_add_avx(r->coeffs, mu_ddot_buf, LOW_CODEWORD_BYTES);
#else
  for (i = 0; i < LOW_CODEWORD_BYTES; i++) {
      for (j = 0; j < 8; j++) {
          mask = -(int16_t)((mu_ddot_buf[i] >> (7 - j)) & 1);
          r->coeffs[8 * i + j + 0] = r->coeffs[8 * i + j + 0] + (mask & (KYBER_Q / 4));
          r->coeffs[8 * i + j + D4_STEP_LEN] = r->coeffs[8 * i + j + D4_STEP_LEN] + (mask & (KYBER_Q / 4));
          r->coeffs[8 * i + j + 2 * D4_STEP_LEN] = r->coeffs[8 * i + j + 2 * D4_STEP_LEN] + (mask & (KYBER_Q / 4));
          r->coeffs[8 * i + j + 3 * D4_STEP_LEN] = r->coeffs[8 * i + j + 3 * D4_STEP_LEN] + (mask & (KYBER_Q / 4));
      }
  }
#endif
}

void poly_tomsg(uint8_t msg[KYBER_INDCPA_MSGBYTES], const poly *a)
{
  unsigned int i, j;
  int16_t w_bar[KYBER_N];
  uint8_t mu_tilde[KYBER_N / 8] = { 0 };
  uint8_t mu_ddot_noisy[LOW_CODEWORD_BYTES] = { 0 };
  uint8_t mu_ddot_clean[LOW_CODEWORD_BYTES] = { 0 };

  memset(msg, 0, KYBER_INDCPA_MSGBYTES);

  for(i = 0; i < KYBER_N; i++) {
      int16_t t = a->coeffs[i];
      w_bar[i] = t + ( (t >> 15) & KYBER_Q );
  }

  for(i = 0; i < 8 * LOW_CODEWORD_BYTES; i++) {
    uint16_t ee = 0;
    ee =  flipabs_ex(w_bar[i + 0  ]);
    ee += flipabs_ex(w_bar[i + D4_STEP_LEN]);
    ee += flipabs_ex(w_bar[i + 2 * D4_STEP_LEN]);
    ee += flipabs_ex(w_bar[i + 3 * D4_STEP_LEN]);
    ee = (ee - KYBER_HALFQ);
    ee >>= 15;
    mu_ddot_noisy[i>>3] |= ee << (7 - (i&7));
  }

  decode_bch_low_nibbles(mu_ddot_noisy, ELL_DDOT_NIBBLES, mu_ddot_noisy + ELL_DDOT_BYTES);
  memcpy(mu_ddot_clean, mu_ddot_noisy, ELL_DDOT_BYTES);
  encode_bch_low_nibbles(mu_ddot_clean, ELL_DDOT_NIBBLES, mu_ddot_clean + ELL_DDOT_BYTES);

#if defined(__AVX2__) && defined(WEAVER_EXPERIMENTAL_MSGENC_AVX)
  tomsg_d4_sub_avx(w_bar, mu_ddot_clean, LOW_CODEWORD_BYTES);
#else
  for(i = 0; i < LOW_CODEWORD_BYTES; i++) {
    for(j = 0; j < 8; j++) {
        int16_t mask = -((mu_ddot_clean[i] >> (7 - j)) & 1);
        w_bar[8*i + j + 0  ] -= (mask & (KYBER_Q/4));
        w_bar[8*i + j + D4_STEP_LEN] -= (mask & (KYBER_Q/4));
        w_bar[8*i + j + 2 * D4_STEP_LEN] -= (mask & (KYBER_Q/4));
        w_bar[8*i + j + 3 * D4_STEP_LEN] -= (mask & (KYBER_Q/4));
    }
  }
#endif

  for(i = 0; i < KYBER_N/8; i++) {
    for(j=0;j<8;j++) {
      int16_t t = w_bar[8*i+j];
      t += ((int16_t)t >> 15) & KYBER_Q;
      t = ((((uint32_t)t << 1) + KYBER_Q/2) / KYBER_Q) & 1;
      mu_tilde[i] |= t << (7 - j);
    }
  }

  decode_bch_high_nibbles(mu_tilde, ELL_BAR_NIBBLES, mu_tilde + ELL_BAR_BYTES);

  memcpy(msg, mu_tilde, ELL_BAR_BYTES - 1);
  msg[ELL_BAR_BYTES - 1] = (mu_tilde[ELL_BAR_BYTES - 1] & 0xF0) | ((mu_ddot_clean[KYBER_INDCPA_MSGBYTES - ELL_BAR_BYTES] >> 4) & 0xF);
  memcpy(msg + ELL_BAR_BYTES, mu_ddot_clean, KYBER_INDCPA_MSGBYTES - ELL_BAR_BYTES);
}
