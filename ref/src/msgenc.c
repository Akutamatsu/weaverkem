#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "params.h"
#include "poly.h"
#include "msgenc.h"
#include "reduce.h"
#include "bch.h"


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
* (Unified Nibbles Architecture for all modes, N=256)
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
  /* mu_tilde = msg[0] || msg[1] || ... ||(msg[27] = 1111xxxx) */
  memcpy(mu_tilde, msg, ELL_BAR_BYTES);
  mu_tilde[ELL_BAR_BYTES - 1] &= 0xF0; // leave 4 bits empty (0) for padding
  encode_bch_high_nibbles(mu_tilde, ELL_BAR_NIBBLES, mu_tilde + ELL_BAR_BYTES); 

  for(i = 0; i < KYBER_N/8; i++) {
    for(j = 0; j < 8; j++) {
      mask = -(int16_t)((mu_tilde[i] >> (7 - j)) & 1); // MSB-first
      r->coeffs[8*i+j] = mask & KYBER_HALFQ;
    }
  }

  // ==========================================================
  // Step 2: Encode to Lower bits
  // ==========================================================
  /* mu_ddot_buf = msg[28:31] || (msg[27] = xxxx1111 << 4) */
  memcpy(mu_ddot_buf, msg + ELL_BAR_BYTES, KYBER_INDCPA_MSGBYTES - ELL_BAR_BYTES);
  mu_ddot_buf[KYBER_INDCPA_MSGBYTES - ELL_BAR_BYTES] = (msg[ELL_BAR_BYTES - 1] << 4);
  encode_bch_low_nibbles(mu_ddot_buf, ELL_DDOT_NIBBLES, mu_ddot_buf + ELL_DDOT_BYTES);

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
  decode_bch_low_nibbles(mu_ddot_noisy, ELL_DDOT_NIBBLES, mu_ddot_noisy + ELL_DDOT_BYTES);
  memcpy(mu_ddot_clean, mu_ddot_noisy, ELL_DDOT_BYTES);
  encode_bch_low_nibbles(mu_ddot_clean, ELL_DDOT_NIBBLES, mu_ddot_clean + ELL_DDOT_BYTES);

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
    }
  }

  decode_bch_high_nibbles(mu_tilde, ELL_BAR_NIBBLES, mu_tilde + ELL_BAR_BYTES);
  
  // Merge all high bits and 4 of low bits back into the 32-byte message
  memcpy(msg, mu_tilde, ELL_BAR_BYTES - 1); 
  msg[ELL_BAR_BYTES - 1] = (mu_tilde[ELL_BAR_BYTES - 1] & 0xF0) | ((mu_ddot_clean[KYBER_INDCPA_MSGBYTES - ELL_BAR_BYTES] >> 4) & 0xF);
  
  // Copy remaining low bits
  memcpy(msg + ELL_BAR_BYTES, mu_ddot_clean, KYBER_INDCPA_MSGBYTES - ELL_BAR_BYTES); 
}

