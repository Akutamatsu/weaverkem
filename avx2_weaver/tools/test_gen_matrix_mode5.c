/* Differential harness: dump gen_matrix / gen_matrix^T outputs for mode 5. */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "params.h"
#include "indcpa.h"

#if WEAVER_MODE != 5
#error "test_gen_matrix_mode5 requires WEAVER_MODE=5"
#endif

#ifndef NCASES_DEFAULT
#define NCASES_DEFAULT 50
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
  polyvec a[WEAVER_K], at[WEAVER_K];
  unsigned int t;
  unsigned int ncases = NCASES_DEFAULT;

  if(argc > 1)
    ncases = (unsigned int)strtoul(argv[1], NULL, 10);

  for(t = 0; t < ncases; t++) {
    unsigned int i;

    for(i = 0; i < sizeof(seed); i++)
      seed[i] = (uint8_t)(0xA5 ^ (t * 17 + i));
    gen_matrix(a, seed, 0);
    gen_matrix(at, seed, 1);
    if(fwrite(seed, 1, sizeof(seed), stdout) != sizeof(seed))
      return 1;
    write_matrix(stdout, a);
    write_matrix(stdout, at);
  }

  return 0;
}
