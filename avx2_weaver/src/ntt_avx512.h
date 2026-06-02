#ifndef NTT_AVX512_H
#define NTT_AVX512_H

#include <stdint.h>
#include "params.h"
#include "polyvec.h"

#if defined(WEAVER_USE_AVX_NTT512) && (WEAVER_N == 512)

#define ntt512_avx WEAVER_NAMESPACE(_ntt512_avx)
void ntt512_avx(int16_t *r);

#define invntt512_avx WEAVER_NAMESPACE(_invntt512_avx)
void invntt512_avx(int16_t *r);

#define basemul512_avx WEAVER_NAMESPACE(_basemul512_avx)
void basemul512_avx(int16_t *r, const int16_t *a, const int16_t *b);

#define polyvec_basemul_acc_avx512 WEAVER_NAMESPACE(_polyvec_basemul_acc_avx512)
void polyvec_basemul_acc_avx512(poly *r, const polyvec *a, const polyvec *b);

#define poly_add512_avx WEAVER_NAMESPACE(_poly_add512_avx)
void poly_add512_avx(int16_t *r, const int16_t *a, const int16_t *b);

#define poly_sub512_avx WEAVER_NAMESPACE(_poly_sub512_avx)
void poly_sub512_avx(int16_t *r, const int16_t *a, const int16_t *b);

#endif

#endif
