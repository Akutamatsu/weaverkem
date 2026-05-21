/* AVX2 gen_matrix: shake128x4 + rej_uniform_avx (pq-crystals kyber avx2). */
#include <stdint.h>
#include <immintrin.h>
#include "params.h"

#if defined(WEAVER_AVX_GEN_MATRIX) && (KYBER_N == 256)

#include "indcpa.h"
#include "polyvec.h"
#include "poly.h"
#include "symmetric.h"
#include "fips202.h"
#include "fips202x4.h"
#include "rejsample.h"

#define GEN_MATRIX_NBLOCKS (AVX_REJ_UNIFORM_BUFLEN / XOF_BLOCKBYTES)

static unsigned int rej_uniform(int16_t *r,
                                unsigned int len,
                                const uint8_t *buf,
                                unsigned int buflen)
{
  unsigned int ctr, pos;
  uint16_t val0, val1;

  ctr = pos = 0;
  while(ctr < len && pos + 3 <= buflen) {
    val0 = ((buf[pos+0] >> 0) | ((uint16_t)buf[pos+1] << 8)) & 0xFFF;
    val1 = ((buf[pos+1] >> 4) | ((uint16_t)buf[pos+2] << 4));
    pos += 3;

    if(val0 < KYBER_Q)
      r[ctr++] = val0;
    if(ctr < len && val1 < KYBER_Q)
      r[ctr++] = val1;
  }

  return ctr;
}

#if KYBER_K == 2
void gen_matrix(polyvec *a, const uint8_t seed[KYBER_SYMBYTES], int transposed)
{
  unsigned int ctr0, ctr1, ctr2, ctr3;
  __attribute__((aligned(32)))
  uint8_t buf[4][AVX_REJ_UNIFORM_BUFLEN];
  __m256i f;
  keccakx4_state state;

  f = _mm256_load_si256((__m256i *)seed);
  _mm256_store_si256((__m256i *)buf[0], f);
  _mm256_store_si256((__m256i *)buf[1], f);
  _mm256_store_si256((__m256i *)buf[2], f);
  _mm256_store_si256((__m256i *)buf[3], f);

  if(transposed) {
    buf[0][KYBER_SYMBYTES+0] = 0;
    buf[0][KYBER_SYMBYTES+1] = 0;
    buf[1][KYBER_SYMBYTES+0] = 0;
    buf[1][KYBER_SYMBYTES+1] = 1;
    buf[2][KYBER_SYMBYTES+0] = 1;
    buf[2][KYBER_SYMBYTES+1] = 0;
    buf[3][KYBER_SYMBYTES+0] = 1;
    buf[3][KYBER_SYMBYTES+1] = 1;
  } else {
    buf[0][KYBER_SYMBYTES+0] = 0;
    buf[0][KYBER_SYMBYTES+1] = 0;
    buf[1][KYBER_SYMBYTES+0] = 1;
    buf[1][KYBER_SYMBYTES+1] = 0;
    buf[2][KYBER_SYMBYTES+0] = 0;
    buf[2][KYBER_SYMBYTES+1] = 1;
    buf[3][KYBER_SYMBYTES+0] = 1;
    buf[3][KYBER_SYMBYTES+1] = 1;
  }

  shake128x4_absorb(&state, buf[0], buf[1], buf[2], buf[3], KYBER_SYMBYTES+2);
  shake128x4_squeezeblocks(buf[0], buf[1], buf[2], buf[3], GEN_MATRIX_NBLOCKS,
                           &state);

  ctr0 = rej_uniform_avx(a[0].vec[0].coeffs, buf[0]);
  ctr1 = rej_uniform_avx(a[0].vec[1].coeffs, buf[1]);
  ctr2 = rej_uniform_avx(a[1].vec[0].coeffs, buf[2]);
  ctr3 = rej_uniform_avx(a[1].vec[1].coeffs, buf[3]);

  while(ctr0 < KYBER_N || ctr1 < KYBER_N || ctr2 < KYBER_N || ctr3 < KYBER_N) {
    shake128x4_squeezeblocks(buf[0], buf[1], buf[2], buf[3], 1, &state);

    ctr0 += rej_uniform(a[0].vec[0].coeffs + ctr0, KYBER_N - ctr0, buf[0],
                        XOF_BLOCKBYTES);
    ctr1 += rej_uniform(a[0].vec[1].coeffs + ctr1, KYBER_N - ctr1, buf[1],
                        XOF_BLOCKBYTES);
    ctr2 += rej_uniform(a[1].vec[0].coeffs + ctr2, KYBER_N - ctr2, buf[2],
                        XOF_BLOCKBYTES);
    ctr3 += rej_uniform(a[1].vec[1].coeffs + ctr3, KYBER_N - ctr3, buf[3],
                        XOF_BLOCKBYTES);
  }

  poly_nttunpack(&a[0].vec[0]);
  poly_nttunpack(&a[0].vec[1]);
  poly_nttunpack(&a[1].vec[0]);
  poly_nttunpack(&a[1].vec[1]);
}

