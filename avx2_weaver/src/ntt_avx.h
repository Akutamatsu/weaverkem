#ifndef NTT_AVX_H
#define NTT_AVX_H

#include <stdint.h>
#include "params.h"

#if defined(WEAVER_AVX256_NTT)

#define ntt_avx KYBER_NAMESPACE(_ntt_avx)
void ntt_avx(int16_t *r, const int16_t *qdata);

#define invntt_avx KYBER_NAMESPACE(_invntt_avx)
void invntt_avx(int16_t *r, const int16_t *qdata);

#define basemul_avx KYBER_NAMESPACE(_basemul_avx)
void basemul_avx(int16_t *r, const int16_t *a, const int16_t *b,
                 const int16_t *qdata);

#define reduce_avx KYBER_NAMESPACE(_reduce_avx)
int16_t reduce_avx(int16_t *r, const int16_t *qdata);

#define tomont_avx KYBER_NAMESPACE(_tomont_avx)
int16_t tomont_avx(int16_t *r, const int16_t *qdata);

#define qdata KYBER_NAMESPACE(_qdata)
extern const int16_t KYBER_NAMESPACE(_qdata)[];

#define ntttobytes_avx KYBER_NAMESPACE(_ntttobytes_avx)
void ntttobytes_avx(uint8_t *r, const int16_t *a, const int16_t *qdata);

#define nttfrombytes_avx KYBER_NAMESPACE(_nttfrombytes_avx)
void nttfrombytes_avx(int16_t *r, const uint8_t *a, const int16_t *qdata);

#define nttunpack_avx KYBER_NAMESPACE(_nttunpack_avx)
void nttunpack_avx(int16_t *r, const int16_t *qdata);

#endif

#endif
