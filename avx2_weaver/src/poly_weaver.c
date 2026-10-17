#include <stdint.h>
#include "params.h"
#include "poly.h"
#include "cbd.h"
#include "symmetric.h"
#include "poly_compress_avx.h"

#if (KYBER_N == 512)
#include "ntt_avx512.h"
#endif

/*************************************************
* Name:        poly_compress
**************************************************/
void poly_compress(uint8_t r[KYBER_POLYCOMPRESSEDBYTES], const poly *a)
{
#if (KYBER_POLYCOMPRESSEDBYTES == (KYBER_N * 4 / 8))
  poly_compress_d4_avx(r, a);
#elif (KYBER_POLYCOMPRESSEDBYTES == (KYBER_N * 5 / 8))
  unsigned int i, j;
  int16_t u;
  uint8_t t[8];
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
  poly_compress_d6_avx(r, a);
#else
#error "KYBER_POLYCOMPRESSEDBYTES needs to be N*4/8, N*5/8, or N*6/8"
#endif
}

/*************************************************
* Name:        poly_decompress
**************************************************/
void poly_decompress(poly *r, const uint8_t a[KYBER_POLYCOMPRESSEDBYTES])
{
#if (KYBER_POLYCOMPRESSEDBYTES == (KYBER_N * 4 / 8))
  poly_decompress_d4_avx(r, a);
#elif (KYBER_POLYCOMPRESSEDBYTES == (KYBER_N * 5 / 8))
  unsigned int i, j;
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
  poly_decompress_d6_avx(r, a);
#else
#error "KYBER_POLYCOMPRESSEDBYTES needs to be N*4/8, N*5/8, or N*6/8"
#endif
}

#if KYBER_N == 512
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
#endif

void poly_getnoise_eta1(poly *r, const uint8_t seed[KYBER_SYMBYTES], uint8_t nonce)
{
  uint8_t buf[KYBER_ETA1*KYBER_N/4];
  prf(buf, sizeof(buf), seed, nonce);
  cbd_eta1(r, buf);
}

#if KYBER_N == 512

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

void poly_tomont(poly *r)
{
  poly_tomont512_avx(r->coeffs);
}

void poly_reduce(poly *r)
{
  poly_reduce512_avx(r->coeffs);
}
#endif
