/* Segment timing for kem enc/dec and indcpa_enc/dec (modes 1 and 3). */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../bench/cpucycles.h"
#include "api.h"
#include "indcpa.h"
#include "params.h"
#include "polyvec.h"
#include "poly.h"
#include "msgenc.h"
#include "symmetric.h"
#include "kem.h"
#include "verify.h"
#include "fips202.h"
#include "invq.h"
#include "rng.h"
#include "weaver_kp_profile.h"

#if (WEAVER_MODE != 1) && (WEAVER_MODE != 3)
#error "profile_encap_decaps requires WEAVER_MODE in {1,3}"
#endif

#ifndef WEAVER_PROFILE_KEYPAIR_DERAND
#error "profile_encap_decaps must be built with -DWEAVER_PROFILE_KEYPAIR_DERAND"
#endif

#define NRUNS 1000
#define NWARM 100

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

#define KP_NSLOTS 8
static int kp_store_idx = -1;
static uint64_t kp_oh;
static uint64_t kp_slot[NRUNS][KP_NSLOTS];
static uint64_t t_indcpa_full[NRUNS];

void weaver_kp_prof_begin_sample(int store_index)
{
  kp_store_idx = store_index;
}

void weaver_kp_prof_segment(int slot, uint64_t raw_delta_cycles)
{
  uint64_t d = raw_delta_cycles;
  if(d > kp_oh)
    d -= kp_oh;
  if(slot < 0 || slot >= KP_NSLOTS)
    return;
  if(kp_store_idx < 0 || kp_store_idx >= NRUNS)
    return;
  kp_slot[kp_store_idx][slot] = d;
}

static uint64_t median_col(int slot)
{
  uint64_t tmp[NRUNS];
  unsigned i;
  for(i = 0; i < NRUNS; i++)
    tmp[i] = kp_slot[i][slot];
  return median(tmp, NRUNS);
}

static void profile_indcpa_keypair_segments(uint64_t overhead)
{
  unsigned int run;
  uint8_t coins[WEAVER_SYMBYTES];
  uint8_t pk[WEAVER_INDCPA_PUBLICKEYBYTES];
  uint8_t sk[WEAVER_INDCPA_SECRETKEYBYTES];
  uint64_t t0, t1;

  kp_oh = overhead;
  memset(coins, 0x5a, sizeof(coins));

  for(run = 0; run < NWARM + NRUNS; run++) {
    weaver_kp_prof_begin_sample(run >= NWARM ? (int)(run - NWARM) : -1);
    t0 = cpucycles();
    indcpa_keypair_derand(pk, sk, coins);
    t1 = cpucycles();
    if(run >= NWARM)
      t_indcpa_full[run - NWARM] = t1 - t0 - overhead;
  }

  printf("\n=== indcpa_keypair_derand segments (median cycles, in-tree hooks) ===\n");
  printf("  [0] buf + hash_g:          %8llu\n", (unsigned long long)median_col(0));
  printf("  [1] gen_a (gen_matrix):    %8llu\n", (unsigned long long)median_col(1));
  printf("  [2] poly_getnoise x%d:     %8llu\n", WEAVER_K, (unsigned long long)median_col(2));
  printf("  [3] polyvec_ntt(sk):       %8llu\n", (unsigned long long)median_col(3));
  printf("  [4] matvec basemul x%d:    %8llu\n", WEAVER_K, (unsigned long long)median_col(4));
  printf("  [5] polyvec_invntt_tomont: %8llu\n", (unsigned long long)median_col(5));
  printf("  [6] polyvec_reduce:        %8llu\n", (unsigned long long)median_col(6));
  printf("  [7] pack_sk + pack_pk:     %8llu\n", (unsigned long long)median_col(7));
  {
    unsigned s;
    uint64_t sum = 0;
    for(s = 0; s < KP_NSLOTS; s++)
      sum += median_col(s);
    printf("  sum(segment medians):      %8llu\n", (unsigned long long)sum);
    printf("  indcpa_keypair_derand:     %8llu  (single rdtsc wrap, same runs)\n",
           (unsigned long long)median(t_indcpa_full, NRUNS));
    printf("  residual (sum - wrap):     %8llu\n",
           (unsigned long long)(median(t_indcpa_full, NRUNS) > sum
                                    ? median(t_indcpa_full, NRUNS) - sum
                                    : sum - median(t_indcpa_full, NRUNS)));
  }
}

