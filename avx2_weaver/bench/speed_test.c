#include <stdint.h>
#include <stdio.h>
#include "api.h"
#include "params.h"
#include "cpucycles.h"
#include "speed_print.h"
#include "randombytes.h"

#include "kem.h"

#ifndef NTESTS
#define NTESTS 1000
#endif

static uint64_t t[NTESTS];
int main(void)
{
  unsigned int i;
  uint8_t pk[CRYPTO_PUBLICKEYBYTES];
  uint8_t sk[CRYPTO_SECRETKEYBYTES];
  uint8_t ct[CRYPTO_CIPHERTEXTBYTES];
  uint8_t key[CRYPTO_BYTES];
  uint8_t kp_coins[2*KYBER_SYMBYTES];
  uint8_t enc_coins[KYBER_KEM_DERAND_COINBYTES]
    __attribute__((aligned(32)));

  randombytes(kp_coins, sizeof(kp_coins));
  randombytes(enc_coins, sizeof(enc_coins));

  printf("%s AVX2\n", CRYPTO_ALGNAME);

  for(i = 0; i < NTESTS; i++) {
    t[i] = cpucycles();
    crypto_kem_keypair_derand(pk, sk, kp_coins);
  }
  print_results("keypair_derand: ", t, NTESTS);

  /* enc_derand needs valid pk/sk and FO message coins (not keypair seed) */
  crypto_kem_keypair_derand(pk, sk, kp_coins);

  for(i = 0; i < NTESTS; i++) {
    t[i] = cpucycles();
    crypto_kem_enc_derand(ct, key, pk, enc_coins);
  }
  print_results("encaps_derand: ", t, NTESTS);

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
