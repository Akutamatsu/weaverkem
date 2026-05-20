#ifndef POLY_COMPRESS9_H
#define POLY_COMPRESS9_H

#include <stdint.h>
#include "poly.h"

#define poly_compress9_coeff_scalar KYBER_NAMESPACE(_poly_compress9_coeff_scalar)
uint16_t poly_compress9_coeff_scalar(int16_t a);

#define poly_compress9_pack8 KYBER_NAMESPACE(_poly_compress9_pack8)
void poly_compress9_pack8(uint8_t r[9], const uint16_t t[8]);

#define poly_compress9_scalar KYBER_NAMESPACE(_poly_compress9_scalar)
void poly_compress9_scalar(uint8_t r[(KYBER_N * 9) / 8], const poly *a);

#if defined(WEAVER_USE_AVX_COMPRESS) && (KYBER_N == 256)
#define poly_compress9_quant_avx KYBER_NAMESPACE(_poly_compress9_quant_avx)
void poly_compress9_quant_avx(uint16_t t[KYBER_N], const poly *a);

#define poly_compress9_avx KYBER_NAMESPACE(_poly_compress9_avx)
void poly_compress9_avx(uint8_t r[(KYBER_N * 9) / 8], const poly *a);
#endif

#endif
