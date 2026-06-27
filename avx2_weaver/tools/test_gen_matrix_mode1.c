/* Differential harness: dump gen_matrix outputs for mode 1. */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "params.h"
#include "indcpa.h"

#if WEAVER_MODE != 1
#error "test_gen_matrix_mode1 requires WEAVER_MODE=1"
#endif

static void write_matrix(FILE *out, const polyvec a[WEAVER_K])
{
  unsigned int i, j, k;

  for(i = 0; i < WEAVER_K; i++)
    for(j = 0; j < WEAVER_K; j++)
      for(k = 0; k < WEAVER_N; k++)
        if(fwrite(&a[i].vec[j].coeffs[k], sizeof(int16_t), 1, out) != 1)
          return;
}

int main(int argc, char **argv)
{
  uint8_t seed[WEAVER_SYMBYTES];
  polyvec a[WEAVER_K];
  unsigned int t;
  unsigned int ncases = (argc > 1) ? (unsigned int)strtoul(argv[1], NULL, 10) : 20;

  for(t = 0; t < ncases; t++) {
    unsigned int i;

    for(i = 0; i < sizeof(seed); i++)
      seed[i] = (uint8_t)(0x5A ^ (t * 13 + i));
    gen_matrix(a, seed, 0);
    if(fwrite(seed, 1, sizeof(seed), stdout) != sizeof(seed))
      return 1;
    write_matrix(stdout, a);
  }

  return 0;
}
