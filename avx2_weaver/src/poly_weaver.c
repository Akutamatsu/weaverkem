#include <stdint.h>
#include <stdlib.h>
#include "params.h"
#include "poly.h"
#include <string.h>
#include "ntt.h"
#include "reduce.h"
#include "cbd.h"
#include "symmetric.h"
#if defined(WEAVER_USE_AVX_COMPRESS)
#include "poly_compress_avx.h"
#endif

#if defined(WEAVER_USE_AVX_NTT512)
#include "ntt_avx512.h"
#endif

#if (KYBER_N == 512) && defined(WEAVER_USE_AVX_FQ_512)
#define reduce_avx_fq KYBER_NAMESPACE(_reduce_avx)
extern int16_t reduce_avx_fq(int16_t *r, const int16_t *qdata);

#define reduce512_avx_fq KYBER_NAMESPACE(_reduce512_avx)
extern int16_t reduce512_avx_fq(int16_t *r, const int16_t *qdata);

#define tomont_avx_fq KYBER_NAMESPACE(_tomont_avx)
extern int16_t tomont_avx_fq(int16_t *r, const int16_t *qdata);

#define tomont512_avx_fq KYBER_NAMESPACE(_tomont512_avx)
extern int16_t tomont512_avx_fq(int16_t *r, const int16_t *qdata);

#define qdata_fq KYBER_NAMESPACE(_qdata)
extern const int16_t qdata_fq[];
#endif

/*************************************************
* Name:        poly_compress
**************************************************/
void poly_compress(uint8_t r[KYBER_POLYCOMPRESSEDBYTES], const poly *a)
{
  unsigned int i,j;
  int16_t u;
  uint8_t t[8];


#if (KYBER_POLYCOMPRESSEDBYTES == (KYBER_N * 4 / 8))
#if defined(WEAVER_USE_AVX_COMPRESS) && (KYBER_N == 256)
  poly_compress_d4_avx(r, a);
#else
  for(i=0;i<KYBER_N/8;i++) {
    for(j=0;j<8;j++) {
      u  = a->coeffs[8*i+j];
      u += (u >> 15) & KYBER_Q;
      t[j] = ((((uint16_t)u << 4) + KYBER_Q/2)/KYBER_Q) & 15;
    }

    r[0] = t[0] | (t[1] << 4);
    r[1] = t[2] | (t[3] << 4);
    r[2] = t[4] | (t[5] << 4);
    r[3] = t[6] | (t[7] << 4);
    r += 4;
  }
#endif
#elif (KYBER_POLYCOMPRESSEDBYTES == (KYBER_N * 5 / 8))
  for(i=0;i<KYBER_N/8;i++) {
    for(j=0;j<8;j++) {
      u  = a->coeffs[8*i+j];
      u += (u >> 15) & KYBER_Q;
      t[j] = ((((uint32_t)u << 5) + KYBER_Q/2)/KYBER_Q) & 31;
    }

    r[0] = (t[0] >> 0) | (t[1] << 5);
    r[1] = (t[1] >> 3) | (t[2] << 2) | (t[3] << 7);
    r[2] = (t[3] >> 1) | (t[4] << 4);
    r[3] = (t[4] >> 4) | (t[5] << 1) | (t[6] << 6);
    r[4] = (t[6] >> 2) | (t[7] << 3);
    r += 5;
  }
  #elif (KYBER_POLYCOMPRESSEDBYTES == (KYBER_N * 6 / 8))
  for(i=0;i<KYBER_N/4;i++) {
    for(j=0;j<4;j++) {
      u  = a->coeffs[4*i+j];
      u += (u >> 15) & KYBER_Q;
      t[j] = ((((uint32_t)u << 6) + KYBER_Q/2)/KYBER_Q) & 63;
    }
    r[0] = (t[0] >> 0) | (t[1] << 6);
    r[1] = (t[1] >> 2) | (t[2] << 4);
    r[2] = (t[2] >> 4) | (t[3] << 2);
    r += 3;
  }
#else
#error "KYBER_POLYCOMPRESSEDBYTES needs to be N*4/8, N*5/8, or N*6/8"
#endif
}