static void profile_kem_keypair(uint64_t overhead)
{
  unsigned int run;
  uint64_t t_rng[NRUNS], t_indcpa[NRUNS], t_cca[NRUNS], t_kem[NRUNS];
  uint8_t coins[2 * WEAVER_SYMBYTES];
  uint8_t pk[CRYPTO_PUBLICKEYBYTES];
  uint8_t sk[CRYPTO_SECRETKEYBYTES];
  uint64_t t0, t1, t2;

  for(run = 0; run < NWARM + NRUNS; run++) {
    t0 = cpucycles();
    randombytes(coins, 2 * WEAVER_SYMBYTES);
    t1 = cpucycles();
    indcpa_keypair_derand(pk, sk, coins);
    t2 = cpucycles();
    if(run >= NWARM) {
      t_rng[run - NWARM] = t1 - t0 - overhead;
      t_indcpa[run - NWARM] = t2 - t1 - overhead;
    }

    randombytes(coins, 2 * WEAVER_SYMBYTES);
    indcpa_keypair_derand(pk, sk, coins);
    t0 = cpucycles();
    memcpy(sk + WEAVER_INDCPA_SECRETKEYBYTES, pk, WEAVER_PUBLICKEYBYTES);
    hash_h(sk + WEAVER_SECRETKEYBYTES - 2 * WEAVER_SYMBYTES, pk, WEAVER_PUBLICKEYBYTES);
    memcpy(sk + WEAVER_SECRETKEYBYTES - WEAVER_SYMBYTES, coins + WEAVER_SYMBYTES,
           WEAVER_SYMBYTES);
    t1 = cpucycles();
    if(run >= NWARM)
      t_cca[run - NWARM] = t1 - t0 - overhead;

    t0 = cpucycles();
    crypto_kem_keypair(pk, sk);
    t1 = cpucycles();
    if(run >= NWARM)
      t_kem[run - NWARM] = t1 - t0 - overhead;
  }

  printf("\n=== crypto_kem_keypair (median cycles) ===\n");
  printf("  randombytes(%d):           %8llu\n", 2 * WEAVER_SYMBYTES,
         (unsigned long long)median(t_rng, NRUNS));
  printf("  indcpa_keypair_derand:     %8llu\n", (unsigned long long)median(t_indcpa, NRUNS));
  printf("  rng + indcpa (medians):    %8llu\n",
         (unsigned long long)(median(t_rng, NRUNS) + median(t_indcpa, NRUNS)));
  printf("  CCA tail (H||copy z):      %8llu\n", (unsigned long long)median(t_cca, NRUNS));
  printf("  kem_keypair total:         %8llu  (bench_components kem_keypair)\n",
         (unsigned long long)median(t_kem, NRUNS));
  printf("  check: rng+indcpa+cca ~    %8llu\n",
         (unsigned long long)(median(t_rng, NRUNS) + median(t_indcpa, NRUNS) +
                              median(t_cca, NRUNS)));
}

