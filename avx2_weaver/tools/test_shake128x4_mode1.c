/* Compare scalar shake128 vs shake128x4 per lane (mode 1 seeds). */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "params.h"
#include "symmetric.h"
#include "fips202.h"
#include "fips202x4.h"

#if WEAVER_MODE != 1
#error "mode 1 only"
#endif

#define GEN_MATRIX_NBLOCKS \
  ((12 * WEAVER_N / 8 * (1 << 12) / WEAVER_Q + XOF_BLOCKBYTES) / XOF_BLOCKBYTES)

static void make_extseed(uint8_t ext[WEAVER_SYMBYTES + 2],
                         const uint8_t seed[WEAVER_SYMBYTES],
                         uint8_t x, uint8_t y)
{
  memcpy(ext, seed, WEAVER_SYMBYTES);
  ext[WEAVER_SYMBYTES + 0] = x;
  ext[WEAVER_SYMBYTES + 1] = y;
}

int main(void)
{
  uint8_t seed[WEAVER_SYMBYTES];
  uint8_t ext[4][WEAVER_SYMBYTES + 2];
  uint8_t out1[4][GEN_MATRIX_NBLOCKS * XOF_BLOCKBYTES + 32];
  uint8_t outx[4][GEN_MATRIX_NBLOCKS * XOF_BLOCKBYTES + 32];
  keccak_state st;
  keccakx4_state st4;
  unsigned j;

  for(unsigned i = 0; i < sizeof(seed); i++)
    seed[i] = (uint8_t)(0x5A ^ i);

  for(j = 0; j < 4; j++) {
    make_extseed(ext[j], seed, (uint8_t)j, 0);
    xof_absorb(&st, seed, (uint8_t)j, 0);
    xof_squeezeblocks(out1[j], GEN_MATRIX_NBLOCKS, &st);
  }

  /* Reuse one buffer for seeds + squeeze (like gen_poly_x4). */
  {
    uint8_t buf[4][GEN_MATRIX_NBLOCKS * XOF_BLOCKBYTES + 32];
    unsigned b;

    for(b = 0; b < 4; b++)
      make_extseed(buf[b], seed, (uint8_t)b, 0);
    shake128x4_absorb(&st4, buf[0], buf[1], buf[2], buf[3], WEAVER_SYMBYTES + 2);
    shake128x4_squeezeblocks(buf[0], buf[1], buf[2], buf[3], GEN_MATRIX_NBLOCKS, &st4);

    for(j = 0; j < 4; j++) {
      if(memcmp(out1[j], buf[j], GEN_MATRIX_NBLOCKS * XOF_BLOCKBYTES) != 0)
        printf("reuse lane %u MISMATCH\n", j);
      else
        printf("reuse lane %u OK\n", j);
    }
  }

  shake128x4_absorb(&st4, ext[0], ext[1], ext[2], ext[3], WEAVER_SYMBYTES + 2);
  shake128x4_squeezeblocks(outx[0], outx[1], outx[2], outx[3], GEN_MATRIX_NBLOCKS, &st4);

  for(j = 0; j < 4; j++) {
    if(memcmp(out1[j], outx[j], GEN_MATRIX_NBLOCKS * XOF_BLOCKBYTES) != 0) {
      unsigned k;
      printf("lane %u MISMATCH\n", j);
      for(k = 0; k < GEN_MATRIX_NBLOCKS * XOF_BLOCKBYTES; k++) {
        if(out1[j][k] != outx[j][k]) {
          printf("  first byte diff at %u: scalar=%u x4=%u\n", k, out1[j][k], outx[j][k]);
          break;
        }
      }
    } else {
      printf("lane %u OK\n", j);
    }
  }

  return 0;
}
