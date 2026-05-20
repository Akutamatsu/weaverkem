/* Deterministic KEM derand replay: write pk|sk|ct|ss|ss_dec per case to stdout. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "api.h"
#include "params.h"
#include "kem.h"

#if WEAVER_REF_SIDE
/* ref kem_cca pulls in randombytes for non-derand entry points. */
void randombytes_init(unsigned char *entropy_input,
                      unsigned char *personalization_string,
                      int security_strength)
{
  (void)entropy_input;
  (void)personalization_string;
  (void)security_strength;
}

int randombytes(unsigned char *x, unsigned long long xlen)
{
  (void)x;
  (void)xlen;
  return -1;
}
#endif

#ifndef NCASES_DEFAULT
#if WEAVER_MODE == 5
#define NCASES_DEFAULT 5000
#else
#define NCASES_DEFAULT 10000
#endif
#endif

static uint32_t rng_state = 0xC0FFEE42;

static uint32_t xorshift32(void)
{
  uint32_t x = rng_state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  rng_state = x;
  return x;
}

int main(int argc, char **argv)
{
  unsigned int i;
  unsigned int ncases = NCASES_DEFAULT;
  uint8_t coins[2 * KYBER_SYMBYTES];
  uint8_t enc_coins[KYBER_INDCPA_MSGBYTES];
  uint8_t pk[8192], sk[8192], ct[8192], ss[64], ss_dec[64];
  size_t pklen = CRYPTO_PUBLICKEYBYTES;
  size_t sklen = CRYPTO_SECRETKEYBYTES;
  size_t ctlen = CRYPTO_CIPHERTEXTBYTES;
  size_t sslen = CRYPTO_BYTES;

  if(argc > 1)
    ncases = (unsigned int)strtoul(argv[1], NULL, 10);
  if(argc > 2)
    rng_state = (uint32_t)strtoul(argv[2], NULL, 16);

  for(i = 0; i < ncases; i++) {
    unsigned int j;
    for(j = 0; j < sizeof(coins); j++)
      coins[j] = (uint8_t)xorshift32();
    for(j = 0; j < sizeof(enc_coins); j++)
      enc_coins[j] = (uint8_t)xorshift32();

    crypto_kem_keypair_derand(pk, sk, coins);
    crypto_kem_enc_derand(ct, ss, pk, enc_coins);
    crypto_kem_dec(ss_dec, ct, sk);

    if(fwrite(pk, 1, pklen, stdout) != pklen ||
       fwrite(sk, 1, sklen, stdout) != sklen ||
       fwrite(ct, 1, ctlen, stdout) != ctlen ||
       fwrite(ss, 1, sslen, stdout) != sslen ||
       fwrite(ss_dec, 1, sslen, stdout) != sslen) {
      fprintf(stderr, "write failed at case %u\n", i);
      return 1;
    }
  }
  return 0;
}