static void profile_indcpa_enc(uint64_t overhead,
                               const uint8_t m[WEAVER_INDCPA_MSGBYTES],
                               const uint8_t pk[WEAVER_INDCPA_PUBLICKEYBYTES],
                               const uint8_t coins[WEAVER_SYMBYTES])
{
  unsigned int run, i;
  uint64_t t_parse[NRUNS], t_invq[NRUNS], t_pkntt[NRUNS], t_msg[NRUNS];
  uint64_t t_gen[NRUNS], t_noise[NRUNS], t_spntt[NRUNS], t_mat[NRUNS];
  uint64_t t_invntt[NRUNS], t_pack[NRUNS], t_total[NRUNS];
  uint8_t seed[WEAVER_SYMBYTES];
  uint8_t nonce;
  polyvec sp, pkpv, at[WEAVER_K], b;
  poly v, k;
  uint8_t c[WEAVER_INDCPA_BYTES];
  uint64_t t0, t1;

  for(run = 0; run < NWARM + NRUNS; run++) {
    nonce = 0;

    t0 = cpucycles();
    polyvec_fromcompressed_pk(&pkpv, pk);
    memcpy(seed, pk + WEAVER_PK_POLYVECBYTES, WEAVER_SYMBYTES);
    t1 = cpucycles();
    if(run >= NWARM) t_parse[run - NWARM] = t1 - t0 - overhead;

    t0 = cpucycles();
    polyvec_invq(&pkpv, coins, nonce++);
    t1 = cpucycles();
    if(run >= NWARM) t_invq[run - NWARM] = t1 - t0 - overhead;

    t0 = cpucycles();
    polyvec_ntt(&pkpv);
    t1 = cpucycles();
    if(run >= NWARM) t_pkntt[run - NWARM] = t1 - t0 - overhead;

    t0 = cpucycles();
    poly_frommsg(&k, m);
    t1 = cpucycles();
    if(run >= NWARM) t_msg[run - NWARM] = t1 - t0 - overhead;

    t0 = cpucycles();
    gen_matrix(at, seed, 1);
    t1 = cpucycles();
    if(run >= NWARM) t_gen[run - NWARM] = t1 - t0 - overhead;

    memset(&sp, 0, sizeof(sp));
    t0 = cpucycles();
    for(i = 0; i < WEAVER_K; i++)
      poly_getnoise_eta1(&sp.vec[i], coins, nonce++);
    t1 = cpucycles();
    if(run >= NWARM) t_noise[run - NWARM] = t1 - t0 - overhead;

    t0 = cpucycles();
    polyvec_ntt(&sp);
    t1 = cpucycles();
    if(run >= NWARM) t_spntt[run - NWARM] = t1 - t0 - overhead;

    memset(&b, 0, sizeof(b));
    memset(&v, 0, sizeof(v));
    t0 = cpucycles();
    for(i = 0; i < WEAVER_K; i++)
      polyvec_basemul_acc_montgomery(&b.vec[i], &at[i], &sp);
    polyvec_basemul_acc_montgomery(&v, &pkpv, &sp);
    t1 = cpucycles();
    if(run >= NWARM) t_mat[run - NWARM] = t1 - t0 - overhead;

    t0 = cpucycles();
    polyvec_invntt_tomont(&b);
    poly_invntt_tomont(&v);
    poly_add(&v, &v, &k);
    polyvec_reduce(&b);
    poly_reduce(&v);
    t1 = cpucycles();
    if(run >= NWARM) t_invntt[run - NWARM] = t1 - t0 - overhead;

    t0 = cpucycles();
    polyvec_compress(c, &b);
    poly_compress(c + WEAVER_POLYVECCOMPRESSEDBYTES, &v);
    t1 = cpucycles();
    if(run >= NWARM) t_pack[run - NWARM] = t1 - t0 - overhead;

    t0 = cpucycles();
    indcpa_enc(c, m, pk, coins);
    t1 = cpucycles();
    if(run >= NWARM) t_total[run - NWARM] = t1 - t0 - overhead;
  }

  printf("\n=== indcpa_enc segments (median cycles, %u runs) ===\n", NRUNS);
  printf("  parse_pk (fromcompressed): %8llu\n", (unsigned long long)median(t_parse, NRUNS));
  printf("  polyvec_invq:              %8llu  <-- Weaver-only\n", (unsigned long long)median(t_invq, NRUNS));
  printf("  polyvec_ntt(pk):           %8llu\n", (unsigned long long)median(t_pkntt, NRUNS));
  printf("  poly_frommsg (BCH encode): %8llu  <-- Weaver-only\n", (unsigned long long)median(t_msg, NRUNS));
  printf("  gen_matrix^T:              %8llu\n", (unsigned long long)median(t_gen, NRUNS));
  printf("  poly_getnoise x%d:         %8llu\n", WEAVER_K, (unsigned long long)median(t_noise, NRUNS));
  printf("  polyvec_ntt(sp):           %8llu\n", (unsigned long long)median(t_spntt, NRUNS));
  printf("  basemul_acc (b+v):         %8llu\n", (unsigned long long)median(t_mat, NRUNS));
  printf("  invntt+add+reduce:         %8llu\n", (unsigned long long)median(t_invntt, NRUNS));
  printf("  compress (c1+c2):          %8llu\n", (unsigned long long)median(t_pack, NRUNS));
  printf("  indcpa_enc total:          %8llu\n", (unsigned long long)median(t_total, NRUNS));
}

