#include <stdint.h>
#include <stdlib.h>
#include "params.h"
#include "poly.h"
#include "bch.h"
#include <string.h>
#include "ntt.h"
#include "reduce.h"
#include "cbd.h"
#include "symmetric.h"


// --- 辅助函数：按位提取与写入 ---
static inline uint8_t get_bit(const uint8_t *in, int bit_pos) {
    return (in[bit_pos / 8] >> (bit_pos % 8)) & 1;
}
static inline void set_bit(uint8_t *out, int bit_pos, uint8_t val) {
    if(val) out[bit_pos / 8] |=  (1 << (bit_pos % 8));
    else    out[bit_pos / 8] &= ~(1 << (bit_pos % 8));
}

// // --- 辅助函数：正中心取模 (对应伪代码的 mod+ ) ---
// // 将 x 映射到 [-mod/2, mod/2) 区间
// static inline int16_t center_mod(int16_t x, int16_t mod) {
//     int16_t r = x % mod;
//     if (r > mod / 2) {
//         r -= mod;
//     }
//     return r;
// }

/*************************************************
* Name:        poly_compress
*
* Description: Compression and subsequent serialization of a polynomial
*
* Arguments:   - uint8_t *r: pointer to output byte array
*                            (of length KYBER_POLYCOMPRESSEDBYTES)
*              - poly *a:    pointer to input polynomial
**************************************************/
void poly_compress(uint8_t r[KYBER_POLYCOMPRESSEDBYTES], poly *a)
{
  unsigned int i,j;
  uint8_t t[8];

  poly_csubq(a);

#if (KYBER_POLYCOMPRESSEDBYTES == (KYBER_N * 4 / 8))
  for(i=0;i<KYBER_N/8;i++) {
    for(j=0;j<8;j++)
      t[j] = ((((uint16_t)a->coeffs[8*i+j] << 4) + KYBER_Q/2)/KYBER_Q) & 15;

    r[0] = t[0] | (t[1] << 4);
    r[1] = t[2] | (t[3] << 4);
    r[2] = t[4] | (t[5] << 4);
    r[3] = t[6] | (t[7] << 4);
    r += 4;
  }
#elif (KYBER_POLYCOMPRESSEDBYTES == (KYBER_N * 5 / 8))
  for(i=0;i<KYBER_N/8;i++) {
    for(j=0;j<8;j++)
      t[j] = ((((uint32_t)a->coeffs[8*i+j] << 5) + KYBER_Q/2)/KYBER_Q) & 31;

    r[0] = (t[0] >> 0) | (t[1] << 5);
    r[1] = (t[1] >> 3) | (t[2] << 2) | (t[3] << 7);
    r[2] = (t[3] >> 1) | (t[4] << 4);
    r[3] = (t[4] >> 4) | (t[5] << 1) | (t[6] << 6);
    r[4] = (t[6] >> 2) | (t[7] << 3);
    r += 5;
  }
#else
#error "KYBER_POLYCOMPRESSEDBYTES needs to be N*4/8 or N*5/8"
#endif
}

/*************************************************
* Name:        poly_decompress
*
* Description: De-serialization and subsequent decompression of a polynomial;
*              approximate inverse of poly_compress
*
* Arguments:   - poly *r:          pointer to output polynomial
*              - const uint8_t *a: pointer to input byte array
*                                  (of length KYBER_POLYCOMPRESSEDBYTES bytes)
**************************************************/
void poly_decompress(poly *r, const uint8_t a[KYBER_POLYCOMPRESSEDBYTES])
{
  unsigned int i;

#if (KYBER_POLYCOMPRESSEDBYTES == (KYBER_N * 4 / 8))
  for(i=0;i<KYBER_N/2;i++) {
    r->coeffs[2*i+0] = (((uint16_t)(a[0] & 15)*KYBER_Q) + 8) >> 4;
    r->coeffs[2*i+1] = (((uint16_t)(a[0] >> 4)*KYBER_Q) + 8) >> 4;
    a += 1;
  }
#elif (KYBER_POLYCOMPRESSEDBYTES == (KYBER_N * 5 / 8))
  unsigned int j;
  uint8_t t[8];
  for(i=0;i<KYBER_N/8;i++) {
    t[0] = (a[0] >> 0);
    t[1] = (a[0] >> 5) | (a[1] << 3);
    t[2] = (a[1] >> 2);
    t[3] = (a[1] >> 7) | (a[2] << 1);
    t[4] = (a[2] >> 4) | (a[3] << 4);
    t[5] = (a[3] >> 1);
    t[6] = (a[3] >> 6) | (a[4] << 2);
    t[7] = (a[4] >> 3);
    a += 5;

    for(j=0;j<8;j++)
      r->coeffs[8*i+j] = ((uint32_t)(t[j] & 31)*KYBER_Q + 16) >> 5;
  }
#else
#error "KYBER_POLYCOMPRESSEDBYTES needs to be N*4/8 or N*5/8"
#endif
}

