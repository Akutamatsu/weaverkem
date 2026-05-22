/* Encapsulation breakdown: full crypto_kem_enc (incl. randombytes in SEG_RNG_HASH).
 * Same 7-segment layout as ref/src/speed/test_encap_breakdown.c for cross-comparison.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "api.h"
#include "params.h"
#include "kem.h"
#include "indcpa.h"
#include "poly.h"
#include "polyvec.h"
#include "msgenc.h"
#include "symmetric.h"
#include "rng.h"
#include "cpucycles.h"

#ifdef PK_COMPRESS
#include "invq.h"
#if !defined(NO_INV_Q_LIFTING)
#define INV_Q_LIFTING
#endif
#endif

#ifndef NTESTS
#define NTESTS 1000
#endif

#define CYCLES_MAX_VALID  (500000000ULL)

enum {
  SEG_GEN_MATRIX = 0,
  SEG_CBD,
  SEG_NTT_BASEMUL,
  SEG_INVQ,
  SEG_MSGENC,
  SEG_COMPRESS_PACK,
  SEG_RNG_HASH,
  SEG_COUNT
};

static const char *seg_name[SEG_COUNT] = {
  "gen_matrix",
  "poly_getnoise (CBD)",
  "NTT + basemul",
  "Inv_q",
  "BCH MsgEncode",
  "Compress + pack",
  "randombytes + hash"
};

static uint64_t t_api_enc[NTESTS];
static uint64_t t_prof_wall[NTESTS];
static uint64_t t_seg[SEG_COUNT][NTESTS];

static inline int cycles_valid(uint64_t dt)
{
  return dt > 0 && dt < CYCLES_MAX_VALID;
}

static inline uint64_t cycles_span(uint64_t t0, uint64_t t1)
{
  if(t1 >= t0)
    return t1 - t0;
  return 0;
}

static int cmp_u64(const void *a, const void *b)
{
  uint64_t x = *(const uint64_t *)a;
  uint64_t y = *(const uint64_t *)b;
  if(x < y) return -1;
  if(x > y) return 1;
  return 0;
}

static uint64_t median_filtered(uint64_t *src, size_t n, size_t *n_used)
{
  size_t i, k = 0;
  uint64_t *tmp;

  tmp = (uint64_t *)malloc(n * sizeof(uint64_t));
  if(!tmp) {
    *n_used = 0;
    return 0;
  }

  for(i = 0; i < n; i++) {
    if(cycles_valid(src[i]))
      tmp[k++] = src[i];
  }
  *n_used = k;
  if(k == 0) {
    free(tmp);
    return 0;
  }

  qsort(tmp, k, sizeof(uint64_t), cmp_u64);
  uint64_t med = (k % 2) ? tmp[k / 2] : (tmp[k / 2 - 1] + tmp[k / 2]) / 2;
  free(tmp);
  return med;
}

static uint64_t average_filtered(uint64_t *src, size_t n)
{
  size_t i, k = 0;
  uint64_t acc = 0;

  for(i = 0; i < n; i++) {
    if(cycles_valid(src[i])) {
      acc += src[i];
      k++;
    }
  }
  return k ? acc / k : 0;
}

static void print_delta_line(const char *label, uint64_t *d, size_t n)
{
  size_t used;
  uint64_t med = median_filtered(d, n, &used);
  uint64_t avg = average_filtered(d, n);

  printf("%-36s median: %12" PRIu64 "  avg: %12" PRIu64,
         label, med, avg);
  if(used < n)
    printf("  (valid %zu/%zu)", used, n);
  printf("\n");
}

/*
 * Full encapsulation path aligned with crypto_kem_enc():
 *   randombytes(coins) + enc_derand body (hash_h/shake256 + indcpa_enc).
 * SEG_RNG_HASH includes randombytes(coins) as in the official API.
 */
