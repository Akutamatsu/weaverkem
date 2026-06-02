#ifndef POLY_COMPRESS_AVX_H
#define POLY_COMPRESS_AVX_H

#include "params.h"
#include "poly.h"

#if defined(WEAVER_USE_AVX_COMPRESS) && (WEAVER_N == 256)

#define poly_compress10_avx WEAVER_NAMESPACE(_poly_compress10_avx)
void poly_compress10_avx(uint8_t r[(WEAVER_N * 10) / 8], const poly *a);

#define poly_decompress10_avx WEAVER_NAMESPACE(_poly_decompress10_avx)
void poly_decompress10_avx(poly *r, const uint8_t a[(WEAVER_N * 10) / 8]);

#if (WEAVER_POLYCOMPRESSEDBYTES == (WEAVER_N * 4 / 8))
#define poly_compress_d4_avx WEAVER_NAMESPACE(_poly_compress_d4_avx)
void poly_compress_d4_avx(uint8_t r[WEAVER_POLYCOMPRESSEDBYTES], const poly *a);

#define poly_decompress_d4_avx WEAVER_NAMESPACE(_poly_decompress_d4_avx)
void poly_decompress_d4_avx(poly *r, const uint8_t a[WEAVER_POLYCOMPRESSEDBYTES]);
#endif

#endif

#endif