static void profile_indcpa_dec(uint64_t overhead,
                               const uint8_t c[WEAVER_INDCPA_BYTES],
                               const uint8_t sk[WEAVER_INDCPA_SECRETKEYBYTES])
{
  unsigned int run;
  uint64_t t_unpack[NRUNS], t_nttmul[NRUNS], t_msgdec[NRUNS], t_total[NRUNS];
  polyvec b, skpv;
  poly v, mp;
  uint8_t m[WEAVER_INDCPA_MSGBYTES];
  uint64_t t0, t1;

  for(run = 0; run < NWARM + NRUNS; run++) {
    t0 = cpucycles();
    polyvec_decompress(&b, c);
    poly_decompress(&v, c + WEAVER_POLYVECCOMPRESSEDBYTES);
    polyvec_frombytes(&skpv, sk);
    t1 = cpucycles();
    if(run >= NWARM) t_unpack[run - NWARM] = t1 - t0 - overhead;

    t0 = cpucycles();
    polyvec_ntt(&b);
    polyvec_basemul_acc_montgomery(&mp, &skpv, &b);
    poly_invntt_tomont(&mp);
    poly_sub(&mp, &v, &mp);
    poly_reduce(&mp);
    t1 = cpucycles();
    if(run >= NWARM) t_nttmul[run - NWARM] = t1 - t0 - overhead;

    t0 = cpucycles();
    poly_tomsg(m, &mp);
    t1 = cpucycles();
    if(run >= NWARM) t_msgdec[run - NWARM] = t1 - t0 - overhead;

    t0 = cpucycles();
    indcpa_dec(m, c, sk);
    t1 = cpucycles();
    if(run >= NWARM) t_total[run - NWARM] = t1 - t0 - overhead;
  }

  printf("\n=== indcpa_dec segments (median cycles) ===\n");
  printf("  decompress (c+sk):         %8llu\n", (unsigned long long)median(t_unpack, NRUNS));
  printf("  ntt+basemul+invntt+sub:    %8llu\n", (unsigned long long)median(t_nttmul, NRUNS));
  printf("  poly_tomsg (BCH decode):   %8llu  <-- Weaver-only\n", (unsigned long long)median(t_msgdec, NRUNS));
  printf("  indcpa_dec total:          %8llu\n", (unsigned long long)median(t_total, NRUNS));
}

