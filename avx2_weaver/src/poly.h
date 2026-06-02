#ifndef POLY_H
#define POLY_H

#include <stdint.h>
#include "params.h"

/*
 * Elements of R_q = Z_q[X]/(X^n + 1). Represents polynomial
 * coeffs[0] + X*coeffs[1] + X^2*coeffs[2] + ... + X^{n-1}*coeffs[n-1]
 */
typedef struct{
#if defined(WEAVER_AVX256_NTT) || defined(WEAVER_USE_AVX_NTT512)
  int16_t coeffs[WEAVER_N] __attribute__((aligned(32)));
#else
  int16_t coeffs[WEAVER_N];
#endif
} poly;

#define poly_compress WEAVER_NAMESPACE(_poly_compress)
void poly_compress(uint8_t r[WEAVER_POLYCOMPRESSEDBYTES], const poly *a);
#define poly_decompress WEAVER_NAMESPACE(_poly_decompress)
void poly_decompress(poly *r, const uint8_t a[WEAVER_POLYCOMPRESSEDBYTES]);

#define poly_tobytes WEAVER_NAMESPACE(_poly_tobytes)
void poly_tobytes(uint8_t r[WEAVER_POLYBYTES], const poly *a);
#define poly_frombytes WEAVER_NAMESPACE(_poly_frombytes)
void poly_frombytes(poly *r, const uint8_t a[WEAVER_POLYBYTES]);

#define poly_getnoise_eta1 WEAVER_NAMESPACE(_poly_getnoise_eta1)
void poly_getnoise_eta1(poly *r, const uint8_t seed[WEAVER_SYMBYTES], uint8_t nonce);

#define poly_ntt WEAVER_NAMESPACE(_poly_ntt)
void poly_ntt(poly *r);
#define poly_invntt_tomont WEAVER_NAMESPACE(_poly_invntt_tomont)
void poly_invntt_tomont(poly *r);
#define poly_nttunpack WEAVER_NAMESPACE(_poly_nttunpack)
void poly_nttunpack(poly *r);
#define poly_basemul_montgomery WEAVER_NAMESPACE(_poly_basemul_montgomery)
void poly_basemul_montgomery(poly *r, const poly *a, const poly *b);
#define poly_tomont WEAVER_NAMESPACE(_poly_tomont)
void poly_tomont(poly *r);

#define poly_reduce WEAVER_NAMESPACE(_poly_reduce)
void poly_reduce(poly *r);

#define poly_add WEAVER_NAMESPACE(_poly_add)
void poly_add(poly *r, const poly *a, const poly *b);
#define poly_sub WEAVER_NAMESPACE(_poly_sub)
void poly_sub(poly *r, const poly *a, const poly *b);

#endif
