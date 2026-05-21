/* Segment timing for kem_keypair / indcpa_keypair (mode 5, Weaver-1024). */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../bench/cpucycles.h"
#include "api.h"
#include "indcpa.h"
#include "params.h"
#include "polyvec.h"
#include "poly.h"
#include "symmetric.h"
#include "kem.h"
#include "rng.h"

#if WEAVER_MODE != 5
#error "profile_keypair_mode5 requires WEAVER_MODE=5"
#endif

#define NRUNS 500
#define NWARM 50

static uint64_t median(uint64_t *t, unsigned n)
{
  unsigned i, j;
  uint64_t tmp;
  for(i = 0; i < n; i++) {
    for(j = i + 1; j < n; j++) {
      if(t[j] < t[i]) {
        tmp = t[i];
        t[i] = t[j];
        t[j] = tmp;
      }
    }
  }
  return t[n / 2];
}

static void profile_indcpa_keypair(uint64_t overhead)
{
  unsigned int run;
  uint64_t t_hash[NRUNS], t_gen[NRUNS], t_noise[NRUNS], t_ntt[NRUNS];
  uint64_t t_basemul[NRUNS], t_invntt[NRUNS], t_reduce[NRUNS], t_total[NRUNS];
  uint8_t coins[KYBER_SYMBYTES];
  uint8_t pk[KYBER_INDCPA_PUBLICKEYBYTES];
  uint8_t sk[KYBER_INDCPA_SECRETKEYBYTES];
  uint8_t buf[2 * KYBER_SYMBYTES];
  const uint8_t *publicseed = buf;
  const uint8_t *noiseseed = buf + KYBER_SYMBYTES;
  uint8_t nonce;
  polyvec a[KYBER_K], pkpv, skpv;
  uint64_t t0, t1, sum, med;

  memset(coins, 0x42, sizeof(coins));

  for(run = 0; run < NWARM + NRUNS; run++) {
    memcpy(buf, coins, KYBER_SYMBYTES);
    buf[KYBER_SYMBYTES] = KYBER_K;

    t0 = cpucycles();
    hash_g(buf, buf, KYBER_SYMBYTES + 1);
    t1 = cpucycles();
    if(run >= NWARM) t_hash[run - NWARM] = t1 - t0 - overhead;

    t0 = cpucycles();
    gen_matrix(a, publicseed, 0);
    t1 = cpucycles();
    if(run >= NWARM) t_gen[run - NWARM] = t1 - t0 - overhead;

    memset(&skpv, 0, sizeof(skpv));
    nonce = 0;
    t0 = cpucycles();
    poly_getnoise_eta1(&skpv.vec[0], noiseseed, nonce++);
    poly_getnoise_eta1(&skpv.vec[1], noiseseed, nonce++);
    poly_getnoise_eta1(&skpv.vec[2], noiseseed, nonce++);
    poly_getnoise_eta1(&skpv.vec[3], noiseseed, nonce++);
    t1 = cpucycles();
    if(run >= NWARM) t_noise[run - NWARM] = t1 - t0 - overhead;

    t0 = cpucycles();
    polyvec_ntt(&skpv);
    t1 = cpucycles();
    if(run >= NWARM) t_ntt[run - NWARM] = t1 - t0 - overhead;

    memset(&pkpv, 0, sizeof(pkpv));
    t0 = cpucycles();
    polyvec_basemul_acc_montgomery(&pkpv.vec[0], &a[0], &skpv);
    polyvec_basemul_acc_montgomery(&pkpv.vec[1], &a[1], &skpv);
    polyvec_basemul_acc_montgomery(&pkpv.vec[2], &a[2], &skpv);
    polyvec_basemul_acc_montgomery(&pkpv.vec[3], &a[3], &skpv);
    t1 = cpucycles();
    if(run >= NWARM) t_basemul[run - NWARM] = t1 - t0 - overhead;

    t0 = cpucycles();
    polyvec_invntt_tomont(&pkpv);
    t1 = cpucycles();
    if(run >= NWARM) t_invntt[run - NWARM] = t1 - t0 - overhead;

    t0 = cpucycles();
    polyvec_reduce(&pkpv);
    t1 = cpucycles();
    if(run >= NWARM) t_reduce[run - NWARM] = t1 - t0 - overhead;

    t0 = cpucycles();
    indcpa_keypair_derand(pk, sk, coins);
    t1 = cpucycles();
    if(run >= NWARM) t_total[run - NWARM] = t1 - t0 - overhead;
  }

  printf("=== indcpa_keypair_derand segments (median cycles, %u runs) ===\n", NRUNS);
  med = median(t_hash, NRUNS);
  printf("  hash_g:              %8llu\n", (unsigned long long)med);
  sum = med;
  med = median(t_gen, NRUNS);
  printf("  gen_a (gen_matrix):  %8llu\n", (unsigned long long)med);
  sum += med;
  med = median(t_noise, NRUNS);
  printf("  poly_getnoise x4:    %8llu\n", (unsigned long long)med);
  sum += med;
  med = median(t_ntt, NRUNS);
  printf("  polyvec_ntt (sk):    %8llu\n", (unsigned long long)med);
  sum += med;
  med = median(t_basemul, NRUNS);
  printf("  basemul_acc x4:      %8llu  (4 row dot-products)\n", (unsigned long long)med);
  sum += med;
  med = median(t_invntt, NRUNS);
  printf("  polyvec_invntt:      %8llu\n", (unsigned long long)med);
  sum += med;
  med = median(t_reduce, NRUNS);
  printf("  polyvec_reduce:      %8llu\n", (unsigned long long)med);
  sum += med;
  med = median(t_total, NRUNS);
  printf("  indcpa_keypair total:%8llu\n", (unsigned long long)med);
  printf("  residual (pack etc): %8llu\n", (unsigned long long)(med > sum ? med - sum : 0));
}

