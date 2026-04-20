/*
 * Valgrind / Timecop-style harness for ConstantTimeRef.
 * Marks the full secret key as secret (poison) then runs decapsulation.
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

  // 1. Generate keypair
  if (crypto_kem_keypair(pk, sk) != 0) {
    fprintf(stderr, "Keygen failed\n");
    return 1;
  }

  // 2. Encapsulate
  if (crypto_kem_enc(ct, ss, pk) != 0) {
    fprintf(stderr, "Encaps failed\n");
    return 2;
  }

  // 3. Poison the secret key
  poison(sk, CRYPTO_SECRETKEYBYTES);

  // 4. Decapsulate (this is what TIMECOP monitors)
  if (crypto_kem_dec(ss_dec, ct, sk) != 0) {
    fprintf(stderr, "Decaps failed\n");
    return 3;
  }

  // 5. Check correctness (optional, but good for sanity)
  unpoison(sk, CRYPTO_SECRETKEYBYTES);
  if (memcmp(ss, ss_dec, CRYPTO_BYTES) != 0) {
    fprintf(stderr, "SS mismatch!\n");
    return 4;
  }

  printf("TIMECOP harness finished successfully\n");
  return 0;
}
