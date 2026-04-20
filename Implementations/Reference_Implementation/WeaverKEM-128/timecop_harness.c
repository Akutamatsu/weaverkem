/*
 * Valgrind / Timecop-style harness for WeaverKEM-128 reference code.
 * Marks the full secret key as secret (poison) then runs decapsulation.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drng.h"
#include "KEM_WeaverKEM-128.h"
#include "poison.h"

DRNG_ctx drng_algorithm;

static void init_deterministic_drbg(void)
{
  unsigned char nonce[64];
  unsigned char seed[64];
  DRNG_ctx drng_seed;
  int i;

  for (i = 0; i < 64 / 4; i++) {
    memcpy(nonce + 4 * i, "seed", 4);
  }
  init_random_number(&drng_seed, nonce, 64);
  get_random_number(&drng_seed, seed, 64 * 8);
  init_random_number(&drng_algorithm, seed, 64);
}

int main(void)
{
  unsigned long long pk_len, sk_len, ss_len, ct_len;
  unsigned char *pk, *sk, *ss, *ct;

  init_deterministic_drbg();

  pk_len = kem_get_pk_len_bytes();
  sk_len = kem_get_sk_len_bytes();
  ss_len = kem_get_ss_len_bytes();
  ct_len = kem_get_ct_len_bytes();

  pk = calloc((size_t)pk_len, 1);
  sk = calloc((size_t)sk_len, 1);
  ss = calloc((size_t)ss_len, 1);
  ct = calloc((size_t)ct_len, 1);
  if (!pk || !sk || !ss || !ct) {
    return 1;
  }

  if (kem_keygen(pk, &pk_len, sk, &sk_len) != 0) {
    return 2;
  }
  if (kem_enc(pk, pk_len, ss, &ss_len, ct, &ct_len) != 0) {
    return 3;
  }

  poison(sk, (size_t)sk_len);

  if (kem_dec(sk, sk_len, ct, ct_len, ss, &ss_len) != 0) {
    return 4;
  }

  free(pk);
  free(sk);
  free(ss);
  free(ct);
  return 0;
}
