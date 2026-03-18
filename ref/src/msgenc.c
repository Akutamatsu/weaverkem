#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "params.h"
#include "poly.h"
#include "msgenc.h"
#include "bch.h"

// --- 辅助函数：按位提取与写入 ---
static inline uint8_t get_bit(const uint8_t *in, int bit_pos) {
    return (in[bit_pos / 8] >> (bit_pos % 8)) & 1;
}
static inline void set_bit(uint8_t *out, int bit_pos, uint8_t val) {
    if(val) out[bit_pos / 8] |=  (1 << (bit_pos % 8));
    else    out[bit_pos / 8] &= ~(1 << (bit_pos % 8));
}

/*************************************************
* Name:        poly_frommsg
*
* Description: Convert 32-byte message to polynomial
*
* Arguments:   - poly *r:            pointer to output polynomial
*              - const uint8_t *msg: pointer to input message
**************************************************/
// kyber原代码
// void poly_frommsg(poly *r, const uint8_t msg[KYBER_INDCPA_MSGBYTES])
// {
//   unsigned int i,j;
//   int16_t mask;

// #if (KYBER_INDCPA_MSGBYTES != KYBER_N/8)
// #error "KYBER_INDCPA_MSGBYTES must be equal to KYBER_N/8 bytes!"
// #endif

//   for(i=0;i<KYBER_N/8;i++) {
//     for(j=0;j<8;j++) {
//       mask = -(int16_t)((msg[i] >> j)&1);
//       r->coeffs[8*i+j] = mask & ((KYBER_Q+1)/2);
//     }
//   }
// }

// 只更改ntt算法的代码
// void poly_frommsg(poly *r, const uint8_t msg[KYBER_INDCPA_MSGBYTES])
// {
//   unsigned int i,j;
//   int16_t mask;

// #if KYBER_N == 128
//   // Level 1: 128个系数装32字节，每个系数承载2个bit
//   for(i=0;i<KYBER_INDCPA_MSGBYTES;i++) {
//     for(j=0;j<4;j++) {
//       int16_t val = (msg[i] >> (2*j)) & 3;
//       r->coeffs[4*i+j] = (val * KYBER_Q + 2) / 4;
//     }
//   }

// #elif KYBER_N == 256 || KYBER_N == 512
//   // 先将所有系数清零 (这是为了应对 N=512 时，后 256 个系数没有被用到的情况)
//   for(i=0; i<KYBER_N; i++) {
//     r->coeffs[i] = 0;
//   }

//   // Level 2 & 3: 前256个系数，每个系数装1个bit
//   for(i=0;i<KYBER_INDCPA_MSGBYTES;i++) {
//     for(j=0;j<8;j++) {
//       mask = -(int16_t)((msg[i] >> j)&1);
//       r->coeffs[8*i+j] = mask & ((KYBER_Q+1)/2);
//     }
//   }
// #endif
// }

