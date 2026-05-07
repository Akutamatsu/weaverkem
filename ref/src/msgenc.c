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

// Algorithm 3: MsgEncode
/*************************************************
* Name:        poly_frommsg
* Description: Convert message to polynomial using WEAVER multi-level coding
**************************************************/
void poly_frommsg(poly *r, const uint8_t msg[KYBER_INDCPA_MSGBYTES])
{
  unsigned int i, j;
  int16_t mask;
  uint8_t mu_tilde[KYBER_N/8] = {0};
#if WEAVER_MODE == 3 || WEAVER_MODE == 5
  uint8_t mu_ddot_buf[LOW_CODEWORD_BYTES] = {0};
  const uint8_t *msg_low = msg + ELL_BAR_BYTES; // msg lowbits part
#endif

  for(i = 0; i < KYBER_N; i++) {
    r->coeffs[i] = 0;
  }

  // ==========================================================
  // Step 1: Encode to Higher bits
  // ==========================================================
  // 拼装高位码字：拷贝明文 -> 追加校验位
  memcpy(mu_tilde, msg, ELL_BAR_BYTES);
  encode_bch_high(msg, ELL_BAR_BYTES, mu_tilde + ELL_BAR_BYTES);

  // 调制高位
  for(i = 0; i < KYBER_N/8; i++) {
    for(j=0;j<8;j++) {
      mask = -(int16_t)((mu_tilde[i] >> j)&1);
      r->coeffs[8*i+j] = mask & KYBER_HALFQ;
    }
  }

#if WEAVER_MODE == 3 
  // ==========================================================
  // Step 2: Encode to Lower bits
  // ==========================================================
  
  // 拼装次高位码字：拷贝明文 -> 追加校验位
  memcpy(mu_ddot_buf, msg_low, ELL_DDOT_BYTES);
  encode_bch_low(msg_low, ELL_DDOT_BYTES, mu_ddot_buf + ELL_DDOT_BYTES);

  // 调制次高位 (带 4 倍重复码)
  for(i = 0; i < LOW_CODEWORD_BYTES; i++) {
    for(j=0;j<8;j++) {
      mask = -(int16_t)((mu_ddot_buf[i] >> j)&1);
      r->coeffs[8*i + j + + 0  ] = r->coeffs[8*i + j + + 0  ] + (mask & (KYBER_Q/4));
      r->coeffs[8*i + j + + 64 ] = r->coeffs[8*i + j + + 64 ] + (mask & (KYBER_Q/4));
      r->coeffs[8*i + j + + 128] = r->coeffs[8*i + j + + 128] + (mask & (KYBER_Q/4));
      r->coeffs[8*i + j + + 192] = r->coeffs[8*i + j + + 192] + (mask & (KYBER_Q/4));
    }
  }
  #elif WEAVER_MODE == 5
  memcpy(mu_ddot_buf, msg_low, ELL_DDOT_BYTES);
  encode_bch_low(msg_low, ELL_DDOT_BYTES, mu_ddot_buf + ELL_DDOT_BYTES);
  // 【Mode 5】：N = 512，4倍重复，步长 128
  for(i = 0; i < LOW_CODEWORD_BYTES; i++) {
    for(j=0;j<8;j++) {
      mask = -(int16_t)((mu_ddot_buf[i] >> j)&1);
      // 完美平铺在 512 维的系数环中
      r->coeffs[8*i + j + 0  ] = r->coeffs[8*i + j + 0  ] + (mask & (KYBER_Q/4));
      r->coeffs[8*i + j + 128] = r->coeffs[8*i + j + 128] + (mask & (KYBER_Q/4));
      r->coeffs[8*i + j + 256] = r->coeffs[8*i + j + 256] + (mask & (KYBER_Q/4));
      r->coeffs[8*i + j + 384] = r->coeffs[8*i + j + 384] + (mask & (KYBER_Q/4));
    }
  }
#endif
}

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
  int16_t r,m;
  r = barrett_reduce_ex(x);

  r = r - KYBER_Q/4;
  m = r >> 15;
  return (r + m)^m; // turn to positive
}

