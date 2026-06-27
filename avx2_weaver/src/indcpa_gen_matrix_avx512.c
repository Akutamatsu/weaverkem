/* Mode 5 (n=512): SHAKE128x4 parallel XOF + scalar rej_uniform (standard coefficient order).
 * No Kyber packed layout and no poly_nttunpack. */
#include <stdint.h>
#include <string.h>
#include <immintrin.h>
#include "params.h"

#if defined(WEAVER_AVX_GEN_MATRIX512) && (WEAVER_N == 512) && (WEAVER_K == 4)

#include "indcpa.h"
#include "polyvec.h"
#include "poly.h"
#include "symmetric.h"
#include "fips202.h"
#include "fips202x4.h"

#if(XOF_BLOCKBYTES % 3)
#error "gen_matrix assumes XOF_BLOCKBYTES is a multiple of 3"
#endif

#define GEN_MATRIX_NBLOCKS \
  ((12 * WEAVER_N / 8 * (1 << 12) / WEAVER_Q + XOF_BLOCKBYTES) / XOF_BLOCKBYTES)

#define GEN_MATRIX_BUFLEN (GEN_MATRIX_NBLOCKS * XOF_BLOCKBYTES)

/* Same rejection sampling as indcpa.c (standard coefficient order). */
static unsigned int rej_uniform_scalar(int16_t *r,
                                         unsigned int len,
                                         const uint8_t *buf,
                                         unsigned int buflen)
{
  unsigned int ctr, pos;
  uint16_t val0, val1;

  ctr = pos = 0;
  while(ctr < len && pos + 3 <= buflen) {
    val0 = ((buf[pos + 0] >> 0) | ((uint16_t)buf[pos + 1] << 8)) & 0xFFF;
    val1 = ((buf[pos + 1] >> 4) | ((uint16_t)buf[pos + 2] << 4)) & 0xFFF;
    pos += 3;

    if(val0 < WEAVER_Q)
      r[ctr++] = val0;
    if(ctr < len && val1 < WEAVER_Q)
      r[ctr++] = val1;
  }

  return ctr;
}

static void gen_matrix_row_x4(polyvec *row,
                            const uint8_t seed[WEAVER_SYMBYTES],
                            unsigned int row_idx,
                            int transposed,
                            keccakx4_state *state)
{
  unsigned int ctr0, ctr1, ctr2, ctr3;
  unsigned int buflen, b;
  __attribute__((aligned(32)))
  uint8_t buf[4][(GEN_MATRIX_BUFLEN + 31) / 32 * 32];

  for(b = 0; b < 4; b++)
    memcpy(buf[b], seed, WEAVER_SYMBYTES);

  if(transposed) {
    buf[0][WEAVER_SYMBYTES + 0] = (uint8_t)row_idx;
    buf[0][WEAVER_SYMBYTES + 1] = 0;
    buf[1][WEAVER_SYMBYTES + 0] = (uint8_t)row_idx;
    buf[1][WEAVER_SYMBYTES + 1] = 1;
    buf[2][WEAVER_SYMBYTES + 0] = (uint8_t)row_idx;
    buf[2][WEAVER_SYMBYTES + 1] = 2;
    buf[3][WEAVER_SYMBYTES + 0] = (uint8_t)row_idx;
    buf[3][WEAVER_SYMBYTES + 1] = 3;
  } else {
    buf[0][WEAVER_SYMBYTES + 0] = 0;
    buf[0][WEAVER_SYMBYTES + 1] = (uint8_t)row_idx;
    buf[1][WEAVER_SYMBYTES + 0] = 1;
    buf[1][WEAVER_SYMBYTES + 1] = (uint8_t)row_idx;
    buf[2][WEAVER_SYMBYTES + 0] = 2;
    buf[2][WEAVER_SYMBYTES + 1] = (uint8_t)row_idx;
    buf[3][WEAVER_SYMBYTES + 0] = 3;
    buf[3][WEAVER_SYMBYTES + 1] = (uint8_t)row_idx;
  }

  shake128x4_absorb(state, buf[0], buf[1], buf[2], buf[3], WEAVER_SYMBYTES + 2);
  shake128x4_squeezeblocks(buf[0], buf[1], buf[2], buf[3], GEN_MATRIX_NBLOCKS, state);

  buflen = GEN_MATRIX_BUFLEN;
  ctr0 = rej_uniform_scalar(row->vec[0].coeffs, WEAVER_N, buf[0], buflen);
  ctr1 = rej_uniform_scalar(row->vec[1].coeffs, WEAVER_N, buf[1], buflen);
  ctr2 = rej_uniform_scalar(row->vec[2].coeffs, WEAVER_N, buf[2], buflen);
  ctr3 = rej_uniform_scalar(row->vec[3].coeffs, WEAVER_N, buf[3], buflen);

  while(ctr0 < WEAVER_N || ctr1 < WEAVER_N || ctr2 < WEAVER_N || ctr3 < WEAVER_N) {
    shake128x4_squeezeblocks(buf[0], buf[1], buf[2], buf[3], 1, state);

    ctr0 += rej_uniform_scalar(row->vec[0].coeffs + ctr0, WEAVER_N - ctr0, buf[0],
                               XOF_BLOCKBYTES);
    ctr1 += rej_uniform_scalar(row->vec[1].coeffs + ctr1, WEAVER_N - ctr1, buf[1],
                               XOF_BLOCKBYTES);
    ctr2 += rej_uniform_scalar(row->vec[2].coeffs + ctr2, WEAVER_N - ctr2, buf[2],
                               XOF_BLOCKBYTES);
    ctr3 += rej_uniform_scalar(row->vec[3].coeffs + ctr3, WEAVER_N - ctr3, buf[3],
                               XOF_BLOCKBYTES);
  }
}

void gen_matrix(polyvec *a, const uint8_t seed[WEAVER_SYMBYTES], int transposed)
{
  unsigned int i;
  __attribute__((aligned(32))) keccakx4_state state;

  for(i = 0; i < WEAVER_K; i++)
    gen_matrix_row_x4(&a[i], seed, i, transposed, &state);
}

#endif /* WEAVER_AVX_GEN_MATRIX512 && WEAVER_N==512 && WEAVER_K==4 */
