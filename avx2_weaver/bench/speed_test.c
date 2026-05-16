#include <stdint.h>
#include <stdio.h>
#include "api.h"
#include "params.h"
#include "cpucycles.h"
#include "speed_print.h"
#include "rng.h"

#ifndef NTESTS
#define NTESTS 1000
#endif

static uint64_t t[NTESTS];
static uint8_t seed[KYBER_SYMBYTES];

int main(void)
{
  unsigned int i;
  uint8_t pk[CRYPTO_PUBLICKEYBYTES];
  uint8_t sk[CRYPTO_SECRETKEYBYTES];
  uint8_t ct[CRYPTO_CIPHERTEXTBYTES];
  uint8_t key[CRYPTO_BYTES];

  printf("%s AVX2\n", CRYPTO_ALGNAME);

  for(i = 0; i < NTESTS; i++) {
    t[i] = cpucycles();
    crypto_kem_keypair(pk, sk);
  }
  print_results("keypair: ", t, NTESTS);

  crypto_kem_keypair(pk, sk);

  for(i = 0; i < NTESTS; i++) {
    t[i] = cpucycles();
    crypto_kem_enc(ct, key, pk);
  }
  print_results("encaps: ", t, NTESTS);

  for(i = 0; i < NTESTS; i++) {
    t[i] = cpucycles();
    crypto_kem_dec(key, ct, sk);
  }
  print_results("decaps: ", t, NTESTS);

  return 0;
}