#elif KYBER_K == 3
void gen_matrix(polyvec *a, const uint8_t seed[KYBER_SYMBYTES], int transposed)
{
  unsigned int ctr0, ctr1, ctr2, ctr3;
  __attribute__((aligned(32)))
  uint8_t buf[4][(GEN_MATRIX_NBLOCKS*XOF_BLOCKBYTES+31)/32*32];
  __m256i f;
  keccakx4_state state;
  keccak_state state1x;

  f = _mm256_load_si256((__m256i *)seed);
  _mm256_store_si256((__m256i *)buf[0], f);
  _mm256_store_si256((__m256i *)buf[1], f);
  _mm256_store_si256((__m256i *)buf[2], f);
  _mm256_store_si256((__m256i *)buf[3], f);

  if(transposed) {
    buf[0][KYBER_SYMBYTES+0] = 0;
    buf[0][KYBER_SYMBYTES+1] = 0;
    buf[1][KYBER_SYMBYTES+0] = 0;
    buf[1][KYBER_SYMBYTES+1] = 1;
    buf[2][KYBER_SYMBYTES+0] = 0;
    buf[2][KYBER_SYMBYTES+1] = 2;
    buf[3][KYBER_SYMBYTES+0] = 1;
    buf[3][KYBER_SYMBYTES+1] = 0;
  } else {
    buf[0][KYBER_SYMBYTES+0] = 0;
    buf[0][KYBER_SYMBYTES+1] = 0;
    buf[1][KYBER_SYMBYTES+0] = 1;
    buf[1][KYBER_SYMBYTES+1] = 0;
    buf[2][KYBER_SYMBYTES+0] = 2;
    buf[2][KYBER_SYMBYTES+1] = 0;
    buf[3][KYBER_SYMBYTES+0] = 0;
    buf[3][KYBER_SYMBYTES+1] = 1;
  }

  shake128x4_absorb(&state, buf[0], buf[1], buf[2], buf[3], KYBER_SYMBYTES+2);
  shake128x4_squeezeblocks(buf[0], buf[1], buf[2], buf[3], GEN_MATRIX_NBLOCKS,
                           &state);

  ctr0 = rej_uniform_avx(a[0].vec[0].coeffs, buf[0]);
  ctr1 = rej_uniform_avx(a[0].vec[1].coeffs, buf[1]);
  ctr2 = rej_uniform_avx(a[0].vec[2].coeffs, buf[2]);
  ctr3 = rej_uniform_avx(a[1].vec[0].coeffs, buf[3]);

  while(ctr0 < KYBER_N || ctr1 < KYBER_N || ctr2 < KYBER_N || ctr3 < KYBER_N) {
    shake128x4_squeezeblocks(buf[0], buf[1], buf[2], buf[3], 1, &state);

    ctr0 += rej_uniform(a[0].vec[0].coeffs + ctr0, KYBER_N - ctr0, buf[0],
                        XOF_BLOCKBYTES);
    ctr1 += rej_uniform(a[0].vec[1].coeffs + ctr1, KYBER_N - ctr1, buf[1],
                        XOF_BLOCKBYTES);
    ctr2 += rej_uniform(a[0].vec[2].coeffs + ctr2, KYBER_N - ctr2, buf[2],
                        XOF_BLOCKBYTES);
    ctr3 += rej_uniform(a[1].vec[0].coeffs + ctr3, KYBER_N - ctr3, buf[3],
                        XOF_BLOCKBYTES);
  }

  poly_nttunpack(&a[0].vec[0]);
  poly_nttunpack(&a[0].vec[1]);
  poly_nttunpack(&a[0].vec[2]);
  poly_nttunpack(&a[1].vec[0]);

  f = _mm256_load_si256((__m256i *)seed);
  _mm256_store_si256((__m256i *)buf[0], f);
  _mm256_store_si256((__m256i *)buf[1], f);
  _mm256_store_si256((__m256i *)buf[2], f);
  _mm256_store_si256((__m256i *)buf[3], f);

  if(transposed) {
    buf[0][KYBER_SYMBYTES+0] = 1;
    buf[0][KYBER_SYMBYTES+1] = 1;
    buf[1][KYBER_SYMBYTES+0] = 1;
    buf[1][KYBER_SYMBYTES+1] = 2;
    buf[2][KYBER_SYMBYTES+0] = 2;
    buf[2][KYBER_SYMBYTES+1] = 0;
    buf[3][KYBER_SYMBYTES+0] = 2;
    buf[3][KYBER_SYMBYTES+1] = 1;
  } else {
    buf[0][KYBER_SYMBYTES+0] = 1;
    buf[0][KYBER_SYMBYTES+1] = 1;
    buf[1][KYBER_SYMBYTES+0] = 2;
    buf[1][KYBER_SYMBYTES+1] = 1;
    buf[2][KYBER_SYMBYTES+0] = 0;
    buf[2][KYBER_SYMBYTES+1] = 2;
    buf[3][KYBER_SYMBYTES+0] = 1;
    buf[3][KYBER_SYMBYTES+1] = 2;
  }

  shake128x4_absorb(&state, buf[0], buf[1], buf[2], buf[3], KYBER_SYMBYTES+2);
  shake128x4_squeezeblocks(buf[0], buf[1], buf[2], buf[3], GEN_MATRIX_NBLOCKS,
                           &state);

  ctr0 = rej_uniform_avx(a[1].vec[1].coeffs, buf[0]);
  ctr1 = rej_uniform_avx(a[1].vec[2].coeffs, buf[1]);
  ctr2 = rej_uniform_avx(a[2].vec[0].coeffs, buf[2]);
  ctr3 = rej_uniform_avx(a[2].vec[1].coeffs, buf[3]);

  while(ctr0 < KYBER_N || ctr1 < KYBER_N || ctr2 < KYBER_N || ctr3 < KYBER_N) {
    shake128x4_squeezeblocks(buf[0], buf[1], buf[2], buf[3], 1, &state);

    ctr0 += rej_uniform(a[1].vec[1].coeffs + ctr0, KYBER_N - ctr0, buf[0],
                        XOF_BLOCKBYTES);
    ctr1 += rej_uniform(a[1].vec[2].coeffs + ctr1, KYBER_N - ctr1, buf[1],
                        XOF_BLOCKBYTES);
    ctr2 += rej_uniform(a[2].vec[0].coeffs + ctr2, KYBER_N - ctr2, buf[2],
                        XOF_BLOCKBYTES);
    ctr3 += rej_uniform(a[2].vec[1].coeffs + ctr3, KYBER_N - ctr3, buf[3],
                        XOF_BLOCKBYTES);
  }

  poly_nttunpack(&a[1].vec[1]);
  poly_nttunpack(&a[1].vec[2]);
  poly_nttunpack(&a[2].vec[0]);
  poly_nttunpack(&a[2].vec[1]);

  f = _mm256_load_si256((__m256i *)seed);
  _mm256_store_si256((__m256i *)buf[0], f);
  buf[0][KYBER_SYMBYTES+0] = 2;
  buf[0][KYBER_SYMBYTES+1] = 2;
  /* One-shot absorb (Kyber-style); not incremental shake128_absorb. */
  shake128_absorb_once(&state1x, buf[0], KYBER_SYMBYTES+2);
  shake128_squeezeblocks(buf[0], GEN_MATRIX_NBLOCKS, &state1x);
  ctr0 = rej_uniform_avx(a[2].vec[2].coeffs, buf[0]);
  while(ctr0 < KYBER_N) {
    shake128_squeezeblocks(buf[0], 1, &state1x);
    ctr0 += rej_uniform(a[2].vec[2].coeffs + ctr0, KYBER_N - ctr0, buf[0],
                        XOF_BLOCKBYTES);
  }

  poly_nttunpack(&a[2].vec[2]);
}

