#ifndef NTT7681_I16_AVX_H
#define NTT7681_I16_AVX_H

#include <stdint.h>

#define NTT7681_I16_N 256

void ntt7681_i16_init(void);
void ntt7681_i16_ntt(uint16_t a[NTT7681_I16_N]);
void ntt7681_i16_invntt(uint16_t a[NTT7681_I16_N]);
void ntt7681_i16_basemul(uint16_t r[NTT7681_I16_N],
                         const uint16_t a[NTT7681_I16_N],
                         const uint16_t b[NTT7681_I16_N]);

#endif