static void profile_kem_enc(uint64_t overhead,
                            const uint8_t pk[CRYPTO_PUBLICKEYBYTES])
{
  unsigned int run;
  uint64_t t_rng[NRUNS], t_hashh[NRUNS], t_shake[NRUNS], t_pke[NRUNS];
  uint64_t t_derand[NRUNS], t_enc[NRUNS];
  uint8_t coins[WEAVER_INDCPA_MSGBYTES];
  uint8_t ct[CRYPTO_CIPHERTEXTBYTES];
  uint8_t ss[CRYPTO_BYTES];
  uint8_t buf[WEAVER_INDCPA_MSGBYTES + WEAVER_SYMBYTES];
  uint8_t kr[WEAVER_SSBYTES + WEAVER_SYMBYTES];
  uint64_t t0, t1;

  memset(coins, 0x11, sizeof(coins));
  memcpy(buf, coins, WEAVER_INDCPA_MSGBYTES);

  for(run = 0; run < NWARM + NRUNS; run++) {
    t0 = cpucycles();
    randombytes(coins, WEAVER_INDCPA_MSGBYTES);
    t1 = cpucycles();
    if(run >= NWARM) t_rng[run - NWARM] = t1 - t0 - overhead;

    memcpy(buf, coins, WEAVER_INDCPA_MSGBYTES);
    t0 = cpucycles();
    hash_h(buf + WEAVER_INDCPA_MSGBYTES, pk, WEAVER_PUBLICKEYBYTES);
    t1 = cpucycles();
    if(run >= NWARM) t_hashh[run - NWARM] = t1 - t0 - overhead;

    memcpy(buf, coins, WEAVER_INDCPA_MSGBYTES);
    hash_h(buf + WEAVER_INDCPA_MSGBYTES, pk, WEAVER_PUBLICKEYBYTES);
    t0 = cpucycles();
    shake256(kr, sizeof(kr), buf, sizeof(buf));
    t1 = cpucycles();
    if(run >= NWARM) t_shake[run - NWARM] = t1 - t0 - overhead;

    memcpy(buf, coins, WEAVER_INDCPA_MSGBYTES);
    hash_h(buf + WEAVER_INDCPA_MSGBYTES, pk, WEAVER_PUBLICKEYBYTES);
    shake256(kr, sizeof(kr), buf, sizeof(buf));
    t0 = cpucycles();
    indcpa_enc(ct, buf, pk, kr + WEAVER_SSBYTES);
    t1 = cpucycles();
    if(run >= NWARM) t_pke[run - NWARM] = t1 - t0 - overhead;

    t0 = cpucycles();
    crypto_kem_enc_derand(ct, ss, pk, coins);
    t1 = cpucycles();
    if(run >= NWARM) t_derand[run - NWARM] = t1 - t0 - overhead;

    t0 = cpucycles();
    crypto_kem_enc(ct, ss, pk);
    t1 = cpucycles();
    if(run >= NWARM) t_enc[run - NWARM] = t1 - t0 - overhead;
  }

  printf("\n=== crypto_kem_enc segments (median cycles) ===\n");
  printf("  randombytes(%d):           %8llu  (only in kem_enc)\n",
         WEAVER_INDCPA_MSGBYTES, (unsigned long long)median(t_rng, NRUNS));
  printf("  hash_h(pk):                %8llu\n", (unsigned long long)median(t_hashh, NRUNS));
  printf("  shake256(G):               %8llu  (ML-KEM uses hash_g)\n",
         (unsigned long long)median(t_shake, NRUNS));
  printf("  indcpa_enc:                %8llu\n", (unsigned long long)median(t_pke, NRUNS));
  printf("  kem_enc_derand total:      %8llu  (fair vs ML-KEM kyber_encaps_derand)\n",
         (unsigned long long)median(t_derand, NRUNS));
  printf("  kem_enc total:             %8llu  (bench_components kem_encaps)\n",
         (unsigned long long)median(t_enc, NRUNS));
  printf("  enc - enc_derand ~= rng:   %8llu\n",
         (unsigned long long)(median(t_enc, NRUNS) - median(t_derand, NRUNS)));
}

