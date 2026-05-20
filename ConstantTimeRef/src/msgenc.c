#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "params.h"
#include "poly.h"
#include "msgenc.h"
#include "reduce.h"
#include "bch.h"

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
void poly_frommsg(poly *r, const uint8_t msg[KYBER_INDCPA_MSGBYTES])
{
  unsigned int i,j;
  int16_t mask;

#if (KYBER_INDCPA_MSGBYTES > KYBER_N/8)
#error "KYBER_INDCPA_MSGBYTES must be less than KYBER_N/8 bytes!"
#endif

  for(i=0;i<KYBER_N/8;i++) {
    for(j=0;j<8;j++) {
      mask = -(int16_t)((msg[i] >> j)&1);
      r->coeffs[8*i+j] = mask & ((KYBER_Q+1)/2);
    }
  }
}
/* kyber原代码: 不使用纠错, 直接编一层高位 */
void poly_tomsg(uint8_t msg[KYBER_INDCPA_MSGBYTES], const poly *a)
{
  unsigned int i,j;
  uint16_t t;

  //poly_csubq(a); /* modified barret_reduce */

  for(i=0;i<KYBER_N/8;i++) {
    msg[i] = 0;
    for(j=0;j<8;j++) {
      t  = a->coeffs[8*i+j];
      // map to positive standard representatives
      t += ((int16_t)t >> 15) & KYBER_Q;
      t  = (((t << 1) + KYBER_Q/2)/KYBER_Q) & 1;
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
void poly_frommsg(poly *r, const uint8_t msg[KYBER_INDCPA_MSGBYTES])
{
    unsigned int i, j;
    int16_t mask;
    uint8_t mu_tilde[KYBER_N / 8] = { 0 };

    for (i = 0; i < KYBER_N; i++) {
        r->coeffs[i] = 0;
    }

    // ==========================================================
    // Step 1: Encode to Higher bits
    // ==========================================================
    memcpy(mu_tilde, msg, ELL_BAR_BYTES);
    encode_bch_high(msg, ELL_BAR_BYTES, mu_tilde + ELL_BAR_BYTES);

    for (i = 0; i < KYBER_N / 8; i++) {
        for (j = 0; j < 8; j++) {
            mask = -(int16_t)((mu_tilde[i] >> (7 - j)) & 1);
            r->coeffs[8 * i + j] = mask & KYBER_HALFQ;
        }
    }
}
// Algorithm 5: MsgDecode
void poly_tomsg(uint8_t msg[KYBER_INDCPA_MSGBYTES], const poly *a)
{
    unsigned int i, j;
    int16_t w_bar[KYBER_N];
    uint8_t mu_tilde[KYBER_N / 8] = { 0 };
    memset(msg, 0, KYBER_INDCPA_MSGBYTES);

    for (i = 0; i < KYBER_N; i++) { // COPY coeffs into w_bar
        int16_t t = a->coeffs[i];
        // map to positive standard representatives: [0, q-1]
        w_bar[i] = t + ((t >> 15) & KYBER_Q);
    }
    // ==========================================================
    // Phase 3: Decode Higher bits
    // ==========================================================
    for (i = 0; i < KYBER_N / 8; i++) {
        for (j = 0; j < 8; j++) {
            int16_t t = w_bar[8 * i + j];
            t += ((int16_t)t >> 15) & KYBER_Q; // map to positive
            t = ((((uint32_t)t << 1) + KYBER_Q / 2) / KYBER_Q) & 1;
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

    r = r - KYBER_Q / 4;
    m = r >> 15;
    return (r + m) ^ m; // turn to positive
}

// Algorithm 3: MsgEncode
/*************************************************
* Name:        poly_frommsg
* Description: Convert message to polynomial using WEAVER multi-level coding
**************************************************/
void poly_frommsg(poly *r, const uint8_t msg[KYBER_INDCPA_MSGBYTES])
{
  unsigned int i, j;
  int16_t mask;
  uint8_t mu_tilde[KYBER_N / 8] = { 0 };
  uint8_t mu_ddot_buf[LOW_CODEWORD_BYTES] = { 0 };

  for (i = 0; i < KYBER_N; i++) {
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

  for(i = 0; i < KYBER_N/8; i++) {
    for(j = 0; j < 8; j++) {
      mask = -(int16_t)((mu_tilde[i] >> (7 - j)) & 1); // MSB-first
      //mask = -(int16_t)((mu_tilde[i] >> j) & 1);
      r->coeffs[8*i+j] = mask & KYBER_HALFQ;
    }
  }

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
  for (i = 0; i < LOW_CODEWORD_BYTES; i++) {
      for (j = 0; j < 8; j++) {
          mask = -(int16_t)((mu_ddot_buf[i] >> (7 - j)) & 1);
          r->coeffs[8 * i + j + 0] = r->coeffs[8 * i + j + 0] + (mask & (KYBER_Q / 4));
          r->coeffs[8 * i + j + D4_STEP_LEN] = r->coeffs[8 * i + j + D4_STEP_LEN] + (mask & (KYBER_Q / 4));
          r->coeffs[8 * i + j + 2 * D4_STEP_LEN] = r->coeffs[8 * i + j + 2 * D4_STEP_LEN] + (mask & (KYBER_Q / 4));
          r->coeffs[8 * i + j + 3 * D4_STEP_LEN] = r->coeffs[8 * i + j + 3 * D4_STEP_LEN] + (mask & (KYBER_Q / 4));
      }
  }
}

// Algorithm 5: MsgDecode
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

  // ==========================================================
  // Phase 1: Decode Lower bits
  // ==========================================================
  for(i = 0; i < 8 * LOW_CODEWORD_BYTES; i++) {
    uint16_t ee = 0;
    ee =  flipabs_ex(w_bar[i + 0  ]);
    ee += flipabs_ex(w_bar[i + D4_STEP_LEN]);
    ee += flipabs_ex(w_bar[i + 2 * D4_STEP_LEN]);
    ee += flipabs_ex(w_bar[i + 3 * D4_STEP_LEN]);
    ee = (ee - KYBER_HALFQ);
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

  for(i = 0; i < LOW_CODEWORD_BYTES; i++) { // 不保证为正
    for(j = 0; j < 8; j++) {
        int16_t mask = -((mu_ddot_clean[i] >> (7 - j)) & 1); 
        w_bar[8*i + j + 0  ] -= (mask & (KYBER_Q/4));
        w_bar[8*i + j + D4_STEP_LEN] -= (mask & (KYBER_Q/4));
        w_bar[8*i + j + 2 * D4_STEP_LEN] -= (mask & (KYBER_Q/4));
        w_bar[8*i + j + 3 * D4_STEP_LEN] -= (mask & (KYBER_Q/4));
    }
  }

  // ==========================================================
  // Phase 3: Decode Higher bits
  // ==========================================================
  for(i = 0; i < KYBER_N/8; i++) {
    for(j=0;j<8;j++) {
      int16_t t = w_bar[8*i+j];
      t += ((int16_t)t >> 15) & KYBER_Q;  // map to positive
      t = ((((uint32_t)t << 1) + KYBER_Q/2) / KYBER_Q) & 1;
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