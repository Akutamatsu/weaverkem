/* Generate extended deterministic KEM test vectors (100 cases per build). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "api.h"
#include "params.h"
#include "kem.h"
#include "rng.h"

#ifndef NVECTORS
#define NVECTORS 100
#endif
#define SEEDLEN 32

static void hexprint(FILE *f, const char *label, const uint8_t *buf, size_t len)
{
  unsigned int i;
  fprintf(f, "%s", label);
  for(i = 0; i < len; i++)
    fprintf(f, "%02X", buf[i]);
  fprintf(f, "\n");
}

static void seed_expand(uint8_t out[SEEDLEN], unsigned idx)
{
  memset(out, 0, SEEDLEN);
  out[0] = (uint8_t)(idx);
  out[1] = (uint8_t)(idx >> 8);
  out[2] = (uint8_t)(idx >> 16);
  out[3] = (uint8_t)(idx >> 24);
  out[4] = 0x57; /* 'W' */
  out[5] = 0x56; /* 'V' */
}

int main(int argc, char **argv)
{
  FILE *out;
  unsigned int i;
  uint8_t seed[SEEDLEN];
  uint8_t coins[2 * WEAVER_SYMBYTES];
  uint8_t pk[CRYPTO_PUBLICKEYBYTES];
  uint8_t sk[CRYPTO_SECRETKEYBYTES];
  uint8_t ct[CRYPTO_CIPHERTEXTBYTES];
  uint8_t ss[CRYPTO_BYTES], ss1[CRYPTO_BYTES];
  const char *path = (argc > 1) ? argv[1] : "test_vectors/weaver_avx.vec";

  out = fopen(path, "w");
  if(!out) {
    perror(path);
    return 1;
  }

  fprintf(out, "WEAVER_MODE=%d\n", WEAVER_MODE);
  fprintf(out, "ALG=%s\n", CRYPTO_ALGNAME);
  fprintf(out, "COUNT=%d\n", NVECTORS);
  fprintf(out, "PK_LEN=%u\n", (unsigned)CRYPTO_PUBLICKEYBYTES);
  fprintf(out, "SK_LEN=%u\n", (unsigned)CRYPTO_SECRETKEYBYTES);
  fprintf(out, "CT_LEN=%u\n", (unsigned)CRYPTO_CIPHERTEXTBYTES);
  fprintf(out, "SS_LEN=%u\n", (unsigned)CRYPTO_BYTES);

  for(i = 0; i < NVECTORS; i++) {
    seed_expand(seed, i);
    randombytes_init(seed, NULL, 256);

    randombytes(coins, sizeof(coins));
    crypto_kem_keypair_derand(pk, sk, coins);

    randombytes(coins, WEAVER_SYMBYTES);
    crypto_kem_enc_derand(ct, ss, pk, coins);

    if(crypto_kem_dec(ss1, ct, sk) != 0 || memcmp(ss, ss1, CRYPTO_BYTES) != 0) {
      fprintf(stderr, "internal decap failed at vector %u\n", i);
      fclose(out);
      return 1;
    }

    fprintf(out, "=== %u ===\n", i);
    hexprint(out, "SEED=", seed, SEEDLEN);
    hexprint(out, "PK=", pk, CRYPTO_PUBLICKEYBYTES);
    hexprint(out, "SK=", sk, CRYPTO_SECRETKEYBYTES);
    hexprint(out, "CT=", ct, CRYPTO_CIPHERTEXTBYTES);
    hexprint(out, "SS=", ss, CRYPTO_BYTES);
  }

  fclose(out);
  printf("Wrote %d vectors to %s (%s)\n", NVECTORS, path, CRYPTO_ALGNAME);
  return 0;
}