static void kem_enc_full_profiled(uint8_t *ct, uint8_t *ss,
                                  const uint8_t *pk,
                                  uint64_t seg[SEG_COUNT])
{
  unsigned int i;
  uint64_t t0, t1;
  uint8_t coins[KYBER_INDCPA_MSGBYTES];
  uint8_t buf[KYBER_INDCPA_MSGBYTES + KYBER_SYMBYTES];
  uint8_t kr[KYBER_SSBYTES + KYBER_SYMBYTES];
  uint8_t seed[KYBER_SYMBYTES];
  uint8_t nonce = 0;
  polyvec sp = {0}, pkpv = {0}, at[KYBER_K] = {0}, b = {0};
  poly v = {0}, k = {0};

  memset(seg, 0, SEG_COUNT * sizeof(uint64_t));

  t0 = cpucycles();
  randombytes(coins, sizeof(coins));
  memcpy(buf, coins, KYBER_INDCPA_MSGBYTES);
  hash_h(buf + KYBER_INDCPA_MSGBYTES, pk, KYBER_PUBLICKEYBYTES);
  shake256(kr, sizeof(kr), buf, sizeof(buf));
  t1 = cpucycles();
  seg[SEG_RNG_HASH] = cycles_span(t0, t1);

#ifdef INV_Q_LIFTING
  t0 = cpucycles();
  polyvec_fromcompressed_pk(&pkpv, pk);
  memcpy(seed, pk + KYBER_PK_POLYVECBYTES, KYBER_SYMBYTES);
  polyvec_invq(&pkpv, kr + KYBER_SSBYTES, nonce++);
  polyvec_ntt(&pkpv);
  t1 = cpucycles();
  seg[SEG_INVQ] = cycles_span(t0, t1);
#else
  t0 = cpucycles();
  unpack_pk(&pkpv, seed, pk);
#ifdef PK_COMPRESS
  polyvec_ntt(&pkpv);
#endif
  t1 = cpucycles();
  seg[SEG_INVQ] = cycles_span(t0, t1);
#endif

  t0 = cpucycles();
  poly_frommsg(&k, buf);
  t1 = cpucycles();
  seg[SEG_MSGENC] = cycles_span(t0, t1);

  t0 = cpucycles();
  gen_matrix(at, seed, 1);
  t1 = cpucycles();
  seg[SEG_GEN_MATRIX] = cycles_span(t0, t1);

  t0 = cpucycles();
  for(i = 0; i < KYBER_K; i++)
    poly_getnoise_eta1(sp.vec + i, kr + KYBER_SSBYTES, nonce++);
  t1 = cpucycles();
  seg[SEG_CBD] = cycles_span(t0, t1);

  t0 = cpucycles();
  polyvec_ntt(&sp);
  for(i = 0; i < KYBER_K; i++)
    polyvec_basemul_acc_montgomery(&b.vec[i], &at[i], &sp);
  polyvec_basemul_acc_montgomery(&v, &pkpv, &sp);
  polyvec_invntt_tomont(&b);
  poly_invntt_tomont(&v);
  poly_add(&v, &v, &k);
  polyvec_reduce(&b);
  poly_reduce(&v);
  t1 = cpucycles();
  seg[SEG_NTT_BASEMUL] = cycles_span(t0, t1);

  t0 = cpucycles();
  polyvec_compress(ct, &b);
  poly_compress(ct + KYBER_POLYVECCOMPRESSEDBYTES, &v);
  t1 = cpucycles();
  seg[SEG_COMPRESS_PACK] = cycles_span(t0, t1);

  memcpy(ss, kr, KYBER_SSBYTES);
}

static void bench_all(void)
{
  unsigned int i, s;
  uint8_t pk[CRYPTO_PUBLICKEYBYTES];
  uint8_t sk[CRYPTO_SECRETKEYBYTES];
  uint8_t ct[CRYPTO_CIPHERTEXTBYTES];
  uint8_t ss[CRYPTO_BYTES];
  uint8_t ct2[CRYPTO_CIPHERTEXTBYTES];
  uint8_t ss2[CRYPTO_BYTES];
  uint64_t seg[SEG_COUNT];
  uint64_t t0, t1;

  crypto_kem_keypair(pk, sk);

  for(i = 0; i < NTESTS; i++) {
    t0 = cpucycles();
    kem_enc_full_profiled(ct, ss, pk, seg);
    t1 = cpucycles();
    t_prof_wall[i] = cycles_span(t0, t1);

    for(s = 0; s < SEG_COUNT; s++)
      t_seg[s][i] = seg[s];

    t0 = cpucycles();
    crypto_kem_enc(ct2, ss2, pk);
    t1 = cpucycles();
    t_api_enc[i] = cycles_span(t0, t1);
  }
}

int main(void)
{
  unsigned int s;
  size_t used;
  uint64_t med_sum;
  uint64_t sum_seg_med[SEG_COUNT];

  printf("=== AVX2 Weaver encapsulation breakdown (%s, NTESTS=%d) ===\n",
         CRYPTO_ALGNAME, NTESTS);
  printf("Scope: full crypto_kem_enc (randombytes inside \"randombytes + hash\")\n");
#ifdef INV_Q_LIFTING
  printf("PK path: Inv_q lifting\n");
#else
  printf("PK path: decompress + NTT\n");
#endif

  bench_all();

  med_sum = 0;
  for(s = 0; s < SEG_COUNT; s++) {
    sum_seg_med[s] = median_filtered(t_seg[s], NTESTS, &used);
    med_sum += sum_seg_med[s];
  }

  printf("\n--- Totals ---\n");
  print_delta_line("total enc (crypto_kem_enc API):", t_api_enc, NTESTS);
  print_delta_line("profiled wall (full encaps):", t_prof_wall, NTESTS);

  printf("\n--- Per-component (median cycles) ---\n");
  printf("%-28s %12s\n", "Component", "Median");
  for(s = 0; s < SEG_COUNT; s++)
    printf("%-28s %12" PRIu64 "\n", seg_name[s], sum_seg_med[s]);
  printf("%-28s %12" PRIu64 "\n", "SUM(7 components)", med_sum);

  return 0;
}