/*************************************************
* Name:        poly_decompress
**************************************************/
void poly_decompress(poly *r, const uint8_t a[KYBER_POLYCOMPRESSEDBYTES])
{
  unsigned int i;

#if (KYBER_POLYCOMPRESSEDBYTES == (KYBER_N * 4 / 8))
#if defined(WEAVER_USE_AVX_COMPRESS) && (KYBER_N == 256)
  poly_decompress_d4_avx(r, a);
#else
  for(i=0;i<KYBER_N/2;i++) {
    r->coeffs[2*i+0] = (((uint16_t)(a[0] & 15)*KYBER_Q) + 8) >> 4;
    r->coeffs[2*i+1] = (((uint16_t)(a[0] >> 4)*KYBER_Q) + 8) >> 4;
    a += 1;
  }
#endif
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
#elif (KYBER_POLYCOMPRESSEDBYTES == (KYBER_N * 6 / 8))
  unsigned int j;
  uint8_t t[4];
  for(i=0;i<KYBER_N/4;i++) {
    t[0] = (a[0] >> 0);
    t[1] = (a[0] >> 6) | (a[1] << 2);
    t[2] = (a[1] >> 4) | (a[2] << 4);
    t[3] = (a[2] >> 2);
    a += 3;

    for(j=0;j<4;j++)
      r->coeffs[4*i+j] = ((uint32_t)(t[j] & 63)*KYBER_Q + 32) >> 6;
  }
#else
#error "KYBER_POLYCOMPRESSEDBYTES needs to be N*4/8, N*5/8, or N*6/8"
#endif
}

#if !defined(WEAVER_AVX256_NTT)
void poly_tobytes(uint8_t r[KYBER_POLYBYTES], const poly *a)
{
  unsigned int i;
  uint16_t t0, t1;

  for(i=0;i<KYBER_N/2;i++) {
    t0  = a->coeffs[2*i];
    t0 += ((int16_t)t0 >> 15) & KYBER_Q;
    t1 = a->coeffs[2*i+1];
    t1 += ((int16_t)t1 >> 15) & KYBER_Q;
    r[3*i+0] = (t0 >> 0);
    r[3*i+1] = (t0 >> 8) | (t1 << 4);
    r[3*i+2] = (t1 >> 4);
  }
}

void poly_frombytes(poly *r, const uint8_t a[KYBER_POLYBYTES])
{
  unsigned int i;
  for(i=0;i<KYBER_N/2;i++) {
    r->coeffs[2*i]   = ((a[3*i+0] >> 0) | ((uint16_t)a[3*i+1] << 8)) & 0xFFF;
    r->coeffs[2*i+1] = ((a[3*i+1] >> 4) | ((uint16_t)a[3*i+2] << 4)) & 0xFFF;
  }
}
#endif /* !WEAVER_AVX256_NTT */

void poly_getnoise_eta1(poly *r, const uint8_t seed[KYBER_SYMBYTES], uint8_t nonce)
{
  uint8_t buf[KYBER_ETA1*KYBER_N/4];
  prf(buf, sizeof(buf), seed, nonce);
  cbd_eta1(r, buf);
}

#if defined(WEAVER_USE_AVX_NTT512)

void poly_ntt(poly *r)
{
  ntt512_avx(r->coeffs);
  poly_reduce(r);
}

void poly_invntt_tomont(poly *r)
{
  invntt512_avx(r->coeffs);
}

void poly_basemul_montgomery(poly *r, const poly *a, const poly *b)
{
  basemul512_avx(r->coeffs, a->coeffs, b->coeffs);
}

void poly_nttunpack(poly *r)
{
  (void)r;
}

