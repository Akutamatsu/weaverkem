/* Regression: enc_derand must not read past KYBER_KEM_DERAND_COINBYTES (64 for mode 5). */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "api.h"
#include "params.h"
#include "kem.h"

#if WEAVER_MODE != 5
#error "test_enc_derand_coins is for WEAVER-2048 (mode 5) only"
#endif

#if KYBER_KEM_DERAND_COINBYTES != KYBER_INDCPA_MSGBYTES
#error "mode 5 expected KYBER_KEM_DERAND_COINBYTES == KYBER_INDCPA_MSGBYTES"
#endif

int main(void)
{
  uint8_t kp_coins[2 * KYBER_SYMBYTES];
  uint8_t enc_coins[KYBER_KEM_DERAND_COINBYTES];
  uint8_t canary[KYBER_KEM_DERAND_COINBYTES];
  uint8_t pk[CRYPTO_PUBLICKEYBYTES];
  uint8_t sk[CRYPTO_SECRETKEYBYTES];
  uint8_t ct[CRYPTO_CIPHERTEXTBYTES];
  uint8_t ss[CRYPTO_BYTES];
  unsigned int i;

  for(i = 0; i < sizeof(kp_coins); i++)
    kp_coins[i] = (uint8_t)(0xA0 + (i & 0x0F));
  for(i = 0; i < sizeof(enc_coins); i++)
    enc_coins[i] = (uint8_t)(0xB0 + (i & 0x0F));
  memset(canary, 0x5A, sizeof(canary));

  crypto_kem_keypair_derand(pk, sk, kp_coins);
  crypto_kem_enc_derand(ct, ss, pk, enc_coins);

  if(memcmp(canary, "\x5a\x5a", 2) != 0) {
    fprintf(stderr, "FAIL: canary after enc_coins was overwritten\n");
    return 1;
  }

  for(i = 0; i < sizeof(canary); i++) {
    if(canary[i] != 0x5A) {
      fprintf(stderr, "FAIL: canary[%u]=0x%02x (OOB read past enc coins?)\n",
              i, canary[i]);
      return 1;
    }
  }

  printf("PASS: enc_derand with strict %u-byte coins buffer (mode 5)\n",
         (unsigned)KYBER_INDCPA_MSGBYTES);
  return 0;
}