static void profile_kem_keypair(uint64_t overhead)
{
  unsigned int run;
  uint64_t t_rng[NRUNS], t_indcpa[NRUNS], t_cca[NRUNS], t_total[NRUNS];
  uint8_t coins[2 * KYBER_SYMBYTES];
  uint8_t pk[CRYPTO_PUBLICKEYBYTES];
  uint8_t sk[CRYPTO_SECRETKEYBYTES];
  uint64_t t0, t1;

  for(run = 0; run < NWARM + NRUNS; run++) {
    t0 = cpucycles();
    randombytes(coins, 2 * KYBER_SYMBYTES);
    t1 = cpucycles();
    if(run >= NWARM) t_rng[run - NWARM] = t1 - t0 - overhead;

    randombytes(coins, 2 * KYBER_SYMBYTES);
    t0 = cpucycles();
    indcpa_keypair_derand(pk, sk, coins);
    t1 = cpucycles();
    if(run >= NWARM) t_indcpa[run - NWARM] = t1 - t0 - overhead;

    randombytes(coins, 2 * KYBER_SYMBYTES);
    indcpa_keypair_derand(pk, sk, coins);
    t0 = cpucycles();
    memcpy(sk + KYBER_INDCPA_SECRETKEYBYTES, pk, KYBER_PUBLICKEYBYTES);
    hash_h(sk + KYBER_SECRETKEYBYTES - 2 * KYBER_SYMBYTES, pk, KYBER_PUBLICKEYBYTES);
    memcpy(sk + KYBER_SECRETKEYBYTES - KYBER_SYMBYTES, coins + KYBER_SYMBYTES,
           KYBER_SYMBYTES);
    t1 = cpucycles();
    if(run >= NWARM) t_cca[run - NWARM] = t1 - t0 - overhead;

    t0 = cpucycles();
    crypto_kem_keypair(pk, sk);
    t1 = cpucycles();
    if(run >= NWARM) t_total[run - NWARM] = t1 - t0 - overhead;
  }

  printf("\n=== crypto_kem_keypair segments (median cycles) ===\n");
  printf("  randombytes(64):     %8llu\n", (unsigned long long)median(t_rng, NRUNS));
  printf("  indcpa_keypair:      %8llu\n", (unsigned long long)median(t_indcpa, NRUNS));
  printf("  CCA tail (H||copy):  %8llu\n", (unsigned long long)median(t_cca, NRUNS));
  printf("  kem_keypair total:   %8llu\n", (unsigned long long)median(t_total, NRUNS));
}

int main(void)
{
  uint64_t overhead = cpucycles_overhead();
  printf("profile_keypair_mode5 (KYBER_K=%d, KYBER_N=%d, PK_COMPRESS=1)\n",
         KYBER_K, KYBER_N);
  profile_indcpa_keypair(overhead);
  profile_kem_keypair(overhead);
  return 0;
}
