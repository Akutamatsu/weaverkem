/*
 * Valgrind / Timecop-style harness for weaverkem/ref.
 * Marks the full secret key as secret (poison) then runs decapsulation.
 * Build with -DWEAVER_MODE=1|3|5 to match WEAVER-512 / 1024 / 2048.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "api.h"
#include "poison.h"

int main(void)
{
  unsigned char pk[CRYPTO_PUBLICKEYBYTES];
  unsigned char sk[CRYPTO_SECRETKEYBYTES];
  unsigned char ct[CRYPTO_CIPHERTEXTBYTES];
  unsigned char ss[CRYPTO_BYTES];
  unsigned char ss_dec[CRYPTO_BYTES];

  if (crypto_kem_keypair(pk, sk) != 0) {
    fprintf(stderr, "Keygen failed\n");
    return 1;
  }

  if (crypto_kem_enc(ct, ss, pk) != 0) {
    fprintf(stderr, "Encaps failed\n");
    return 2;
  }

  poison(sk, CRYPTO_SECRETKEYBYTES);

  if (crypto_kem_dec(ss_dec, ct, sk) != 0) {
    fprintf(stderr, "Decaps failed\n");
    return 3;
  }

  unpoison(sk, CRYPTO_SECRETKEYBYTES);
  if (memcmp(ss, ss_dec, CRYPTO_BYTES) != 0) {
    fprintf(stderr, "SS mismatch!\n");
    return 4;
  }

  printf("TIMECOP harness finished successfully\n");
  return 0;
}
