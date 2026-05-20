/* Replay ICCS KAT_KEM_WeaverKEM-*.txt with AVX2 implementation and compare. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "drng.h"
#include "api.h"
#include "params.h"
#include "kem.h"

DRNG_ctx drng_algorithm;

int randombytes(unsigned char *x, unsigned long long xlen)
{
  return get_random_number(&drng_algorithm, x, xlen * 8);
}

#define LINE_MAX 65536

static int parse_hex(const char *hex, unsigned char *out, size_t len)
{
  size_t i;
  for(i = 0; i < len; i++) {
    int hi = hex[2*i], lo = hex[2*i+1];
    if(!isxdigit(hi) || !isxdigit(lo)) return -1;
    hi = isdigit(hi) ? hi - '0' : toupper(hi) - 'A' + 10;
    lo = isdigit(lo) ? lo - '0' : toupper(lo) - 'A' + 10;
    out[i] = (unsigned char)((hi << 4) | lo);
  }
  return 0;
}

static int cmp_hex(const char *tag, const unsigned char *got, size_t len,
                   const char *expect_hex, int verbose)
{
  unsigned char exp[8192];
  size_t off;
  if(parse_hex(expect_hex, exp, len) != 0) {
    fprintf(stderr, "%s: bad hex in KAT file\n", tag);
    return 1;
  }
  if(memcmp(got, exp, len) == 0)
    return 0;
  fprintf(stderr, "%s: MISMATCH", tag);
  if(verbose) {
    for(off = 0; off < len; off++)
      if(got[off] != exp[off]) break;
    fprintf(stderr, " first_diff@%zu got=0x%02x exp=0x%02x", off, got[off], exp[off]);
  }
  fprintf(stderr, "\n");
  return 1;
}

int main(int argc, char **argv)
{
  FILE *f;
  char line[LINE_MAX];
  int count = -1, errors = 0;
  int trace = (argc > 2 && strcmp(argv[2], "--trace") == 0);
  int case_pk = 0, case_sk = 0, case_ct = 0, case_ss = 0;
  unsigned char seed[64];
  unsigned char pk[CRYPTO_PUBLICKEYBYTES], sk[CRYPTO_SECRETKEYBYTES];
  unsigned char ct[CRYPTO_CIPHERTEXTBYTES], ss[CRYPTO_BYTES], ss1[CRYPTO_BYTES];
  DRNG_ctx drng_seed;
  const char *path = (argc > 1) ? argv[1] : "../Test_Vectors/KAT_KEM_WeaverKEM-256.txt";

  f = fopen(path, "r");
  if(!f) { perror(path); return 1; }

  printf("ICCS KAT verify: %s (%s)\n", path, CRYPTO_ALGNAME);

  while(fgets(line, sizeof(line), f)) {
    if(strncmp(line, "Count = ", 8) == 0) {
      count++;
      continue;
    }
    if(count < 0) continue;

    if(strncmp(line, "Seed = ", 7) == 0) {
      if(parse_hex(line + 7, seed, 64) != 0) { errors++; break; }
      init_random_number(&drng_seed, seed, 64);
      continue;
    }
    if(strncmp(line, "PK = ", 5) == 0) {
      case_pk = case_sk = case_ct = case_ss = 0;
      init_random_number(&drng_algorithm, seed, 64);
      crypto_kem_keypair(pk, sk);
      case_pk = cmp_hex("PK", pk, CRYPTO_PUBLICKEYBYTES, line + 5, trace);
      errors += case_pk;
      continue;
    }
    if(strncmp(line, "SK = ", 5) == 0) {
      case_sk = cmp_hex("SK", sk, CRYPTO_SECRETKEYBYTES, line + 5, trace);
      errors += case_sk;
      continue;
    }
    if(strncmp(line, "CT = ", 5) == 0) {
      crypto_kem_enc(ct, ss, pk);
      case_ct = cmp_hex("CT", ct, CRYPTO_CIPHERTEXTBYTES, line + 5, trace);
      errors += case_ct;
      continue;
    }
    if(strncmp(line, "SS = ", 5) == 0) {
      case_ss = cmp_hex("SS", ss, CRYPTO_BYTES, line + 5, trace);
      errors += case_ss;
      crypto_kem_dec(ss1, ct, sk);
      if(memcmp(ss, ss1, CRYPTO_BYTES) != 0) {
        fprintf(stderr, "SS: decaps mismatch at count %d\n", count);
        errors++;
      }
      if(trace && (case_pk || case_sk || case_ct || case_ss)) {
        fprintf(stderr, "KAT case #%d: pk=%s sk=%s ct=%s ss=%s\n",
                count,
                case_pk ? "FAIL" : "PASS",
                case_sk ? "FAIL" : "PASS",
                case_ct ? "FAIL" : "PASS",
                case_ss ? "FAIL" : "PASS");
      }
    }
  }

  fclose(f);
  if(errors == 0)
    printf("PASS: ICCS KAT vectors match AVX2 implementation.\n");
  else
    printf("FAIL: %d error(s) in ICCS KAT replay.\n", errors);
  return errors ? 1 : 0;
}