// Algorithm 3: MsgEncode
/*************************************************
* Name:        poly_frommsg
* Description: Convert message to polynomial using WEAVER multi-level coding
**************************************************/
void poly_frommsg(poly *r, const uint8_t msg[KYBER_INDCPA_MSGBYTES])
{
  int i;
  const int16_t q = KYBER_Q;           // 3329
  const int16_t half_q = (q + 1) / 2;  // 1665
  const int16_t quarter_q = q / 4;     // 832

#if (WEAVER_MODE == 1)
  const int l_bar = 128;  // 高位承载 bit 数
  const int l_ddot = 0;   // 次高位承载 bit 数
  const int h = 0;        // 次高位码字总长度 (N/4)
#elif (WEAVER_MODE == 3)
  const int l_bar = 224;
  const int l_ddot = 32;  // BCH(63, 32) 明文长度
  const int h = 64;       // 256 / 4 = 64
#elif (WEAVER_MODE == 5)
  const int l_bar = 480;  // 根据你实际参数，也可能是 480
  const int l_ddot = 32;
  const int h = 64;       // 保证跟 BCH63 匹配
#endif

  for(i = 0; i < KYBER_N; i++) {
    r->coeffs[i] = 0;
  }

  // ==========================================================
  // Step 1: 高位 BCH 编码 (Encode to Higher bits)
  // ==========================================================
  uint8_t mu_tilde[(KYBER_N + 7) / 8] = {0}; 
  
  // 拼装高位码字：拷贝明文 -> 追加校验位
  memcpy(mu_tilde, msg, l_bar / 8);
  encode_bch_high(msg, l_bar / 8, mu_tilde + (l_bar / 8)); 

  // 调制高位
  for(i = 0; i < KYBER_N; i++) { 
    uint8_t bit = get_bit(mu_tilde, i);
    r->coeffs[i] = (r->coeffs[i] + half_q * bit) % q;
  }

#if WEAVER_MODE == 3 || WEAVER_MODE == 5
  // ==========================================================
  // Step 2: 次高位 BCH 编码 (Encode to Lower bits)
  // ==========================================================
  uint8_t mu_ddot_buf[8] = {0}; // 最多容纳 64 bits = 8 bytes
  const uint8_t *msg_low = msg + (l_bar / 8); // 定位到 msg 的次高位数据区
  
  // 拼装次高位码字：拷贝明文 -> 追加校验位
  memcpy(mu_ddot_buf, msg_low, l_ddot / 8);
  encode_bch_low(msg_low, l_ddot / 8, mu_ddot_buf + (l_ddot / 8));

  // 调制次高位 (带 4 倍重复码)
  for(i = 0; i < h; i++) {
    uint8_t bit = get_bit(mu_ddot_buf, i);
    r->coeffs[4*i + 0] = (r->coeffs[4*i + 0] + quarter_q * bit);
    r->coeffs[4*i + 1] = (r->coeffs[4*i + 1] + quarter_q * bit);
    r->coeffs[4*i + 2] = (r->coeffs[4*i + 2] + quarter_q * bit);
    r->coeffs[4*i + 3] = (r->coeffs[4*i + 3] + quarter_q * bit);
  }
#endif
}

/*************************************************
* Name:        poly_tomsg
*
* Description: Convert polynomial to 32-byte message
*
* Arguments:   - uint8_t *msg: pointer to output message
*              - const poly *a: pointer to input polynomial
**************************************************/
// kyber原代码
// void poly_tomsg(uint8_t msg[KYBER_INDCPA_MSGBYTES], poly *a)
// {
//   unsigned int i,j;
//   uint16_t t;

//   poly_csubq(a);

//   for(i=0;i<KYBER_N/8;i++) {
//     msg[i] = 0;
//     for(j=0;j<8;j++) {
//       t = ((((uint16_t)a->coeffs[8*i+j] << 1) + KYBER_Q/2)/KYBER_Q) & 1;
//       msg[i] |= t << j;
//     }
//   }
// }

// 只修改ntt 代码
// void poly_tomsg(uint8_t msg[KYBER_INDCPA_MSGBYTES], poly *a)
// {
//   unsigned int i,j;
//   uint16_t t;

//   poly_csubq(a);

// #if KYBER_N == 128
//   // Level 1: 从128个系数中恢复出256个bit(32字节)
//   for(i=0;i<KYBER_INDCPA_MSGBYTES;i++) {
//     msg[i] = 0;
//     for(j=0;j<4;j++) {
//       t = ((((uint32_t)a->coeffs[4*i+j] << 2) + KYBER_Q/2) / KYBER_Q) & 3;
//       msg[i] |= t << (2*j);
//     }
//   }

// #elif KYBER_N == 256 || KYBER_N == 512
//   // Level 2 & 3: 只要前256个系数被还原即可，后面补的0不需要解包
//   for(i=0;i<KYBER_INDCPA_MSGBYTES;i++) {
//     msg[i] = 0;
//     for(j=0;j<8;j++) {
//       t = ((((uint16_t)a->coeffs[8*i+j] << 1) + KYBER_Q/2)/KYBER_Q) & 1;
//       msg[i] |= t << j;
//     }
//   }
// #endif
// }