/*************************************************
* Name:        poly_tobytes
*
* Description: Serialization of a polynomial
*
* Arguments:   - uint8_t *r: pointer to output byte array
*                            (needs space for KYBER_POLYBYTES bytes)
*              - poly *a:    pointer to input polynomial
**************************************************/
void poly_tobytes(uint8_t r[KYBER_POLYBYTES], poly *a)
{
  unsigned int i;
  uint16_t t0, t1;

  poly_csubq(a);

  for(i=0;i<KYBER_N/2;i++) {
    t0 = a->coeffs[2*i];
    t1 = a->coeffs[2*i+1];
    r[3*i+0] = (t0 >> 0);
    r[3*i+1] = (t0 >> 8) | (t1 << 4);
    r[3*i+2] = (t1 >> 4);
  }
}

/*************************************************
* Name:        poly_frombytes
*
* Description: De-serialization of a polynomial;
*              inverse of poly_tobytes
*
* Arguments:   - poly *r:          pointer to output polynomial
*              - const uint8_t *a: pointer to input byte array
*                                  (of KYBER_POLYBYTES bytes)
**************************************************/
void poly_frombytes(poly *r, const uint8_t a[KYBER_POLYBYTES])
{
  unsigned int i;
  for(i=0;i<KYBER_N/2;i++) {
    r->coeffs[2*i]   = ((a[3*i+0] >> 0) | ((uint16_t)a[3*i+1] << 8)) & 0xFFF;
    r->coeffs[2*i+1] = ((a[3*i+1] >> 4) | ((uint16_t)a[3*i+2] << 4)) & 0xFFF;
  }
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
void poly_frommsg(poly *r, const uint8_t msg[KYBER_INDCPA_MSGBYTES])
{
  int i;
  const int16_t q = KYBER_Q;           // 3329
  const int16_t half_q = (q + 1) / 2;  // 1665
  const int16_t quarter_q = q / 4;     // 832

#if KYBER_N == 128
  const int l_bar = 104;
#elif KYBER_N == 256
  const int l_bar = 224;
#elif KYBER_N == 512
  const int l_bar = 472;
#endif

  int h = KYBER_N - l_bar;

  // ==========================================================
  // Step 3: ECCEncode 获取 BCH 码字 (融合 LAC API 的拼接逻辑)
  // ==========================================================
  uint8_t mu_tilde[(KYBER_N + 7) / 8] = {0}; // 完整的 BCH 码字空间
  
  // 1. 先把高位明文数据拷贝进 mu_tilde 的前半段
  memcpy(mu_tilde, msg, l_bar / 8);
  
  // 2. 调用 LAC 的 encode_bch 计算校验位，存入 mu_tilde 偏移后的后半段
  encode_bch(msg, l_bar / 8, mu_tilde + (l_bar / 8)); 

  // ==========================================================
  // 编码过程
  // ==========================================================
  // Step 4: 初始化 w = 0
  for(i = 0; i < KYBER_N; i++) {
    r->coeffs[i] = 0;
  }

  // Step 5-7: Encode to Higher bits (叠加 BCH 码字)
  for(i = 0; i < KYBER_N; i++) { // BCH 编码长度比 N 少 1
    uint8_t bit = get_bit(mu_tilde, i);
    r->coeffs[i] = (r->coeffs[i] + half_q * bit) % q;
  }

  // Step 8-14: Encode to Lower bits (叠加低位数据的重复码)
  const uint8_t *mu_ddot_ptr = msg + (l_bar / 8); // 定位到低位数据的起始点
  for(i = 0; i < h; i++) {
    uint8_t bit = get_bit(mu_ddot_ptr, i);
    r->coeffs[4*i + 0] = (r->coeffs[4*i + 0] + quarter_q * bit) % q;
    r->coeffs[4*i + 1] = (r->coeffs[4*i + 1] + quarter_q * bit) % q;
    r->coeffs[4*i + 2] = (r->coeffs[4*i + 2] + quarter_q * bit) % q;
    r->coeffs[4*i + 3] = (r->coeffs[4*i + 3] + quarter_q * bit) % q;
  }
}

/*************************************************
* Name:        poly_tomsg
*
* Description: Convert polynomial to 32-byte message
*
* Arguments:   - uint8_t *msg: pointer to output message
*              - poly *a:      pointer to input polynomial
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
  const int16_t q = KYBER_Q;           // 3329
  const int16_t half_q = (q + 1) / 2;  // 1665
  const int16_t quarter_q = q / 4;     // 832

#if KYBER_N == 128
  const int l_bar = 104;
#elif KYBER_N == 256
  const int l_bar = 224;
#elif KYBER_N == 512
  const int l_bar = 472;
#endif

  // Step 1: h = n - l_bar 
  int h = KYBER_N - l_bar;

  poly_csubq(a); // 保证系数在 [0, q-1]

  // Step 2-8: Decode from Lower bits
  uint8_t mu_ddot[KYBER_N] = {0}; 
  for(i = 0; i < h; i++) {
    int32_t ee = 0;
    for(j = 0; j < 4; j++) {
      int16_t w_j = a->coeffs[4*i + j];
      
      // w_{4i+j} mod^+ (q+1)/2 (Step 3-6 里的 mod+ 操作)
      if (w_j < 0) w_j += q;
      if (w_j >= q) w_j -= q;
      int16_t mod_val = w_j % half_q;
      
      // | w_{4i+j} mod^+ (q+1)/2 - floor(q/4) |
      int32_t diff = abs(mod_val - quarter_q);
      ee += diff; // 累加 ee
    }
    // Step 7: mu_ddot_i = (ee >= (q+1)/2) ? 0 : 1
    mu_ddot[i] = (ee >= half_q) ? 0 : 1;
  }

  // Step 9: w_bar = w
  int16_t w_bar[KYBER_N];
  for(i = 0; i < KYBER_N; i++) {
    w_bar[i] = a->coeffs[i];
  }
  
  // Step 10-15: Remove Lower bits
  for(i = 0; i < h; i++) {
    for(j = 0; j < 4; j++) {
      w_bar[4*i + j] = w_bar[4*i + j] - quarter_q * mu_ddot[i];
      // 保证减去后，依然落在正确的正整数模环内
      if (w_bar[4*i + j] < 0) {
          w_bar[4*i + j] += q;
      }
    }
  }

  // Step 16-18: Decode from Higher bits
  uint8_t mu_tilde[(KYBER_N + 7) / 8] = {0};
  for(i = 0; i < KYBER_N; i++) { 
    // mu_tilde_i = round( (2/q) * w_bar_i )
    // 在整数域中等效于: (((w_bar_i * 2) + q/2) / q) & 1
    uint8_t bit = ((((uint32_t)w_bar[i] << 1) + q/2) / q) & 1;
    set_bit(mu_tilde, i, bit);
  }

  // Step 19: ECCDecode
  // 初始化输出空间
  memset(msg, 0, KYBER_INDCPA_MSGBYTES);
  uint8_t bch_data[(KYBER_N + 7) / 8] = {0};
  
  // 提取高位数据 (0 到 l_bar/8 - 1)
  memcpy(bch_data, mu_tilde, l_bar / 8);
  
  // 提取校验位并进行 BCH 解码纠错
  // 注意：LAC 的 decode_bch 函数会把纠正后的结果直接写回 bch_data 里
  decode_bch(bch_data, l_bar / 8, mu_tilde + (l_bar / 8));

  // Step 20: mu := mu_bar || mu_ddot
  // 把纠错后的高位数据复制给 msg
  memcpy(msg, bch_data, l_bar / 8);
  
  // 把低位数据通过指针偏移拼接到 msg 后面
  uint8_t *mu_ddot_ptr = msg + (l_bar / 8);
  for(i = 0; i < h; i++) {
    set_bit(mu_ddot_ptr, i, mu_ddot[i]);
  }
}

/*************************************************
* Name:        poly_getnoise_eta1
*
* Description: Sample a polynomial deterministically from a seed and a nonce,
*              with output polynomial close to centered binomial distribution
*              with parameter KYBER_ETA1
*
* Arguments:   - poly *r:             pointer to output polynomial
*              - const uint8_t *seed: pointer to input seed
*                                     (of length KYBER_SYMBYTES bytes)
*              - uint8_t nonce:       one-byte input nonce
**************************************************/
void poly_getnoise_eta1(poly *r, const uint8_t seed[KYBER_SYMBYTES], uint8_t nonce)
{
  uint8_t buf[KYBER_ETA1*KYBER_N/4];
  prf(buf, sizeof(buf), seed, nonce);
  cbd_eta1(r, buf);
}

/*************************************************
* Name:        poly_getnoise_eta2
*
* Description: Sample a polynomial deterministically from a seed and a nonce,
*              with output polynomial close to centered binomial distribution
*              with parameter KYBER_ETA2
*
* Arguments:   - poly *r:             pointer to output polynomial
*              - const uint8_t *seed: pointer to input seed
*                                     (of length KYBER_SYMBYTES bytes)
*              - uint8_t nonce:       one-byte input nonce
**************************************************/
void poly_getnoise_eta2(poly *r, const uint8_t seed[KYBER_SYMBYTES], uint8_t nonce)
{
  uint8_t buf[KYBER_ETA2*KYBER_N/4];
  prf(buf, sizeof(buf), seed, nonce);
  cbd_eta2(r, buf);
}


/*************************************************
* Name:        poly_ntt
*
* Description: Computes negacyclic number-theoretic transform (NTT) of
*              a polynomial in place;
*              inputs assumed to be in normal order, output in bitreversed order
*
* Arguments:   - uint16_t *r: pointer to in/output polynomial
**************************************************/
void poly_ntt(poly *r)
{
  ntt(r->coeffs);
  poly_reduce(r);
}

/*************************************************
* Name:        poly_invntt_tomont
*
* Description: Computes inverse of negacyclic number-theoretic transform (NTT)
*              of a polynomial in place;
*              inputs assumed to be in bitreversed order, output in normal order
*
* Arguments:   - uint16_t *a: pointer to in/output polynomial
**************************************************/
void poly_invntt_tomont(poly *r)
{
  invntt(r->coeffs);
}

/*************************************************
* Name:        poly_basemul_montgomery
*
* Description: Multiplication of two polynomials in NTT domain
*
* Arguments:   - poly *r:       pointer to output polynomial
*              - const poly *a: pointer to first input polynomial
*              - const poly *b: pointer to second input polynomial
**************************************************/
// void poly_basemul_montgomery(poly *r, const poly *a, const poly *b)
// {
//   unsigned int i;
//   for(i=0;i<KYBER_N/4;i++) {
//     basemul(&r->coeffs[4*i], &a->coeffs[4*i], &b->coeffs[4*i], zetas[64+i]);
//     basemul(&r->coeffs[4*i+2], &a->coeffs[4*i+2], &b->coeffs[4*i+2],
//             -zetas[64+i]);
//   }
// }

void poly_basemul_montgomery(poly *r, const poly *a, const poly *b) {
  unsigned int i;

#if KYBER_N == 128
  // Level 1: 纯标量乘法 (彻底分解，无需 zeta)
  for(i = 0; i < KYBER_N; ++i) {
    r->coeffs[i] = montgomery_reduce((int32_t)a->coeffs[i] * b->coeffs[i]);
  }

#elif KYBER_N == 256
  // Level 2: 原版 Kyber 的 2 次多项式乘法
  // 64 对 2 次多项式，分别模 (X^2 - zeta) 和 (X^2 + zeta)
  // 所以步长是 4 个系数 (2*2)
  for(i = 0; i < KYBER_N / 4; ++i) {
    basemul(&r->coeffs[4*i],   &a->coeffs[4*i],   &b->coeffs[4*i],    zetas[64 + i]);
    basemul(&r->coeffs[4*i+2], &a->coeffs[4*i+2], &b->coeffs[4*i+2], -zetas[64 + i]);
  }

#elif KYBER_N == 512
  // Level 3: 4 次多项式乘法
  // 64 对 4 次多项式，分别模 (X^4 - zeta) 和 (X^4 + zeta)
  // 步长是 8 个系数 (4*2)
  for(i = 0; i < KYBER_N / 8; ++i) {
    basemul_degree4(&r->coeffs[8*i],   &a->coeffs[8*i],   &b->coeffs[8*i],    zetas[64 + i]);
    basemul_degree4(&r->coeffs[8*i+4], &a->coeffs[8*i+4], &b->coeffs[8*i+4], -zetas[64 + i]);
  }
#endif
}

/*************************************************
* Name:        poly_tomont
*
* Description: Inplace conversion of all coefficients of a polynomial
*              from normal domain to Montgomery domain
*
* Arguments:   - poly *r: pointer to input/output polynomial
**************************************************/
void poly_tomont(poly *r)
{
  unsigned int i;
  const int16_t f = (1ULL << 32) % KYBER_Q;
  for(i=0;i<KYBER_N;i++)
    r->coeffs[i] = montgomery_reduce((int32_t)r->coeffs[i]*f);
}

/*************************************************
* Name:        poly_reduce
*
* Description: Applies Barrett reduction to all coefficients of a polynomial
*              for details of the Barrett reduction see comments in reduce.c
*
* Arguments:   - poly *r: pointer to input/output polynomial
**************************************************/
void poly_reduce(poly *r)
{
  unsigned int i;
  for(i=0;i<KYBER_N;i++)
    r->coeffs[i] = barrett_reduce(r->coeffs[i]);
}

/*************************************************
* Name:        poly_csubq
*
* Description: Applies conditional subtraction of q to each coefficient
*              of a polynomial. For details of conditional subtraction
*              of q see comments in reduce.c
*
* Arguments:   - poly *r: pointer to input/output polynomial
**************************************************/
void poly_csubq(poly *r)
{
  unsigned int i;
  for(i=0;i<KYBER_N;i++)
    r->coeffs[i] = csubq(r->coeffs[i]);
}

/*************************************************
* Name:        poly_add
*
* Description: Add two polynomials
*
* Arguments: - poly *r:       pointer to output polynomial
*            - const poly *a: pointer to first input polynomial
*            - const poly *b: pointer to second input polynomial
**************************************************/
void poly_add(poly *r, const poly *a, const poly *b)
{
  unsigned int i;
  for(i=0;i<KYBER_N;i++)
    r->coeffs[i] = a->coeffs[i] + b->coeffs[i];
}

/*************************************************
* Name:        poly_sub
*
* Description: Subtract two polynomials
*
* Arguments: - poly *r:       pointer to output polynomial
*            - const poly *a: pointer to first input polynomial
*            - const poly *b: pointer to second input polynomial
**************************************************/
void poly_sub(poly *r, const poly *a, const poly *b)
{
  unsigned int i;
  for(i=0;i<KYBER_N;i++)
    r->coeffs[i] = a->coeffs[i] - b->coeffs[i];
}
