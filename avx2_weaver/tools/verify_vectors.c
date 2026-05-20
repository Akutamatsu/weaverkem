/* Re-run gen_vectors seeds and verify outputs match stored file. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include "api.h"
#include "params.h"
#include "kem.h"
#include "rng.h"

#define LINE_MAX 65536

static int hex2bin(const char *hex, uint8_t *out, size_t outlen)
{
  size_t i;
  for(i = 0; i < outlen; i++) {
  int hi, lo;
    hi = hex[2*i];
    lo = hex[2*i+1];
    if(!isxdigit(hi) || !isxdigit(lo)) return -1;
    hi = isdigit(hi) ? hi - '0' : toupper(hi) - 'A' + 10;
    lo = isdigit(lo) ? lo - '0' : toupper(lo) - 'A' + 10;
    out[i] = (uint8_t)((hi << 4) | lo);
  }
  return 0;
}

static int read_hex_line(FILE *f, const char *tag, uint8_t *buf, size_t len)
{
  char line[LINE_MAX];
  size_t taglen = strlen(tag);
  if(!fgets(line, sizeof(line), f)) return -1;
  if(strncmp(line, tag, taglen) != 0) return -1;
  return hex2bin(line + taglen, buf, len);
}

int main(int argc, char **argv)
{
  FILE *f;
  char line[256];
  unsigned int count = 0, idx = 0, errors = 0;
  uint8_t seed[32], pk[CRYPTO_PUBLICKEYBYTES], sk[CRYPTO_SECRETKEYBYTES];
  uint8_t ct[CRYPTO_CIPHERTEXTBYTES], ss[CRYPTO_BYTES], ss1[CRYPTO_BYTES];
  uint8_t pk2[CRYPTO_PUBLICKEYBYTES], sk2[CRYPTO_SECRETKEYBYTES];
  uint8_t ct2[CRYPTO_CIPHERTEXTBYTES], ss2[CRYPTO_BYTES];
  uint8_t coins[2 * KYBER_SYMBYTES];
  const char *path = (argc > 1) ? argv[1] : "test_vectors/weaver_avx_mode3.vec";

  f = fopen(path, "r");
  if(!f) { perror(path); return 1; }

  while(fgets(line, sizeof(line), f) && strncmp(line, "COUNT=", 6) != 0) {}
  sscanf(line, "COUNT=%u", &count);

  while(idx < count) {
    while(fgets(line, sizeof(line), f) && strncmp(line, "===", 3) != 0) {}
    if(read_hex_line(f, "SEED=", seed, 32) < 0) break;
    if(read_hex_line(f, "PK=", pk, CRYPTO_PUBLICKEYBYTES) < 0) break;
    if(read_hex_line(f, "SK=", sk, CRYPTO_SECRETKEYBYTES) < 0) break;
    if(read_hex_line(f, "CT=", ct, CRYPTO_CIPHERTEXTBYTES) < 0) break;
    if(read_hex_line(f, "SS=", ss, CRYPTO_BYTES) < 0) break;

    randombytes_init(seed, NULL, 256);
    randombytes(coins, sizeof(coins));
    crypto_kem_keypair_derand(pk2, sk2, coins);
    if(memcmp(pk, pk2, CRYPTO_PUBLICKEYBYTES) != 0 ||
       memcmp(sk, sk2, CRYPTO_SECRETKEYBYTES) != 0) {
      fprintf(stderr, "vector %u: keypair mismatch\n", idx);
      errors++;
    }

    randombytes(coins, KYBER_SYMBYTES);
    crypto_kem_enc_derand(ct2, ss2, pk, coins);
    if(memcmp(ct, ct2, CRYPTO_CIPHERTEXTBYTES) != 0 ||
       memcmp(ss, ss2, CRYPTO_BYTES) != 0) {
      fprintf(stderr, "vector %u: encaps mismatch\n", idx);
      errors++;
    }

    if(crypto_kem_dec(ss1, ct, sk) != 0 || memcmp(ss, ss1, CRYPTO_BYTES) != 0) {
      fprintf(stderr, "vector %u: decaps mismatch\n", idx);
      errors++;
    }

    idx++;
  }

  fclose(f);
  if(errors == 0)
    printf("OK: %u vectors verified for %s\n", idx, CRYPTO_ALGNAME);
  else
    printf("FAIL: %u errors in %u vectors\n", errors, idx);
  return errors ? 1 : 0;
}