// Algorithm 5: MsgDecode
void poly_tomsg(uint8_t msg[KYBER_INDCPA_MSGBYTES], poly *a)
{
  int i, j;
  const int16_t q = KYBER_Q;           
  const int16_t half_q = (q + 1) / 2;  
  const int16_t quarter_q = q / 4;     
  memset(msg, 0, KYBER_INDCPA_MSGBYTES);

#if WEAVER_MODE == 1
  const int l_bar = 128;
  const int l_ddot = 0;
  const int h = 0;
#elif WEAVER_MODE == 3
  const int l_bar = 224;
  const int l_ddot = 32;
  const int h = 64;
#elif WEAVER_MODE == 5
  const int l_bar = 480;
  const int l_ddot = 32;
  const int h = 64;
#endif

  poly_csubq(a); // 保证系数在 [0, q-1]

  int16_t w_bar[KYBER_N];
  for(i = 0; i < KYBER_N; i++) {
    w_bar[i] = a->coeffs[i];
  }

#if WEAVER_MODE == 3 || WEAVER_MODE == 5
  // ==========================================================
  // Phase 1: 解码次高位 (Decode Lower bits)
  // ==========================================================
  uint8_t mu_ddot_noisy[8] = {0}; 
  for(i = 0; i < h; i++) {
    int32_t ee = 0;
    for(j = 0; j < 4; j++) {
      int16_t w_j = a->coeffs[4*i + j];
      if (w_j < 0) w_j += q;
      if (w_j >= q) w_j -= q;
      int16_t mod_val = w_j % half_q;
      int32_t diff = abs(mod_val - quarter_q);
      ee += diff; 
    }
    // 判决得到带噪的码字位
    uint8_t bit = (ee >= half_q) ? 0 : 1;
    set_bit(mu_ddot_noisy, i, bit);
  }

  // 关键步骤：纠错并提取出完美的纯数据
  decode_bch_low(mu_ddot_noisy, l_ddot / 8, mu_ddot_noisy + (l_ddot / 8));
  
  uint8_t *msg_low = msg + (l_bar / 8);
  memcpy(msg_low, mu_ddot_noisy, l_ddot / 8); // 将纯数据保存到输出区
  
  // ==========================================================
  // Phase 2: SIC 串行干扰消除 (Re-encode and Cancel)
  // ==========================================================
  // 利用纯净的数据，重新编码出没有任何噪声的完美码字！
  uint8_t mu_ddot_clean[8] = {0};
  memcpy(mu_ddot_clean, msg_low, l_ddot / 8);
  encode_bch_low(msg_low, l_ddot / 8, mu_ddot_clean + (l_ddot / 8));

  // 用完美的码字，把低位造成的干扰从多项式系数中彻底减掉
  for(i = 0; i < h; i++) {
    uint8_t clean_bit = get_bit(mu_ddot_clean, i);
    for(j = 0; j < 4; j++) {
      w_bar[4*i + j] = w_bar[4*i + j] - quarter_q * clean_bit;
      if (w_bar[4*i + j] < 0) {
          w_bar[4*i + j] += q; // 保证始终为正
      }
    }
  }
#endif

  // ==========================================================
  // Phase 3: 解码高位 (Decode Higher bits)
  // ==========================================================
  uint8_t mu_tilde[(KYBER_N + 7) / 8] = {0};
  
  // 此时的 w_bar，底层干扰已经被 SIC 清除了！
  for(i = 0; i < KYBER_N; i++) { 
    uint8_t bit = ((((uint32_t)w_bar[i] << 1) + q/2) / q) & 1;
    set_bit(mu_tilde, i, bit);
  }

  // BCH 纠错高位，并存入输出区
  decode_bch_high(mu_tilde, l_bar / 8, mu_tilde + (l_bar / 8));
  memcpy(msg, mu_tilde, l_bar / 8);
}