#elif KYBER_K == 4
void gen_matrix(polyvec *a, const uint8_t seed[KYBER_SYMBYTES], int transposed)
{
  unsigned int i, ctr0, ctr1, ctr2, ctr3;
  __attribute__((aligned(32)))
  uint8_t buf[4][(GEN_MATRIX_NBLOCKS*XOF_BLOCKBYTES+31)/32*32];
  __m256i f;
  keccakx4_state state;

  for(i = 0; i < 4; i++) {
    f = _mm256_load_si256((__m256i *)seed);
    _mm256_store_si256((__m256i *)buf[0], f);
    _mm256_store_si256((__m256i *)buf[1], f);
    _mm256_store_si256((__m256i *)buf[2], f);
    _mm256_store_si256((__m256i *)buf[3], f);

    if(transposed) {
      buf[0][KYBER_SYMBYTES+0] = i;
      buf[0][KYBER_SYMBYTES+1] = 0;
      buf[1][KYBER_SYMBYTES+0] = i;
      buf[1][KYBER_SYMBYTES+1] = 1;
      buf[2][KYBER_SYMBYTES+0] = i;
      buf[2][KYBER_SYMBYTES+1] = 2;
      buf[3][KYBER_SYMBYTES+0] = i;
      buf[3][KYBER_SYMBYTES+1] = 3;
    } else {
      buf[0][KYBER_SYMBYTES+0] = 0;
      buf[0][KYBER_SYMBYTES+1] = i;
      buf[1][KYBER_SYMBYTES+0] = 1;
      buf[1][KYBER_SYMBYTES+1] = i;
      buf[2][KYBER_SYMBYTES+0] = 2;
      buf[2][KYBER_SYMBYTES+1] = i;
      buf[3][KYBER_SYMBYTES+0] = 3;
      buf[3][KYBER_SYMBYTES+1] = i;
    }

    shake128x4_absorb(&state, buf[0], buf[1], buf[2], buf[3], KYBER_SYMBYTES+2);
    shake128x4_squeezeblocks(buf[0], buf[1], buf[2], buf[3],
                             GEN_MATRIX_NBLOCKS, &state);

    ctr0 = rej_uniform_avx(a[i].vec[0].coeffs, buf[0]);
    ctr1 = rej_uniform_avx(a[i].vec[1].coeffs, buf[1]);
    ctr2 = rej_uniform_avx(a[i].vec[2].coeffs, buf[2]);
    ctr3 = rej_uniform_avx(a[i].vec[3].coeffs, buf[3]);

    while(ctr0 < KYBER_N || ctr1 < KYBER_N || ctr2 < KYBER_N || ctr3 < KYBER_N) {
      shake128x4_squeezeblocks(buf[0], buf[1], buf[2], buf[3], 1, &state);

      ctr0 += rej_uniform(a[i].vec[0].coeffs + ctr0, KYBER_N - ctr0, buf[0],
                          XOF_BLOCKBYTES);
      ctr1 += rej_uniform(a[i].vec[1].coeffs + ctr1, KYBER_N - ctr1, buf[1],
                          XOF_BLOCKBYTES);
      ctr2 += rej_uniform(a[i].vec[2].coeffs + ctr2, KYBER_N - ctr2, buf[2],
                          XOF_BLOCKBYTES);
      ctr3 += rej_uniform(a[i].vec[3].coeffs + ctr3, KYBER_N - ctr3, buf[3],
                          XOF_BLOCKBYTES);
    }

    poly_nttunpack(&a[i].vec[0]);
    poly_nttunpack(&a[i].vec[1]);
    poly_nttunpack(&a[i].vec[2]);
    poly_nttunpack(&a[i].vec[3]);
  }
}
#endif

#endif /* WEAVER_AVX_GEN_MATRIX && KYBER_N==256 */