static void profile_kem_dec(uint64_t overhead,
                            const uint8_t ct[CRYPTO_CIPHERTEXTBYTES],
                            const uint8_t sk[CRYPTO_SECRETKEYBYTES])
{
  unsigned int run;
  uint64_t t_dec[NRUNS], t_shake[NRUNS], t_reenc[NRUNS], t_verify[NRUNS];
  uint64_t t_total[NRUNS];
  uint8_t buf[WEAVER_INDCPA_MSGBYTES + WEAVER_SYMBYTES];
  uint8_t kr[WEAVER_SSBYTES + WEAVER_SYMBYTES];
  uint8_t cmp[CRYPTO_CIPHERTEXTBYTES];
  uint8_t ss[CRYPTO_BYTES];
  const uint8_t *pk = sk + WEAVER_INDCPA_SECRETKEYBYTES;
  int fail;
  uint64_t t0, t1;

  for(run = 0; run < NWARM + NRUNS; run++) {
    t0 = cpucycles();
    indcpa_dec(buf, ct, sk);
    t1 = cpucycles();
    if(run >= NWARM) t_dec[run - NWARM] = t1 - t0 - overhead;

    indcpa_dec(buf, ct, sk);
    memcpy(buf + WEAVER_INDCPA_MSGBYTES,
           sk + WEAVER_SECRETKEYBYTES - 2 * WEAVER_SYMBYTES, WEAVER_SYMBYTES);
    t0 = cpucycles();
    shake256(kr, sizeof(kr), buf, sizeof(buf));
    t1 = cpucycles();
    if(run >= NWARM) t_shake[run - NWARM] = t1 - t0 - overhead;

    indcpa_dec(buf, ct, sk);
    memcpy(buf + WEAVER_INDCPA_MSGBYTES,
           sk + WEAVER_SECRETKEYBYTES - 2 * WEAVER_SYMBYTES, WEAVER_SYMBYTES);
    shake256(kr, sizeof(kr), buf, sizeof(buf));
    t0 = cpucycles();
    indcpa_enc(cmp, buf, pk, kr + WEAVER_SSBYTES);
    t1 = cpucycles();
    if(run >= NWARM) t_reenc[run - NWARM] = t1 - t0 - overhead;

    indcpa_dec(buf, ct, sk);
    memcpy(buf + WEAVER_INDCPA_MSGBYTES,
           sk + WEAVER_SECRETKEYBYTES - 2 * WEAVER_SYMBYTES, WEAVER_SYMBYTES);
    shake256(kr, sizeof(kr), buf, sizeof(buf));
    indcpa_enc(cmp, buf, pk, kr + WEAVER_SSBYTES);
    t0 = cpucycles();
    fail = verify(ct, cmp, WEAVER_CIPHERTEXTBYTES);
    rkprf(ss, sk + WEAVER_SECRETKEYBYTES - WEAVER_SYMBYTES, ct);
    cmov(ss, kr, WEAVER_SSBYTES, !fail);
    t1 = cpucycles();
    if(run >= NWARM) t_verify[run - NWARM] = t1 - t0 - overhead;

    t0 = cpucycles();
    crypto_kem_dec(ss, ct, sk);
    t1 = cpucycles();
    if(run >= NWARM) t_total[run - NWARM] = t1 - t0 - overhead;
  }

  printf("\n=== crypto_kem_dec segments (median cycles) ===\n");
  printf("  indcpa_dec:                %8llu\n", (unsigned long long)median(t_dec, NRUNS));
  printf("  shake256(G):               %8llu\n", (unsigned long long)median(t_shake, NRUNS));
  printf("  indcpa_enc (re-encrypt):   %8llu\n", (unsigned long long)median(t_reenc, NRUNS));
  printf("  verify+rkprf+cmov:         %8llu\n", (unsigned long long)median(t_verify, NRUNS));
  printf("  kem_dec total:             %8llu  (bench_components kem_decaps)\n",
         (unsigned long long)median(t_total, NRUNS));
  printf("  dec sum (dec+shake+reenc): %8llu\n",
         (unsigned long long)(median(t_dec, NRUNS) + median(t_shake, NRUNS) +
                              median(t_reenc, NRUNS)));
}

int main(void)
{
  uint64_t overhead = cpucycles_overhead();
  uint8_t pk[CRYPTO_PUBLICKEYBYTES];
  uint8_t sk[CRYPTO_SECRETKEYBYTES];
  uint8_t ct[CRYPTO_CIPHERTEXTBYTES];
  uint8_t ss[CRYPTO_BYTES];
  uint8_t coins[WEAVER_SYMBYTES];
  uint8_t m[WEAVER_INDCPA_MSGBYTES];

  printf("profile_encap_decaps WEAVER_MODE=%d (K=%d N=%d, NRUNS=%d)\n",
         WEAVER_MODE, WEAVER_K, WEAVER_N, NRUNS);

  profile_kem_keypair(overhead);
  profile_indcpa_keypair_segments(overhead);

  crypto_kem_keypair(pk, sk);
  randombytes(m, sizeof(m));
  randombytes(coins, WEAVER_SYMBYTES);
  crypto_kem_enc(ct, ss, pk);

  profile_indcpa_enc(overhead, m, pk, coins);
  profile_indcpa_dec(overhead, ct, sk);
  profile_kem_enc(overhead, pk);
  profile_kem_dec(overhead, ct, sk);
  return 0;
}
