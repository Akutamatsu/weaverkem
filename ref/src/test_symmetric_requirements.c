#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "api.h"
#include "fips202.h"
#include "indcpa.h"
#include "kem.h"
#include "params.h"
#include "symmetric.h"

static int expect(int cond, const char *msg)
{
  if(!cond) {
    printf("FAIL: %s\n", msg);
    return 1;
  }
  return 0;
}

static void fill_seq(uint8_t *buf, size_t len, uint8_t start)
{
  size_t i;
  for(i = 0; i < len; i++)
    buf[i] = (uint8_t)(start + (uint8_t)i);
}

static int test_declared_lengths(void)
{
  int rc = 0;

#if WEAVER_MODE == 1
  rc |= expect(WEAVER_SYMBYTES == 32, "mode1 seed length must be 32 bytes");
  rc |= expect(WEAVER_HBYTES == 32, "mode1 H length must be 32 bytes");
#elif WEAVER_MODE == 3
  rc |= expect(WEAVER_SYMBYTES == 32, "mode3 seed length must be 32 bytes");
  rc |= expect(WEAVER_HBYTES == 64, "mode3 H length must be 64 bytes");
#elif WEAVER_MODE == 5
  rc |= expect(WEAVER_SYMBYTES == 64, "mode5 seed length must be 64 bytes");
  rc |= expect(WEAVER_HBYTES == 128, "mode5 H length must be 128 bytes");
#else
  rc |= expect(0, "unsupported mode");
#endif

  rc |= expect(WEAVER_GBYTES == WEAVER_SSBYTES + WEAVER_SYMBYTES,
               "G output length must equal ss||coins length");
  return rc;
}

static int test_hash_h_behavior(void)
{
  static const uint8_t msg[] = "WEAVER symmetric H test";
  uint8_t got[WEAVER_HBYTES];
  int rc = 0;

  memset(got, 0, sizeof(got));
  hash_h(got, msg, sizeof(msg) - 1);

#if WEAVER_HBYTES == 32
  {
    uint8_t ref[32];
    sha3_256(ref, msg, sizeof(msg) - 1);
    rc |= expect(memcmp(got, ref, 32) == 0, "H must match sha3_256 in mode1");
  }
#elif WEAVER_HBYTES == 64
  {
    uint8_t ref[64];
    sha3_512(ref, msg, sizeof(msg) - 1);
    rc |= expect(memcmp(got, ref, 64) == 0, "H must match sha3_512 in mode3");
  }
#elif WEAVER_HBYTES == 128
  {
    uint8_t ref[128];
    shake256(ref, sizeof(ref), msg, sizeof(msg) - 1);
    rc |= expect(memcmp(got, ref, sizeof(ref)) == 0, "H must match shake256-1024 in mode5");
  }
#endif

  return rc;
}

static int test_hash_g_behavior(void)
{
  static const uint8_t msg[] = "WEAVER symmetric G test";
  uint8_t got[WEAVER_GBYTES];
  uint8_t ref[WEAVER_GBYTES];

  memset(got, 0, sizeof(got));
  memset(ref, 0, sizeof(ref));
  hash_g(got, msg, sizeof(msg) - 1);
  shake256(ref, sizeof(ref), msg, sizeof(msg) - 1);

  return expect(memcmp(got, ref, sizeof(ref)) == 0,
                "G must be SHAKE256 with exact requested output length");
}

static int test_kem_roundtrip(void)
{
  uint8_t seed[2 * WEAVER_SYMBYTES];
  uint8_t fo_msg[WEAVER_KEM_DERAND_COINBYTES];
  uint8_t pk[CRYPTO_PUBLICKEYBYTES];
  uint8_t sk[CRYPTO_SECRETKEYBYTES];
  uint8_t ct[CRYPTO_CIPHERTEXTBYTES];
  uint8_t ss1[CRYPTO_BYTES];
  uint8_t ss2[CRYPTO_BYTES];
  int rc = 0;

  fill_seq(seed, sizeof(seed), 0x10);
  fill_seq(fo_msg, sizeof(fo_msg), 0x80);

  crypto_kem_keypair_derand(pk, sk, seed);
  crypto_kem_enc_derand(ct, ss1, pk, fo_msg);
  crypto_kem_dec(ss2, ct, sk);

  rc |= expect(memcmp(ss1, ss2, CRYPTO_BYTES) == 0,
               "deterministic encaps/decaps shared secrets must match");
  return rc;
}

static int test_hpk_storage(void)
{
  uint8_t seed[2 * WEAVER_SYMBYTES];
  uint8_t pk[CRYPTO_PUBLICKEYBYTES];
  uint8_t sk[CRYPTO_SECRETKEYBYTES];
  uint8_t hpk[WEAVER_HBYTES];

  fill_seq(seed, sizeof(seed), 0x21);
  memset(hpk, 0, sizeof(hpk));

  crypto_kem_keypair_derand(pk, sk, seed);
  hash_h(hpk, pk, CRYPTO_PUBLICKEYBYTES);

  if(expect(memcmp(sk + WEAVER_SK_HPK_OFFSET, hpk, WEAVER_HBYTES) == 0,
            "stored H(pk) must match recomputed H(pk)"))
    return 1;

  if(expect(memcmp(sk + WEAVER_SK_Z_OFFSET, seed + WEAVER_SYMBYTES, WEAVER_SYMBYTES) == 0,
            "stored z must match second half of derand seed"))
    return 1;

  return 0;
}

int main(void)
{
  int rc = 0;

  printf("Running symmetric requirement checks for %s\n", CRYPTO_ALGNAME);
  printf("SYMBYTES=%d HBYTES=%d SSBYTES=%d GBYTES=%d\n",
         WEAVER_SYMBYTES, WEAVER_HBYTES, WEAVER_SSBYTES, WEAVER_GBYTES);

  rc |= test_declared_lengths();
  rc |= test_hash_h_behavior();
  rc |= test_hash_g_behavior();
  rc |= test_hpk_storage();
  rc |= test_kem_roundtrip();

  if(rc != 0)
    return 1;

  printf("PASS symmetric requirement checks\n");
  return 0;
}