void poly_add(poly *r, const poly *a, const poly *b)
{
  poly_add512_avx(r->coeffs, a->coeffs, b->coeffs);
}

void poly_sub(poly *r, const poly *a, const poly *b)
{
  poly_sub512_avx(r->coeffs, a->coeffs, b->coeffs);
}

#elif !defined(WEAVER_AVX256_NTT)

void poly_ntt(poly *r)
{
  ntt(r->coeffs);
  poly_reduce(r);
}

void poly_invntt_tomont(poly *r)
{
  invntt(r->coeffs);
}

void poly_basemul_montgomery(poly *r, const poly *a, const poly *b) {
  unsigned int i;

#if KYBER_N == 128
  for(i = 0; i < KYBER_N; ++i) {
    r->coeffs[i] = montgomery_reduce((int32_t)a->coeffs[i] * b->coeffs[i]);
  }

#elif KYBER_N == 256
  for(i = 0; i < KYBER_N / 4; ++i) {
    basemul(&r->coeffs[4*i],   &a->coeffs[4*i],   &b->coeffs[4*i],    zetas[64 + i]);
    basemul(&r->coeffs[4*i+2], &a->coeffs[4*i+2], &b->coeffs[4*i+2], -zetas[64 + i]);
  }

#elif KYBER_N == 512
  for(i = 0; i < KYBER_N / 8; ++i) {
    basemul_degree4(&r->coeffs[8*i],   &a->coeffs[8*i],   &b->coeffs[8*i],    zetas[64 + i]);
    basemul_degree4(&r->coeffs[8*i+4], &a->coeffs[8*i+4], &b->coeffs[8*i+4], -zetas[64 + i]);
  }
#endif
}

void poly_nttunpack(poly *r)
{
  (void)r;
}

void poly_add(poly *r, const poly *a, const poly *b)
{
  unsigned int i;
  for(i=0;i<KYBER_N;i++)
    r->coeffs[i] = a->coeffs[i] + b->coeffs[i];
}

void poly_sub(poly *r, const poly *a, const poly *b)
{
  unsigned int i;
  for(i=0;i<KYBER_N;i++)
    r->coeffs[i] = a->coeffs[i] - b->coeffs[i];
}

#endif /* !WEAVER_AVX256_NTT && !WEAVER_USE_AVX_NTT512 */

#if defined(WEAVER_USE_AVX_NTT512) || !defined(WEAVER_AVX256_NTT)

void poly_tomont(poly *r)
{
#if (KYBER_N == 512) && defined(WEAVER_USE_AVX_FQ_512)
  tomont512_avx_fq(r->coeffs, qdata_fq);
#else
  unsigned int i;
  const int16_t f = (1ULL << 32) % KYBER_Q;
  for(i=0;i<KYBER_N;i++)
    r->coeffs[i] = montgomery_reduce((int32_t)r->coeffs[i]*f);
#endif
}

void poly_reduce(poly *r)
{
#if (KYBER_N == 512) && defined(WEAVER_USE_AVX_FQ_512)
  unsigned int i;
  int32_t diff;
  int16_t mask;

  reduce512_avx_fq(r->coeffs, qdata_fq);
  for(i = 0; i < KYBER_N; i++) {
    diff = (int32_t)r->coeffs[i] - KYBER_Q;
    mask = (int16_t)(((uint32_t)diff | (uint32_t)(-diff)) >> 31) - 1;
    r->coeffs[i] = r->coeffs[i] + (mask & (int16_t)(-KYBER_Q));
    r->coeffs[i] = barrett_reduce(r->coeffs[i]);
  }
#else
  unsigned int i;
  for(i=0;i<KYBER_N;i++)
    r->coeffs[i] = barrett_reduce(r->coeffs[i]);
#endif
}

#endif /* WEAVER_USE_AVX_NTT512 || !WEAVER_AVX256_NTT */