// Algorithm 5: MsgDecode
void poly_tomsg(uint8_t msg[KYBER_INDCPA_MSGBYTES], const poly *a)
{
  unsigned int i, j;
  int16_t w_bar[KYBER_N];
  uint8_t mu_tilde[KYBER_N/8] = {0};
#if WEAVER_MODE == 3 || WEAVER_MODE == 5
  uint8_t mu_ddot_noisy[LOW_CODEWORD_BYTES] = {0};
  uint8_t *msg_low = msg + ELL_BAR_BYTES;
#endif

  memset(msg, 0, KYBER_INDCPA_MSGBYTES);

  for(i = 0; i < KYBER_N; i++) { // COPY coeffs into w_bar
    int16_t t = a->coeffs[i];
    // map to positive standard representatives: [0, q-1]
    w_bar[i] = t + ( (t >> 15) & KYBER_Q );
  }

#if WEAVER_MODE == 3 
  // ==========================================================
  // Phase 1: Decode Lower bits
  // ==========================================================
  for(i = 0; i < 8 * LOW_CODEWORD_BYTES; i++) {
    uint16_t ee = 0;
    ee =  flipabs_ex(w_bar[i + 0  ]);
    ee += flipabs_ex(w_bar[i + 64 ]);
    ee += flipabs_ex(w_bar[i + 128]);
    ee += flipabs_ex(w_bar[i + 192]);
    // 判决得到带噪的码字位
    ee = (ee - KYBER_HALFQ);
    ee >>= 15;
    mu_ddot_noisy[i>>3] |= ee<<(i&7);
  }

  // 关键步骤：纠错并提取出完美的纯数据
  decode_bch_low(mu_ddot_noisy, ELL_DDOT_BYTES, mu_ddot_noisy + ELL_DDOT_BYTES);
  memcpy(msg_low, mu_ddot_noisy, ELL_DDOT_BYTES); // 将纯数据保存到输出区
  
  // ==========================================================
  // Phase 2: Cancel Interference from Lower bits
  // ==========================================================
  // 利用纯净的数据，重新编码出没有任何噪声的完美码字！
  uint8_t mu_ddot_clean[8] = {0};
  memcpy(mu_ddot_clean, msg_low, ELL_DDOT_BYTES);
  encode_bch_low(msg_low, ELL_DDOT_BYTES, mu_ddot_clean + ELL_DDOT_BYTES);

  // 用完美的码字，把低位造成的干扰从多项式系数中彻底减掉
  for(i = 0; i < LOW_CODEWORD_BYTES; i++) { // 不保证为正
    for(j = 0; j < 8; j++) {
        int16_t mask = -((mu_ddot_clean[i] >> j)&1);
        w_bar[8*i + j + 0  ] = w_bar[8*i + j + 0  ] - (mask & (KYBER_Q/4));
        w_bar[8*i + j + 64 ] = w_bar[8*i + j + 64 ] - (mask & (KYBER_Q/4));
        w_bar[8*i + j + 128] = w_bar[8*i + j + 128] - (mask & (KYBER_Q/4));
        w_bar[8*i + j + 192] = w_bar[8*i + j + 192] - (mask & (KYBER_Q/4));
    }
  }
#elif WEAVER_MODE == 5
  // ==========================================================
  // Phase 1: Decode Lower bits
  // ==========================================================
  for(i = 0; i < 8 * LOW_CODEWORD_BYTES; i++) {
    uint16_t ee = 0;
    ee =  flipabs_ex(w_bar[i + 0  ]);
    ee += flipabs_ex(w_bar[i + 128]);
    ee += flipabs_ex(w_bar[i + 256]);
    ee += flipabs_ex(w_bar[i + 384]);
    // 判决得到带噪的码字位
    ee = (ee - KYBER_HALFQ);
    ee >>= 15;
    mu_ddot_noisy[i>>3] |= ee<<(i&7);
  }

  // 关键步骤：纠错并提取出完美的纯数据
  decode_bch_low(mu_ddot_noisy, ELL_DDOT_BYTES, mu_ddot_noisy + ELL_DDOT_BYTES);
  memcpy(msg_low, mu_ddot_noisy, ELL_DDOT_BYTES); // 将纯数据保存到输出区
  
  // ==========================================================
  // Phase 2: Cancel Interference from Lower bits
  // ==========================================================
  // 利用纯净的数据，重新编码出没有任何噪声的完美码字！
  uint8_t mu_ddot_clean[16] = {0};
  memcpy(mu_ddot_clean, msg_low, ELL_DDOT_BYTES);
  encode_bch_low(msg_low, ELL_DDOT_BYTES, mu_ddot_clean + ELL_DDOT_BYTES);

  // 用完美的码字，把低位造成的干扰从多项式系数中彻底减掉
  for(i = 0; i < LOW_CODEWORD_BYTES; i++) { // 不保证为正
    for(j = 0; j < 8; j++) {
        int16_t mask = -((mu_ddot_clean[i] >> j)&1);
        w_bar[8*i + j + 0  ] = w_bar[8*i + j + 0  ] - (mask & (KYBER_Q/4));
        w_bar[8*i + j + 128] = w_bar[8*i + j + 128] - (mask & (KYBER_Q/4));
        w_bar[8*i + j + 256] = w_bar[8*i + j + 256] - (mask & (KYBER_Q/4));
        w_bar[8*i + j + 384] = w_bar[8*i + j + 384] - (mask & (KYBER_Q/4));
    }
  }

#endif

  // ==========================================================
  // Phase 3: Decode Higher bits
  // ==========================================================
  for(i = 0; i < KYBER_N/8; i++) {
    for(j=0;j<8;j++) {
      int16_t t = w_bar[8*i+j];
      t += ((int16_t)t >> 15) & KYBER_Q; // map to positive
      t = ((((uint32_t)t << 1) + KYBER_Q/2) / KYBER_Q) & 1;
      mu_tilde[i] |= t << j;
    }
  }

  // BCH 纠错高位，并存入输出区
  decode_bch_high(mu_tilde, ELL_BAR_BYTES, mu_tilde + ELL_BAR_BYTES);
  memcpy(msg, mu_tilde, ELL_BAR_BYTES);
}
#endif