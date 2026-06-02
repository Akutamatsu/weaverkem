/* Per-component cycle benchmarks (median / p10 / p90). */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "api.h"
#include "params.h"
#include "cpucycles.h"
#include "speed_print.h"
#include "rng.h"
#include "indcpa.h"
#include "polyvec.h"
#include "poly.h"
#include "msgenc.h"
#include "symmetric.h"

#ifndef NTESTS
#define NTESTS 1000
#endif

static uint64_t t[NTESTS];
static uint8_t seed[WEAVER_SYMBYTES];
static uint8_t msg[WEAVER_INDCPA_MSGBYTES];

static polyvec matrix[WEAVER_K];
static polyvec skpv, sp, at[WEAVER_K], b;
static poly v, k, tmp;
static uint8_t pkbuf[WEAVER_PK_POLYVECBYTES];
static uint8_t ctbuf[WEAVER_POLYVECCOMPRESSEDBYTES + WEAVER_POLYCOMPRESSEDBYTES];

static void bench_once(const char *label, void (*fn)(void))
{
  unsigned int i;

  for(i = 0; i < NTESTS; i++) {
    t[i] = cpucycles();
    fn();
  }
  print_results_stats(label, t, NTESTS);
}

static void do_gen_matrix(void)
{
  gen_matrix(matrix, seed, 0);
}

static void do_gen_matrix_t(void)
{
  gen_matrix(at, seed, 1);
}

static void do_polyvec_ntt(void)
{
  polyvec_ntt(&skpv);
}

static void do_polyvec_invntt(void)
{
  polyvec_invntt_tomont(&b);
}

static void do_polyvec_basemul_acc(void)
{
  polyvec_basemul_acc_montgomery(&v, &at[0], &sp);
}

static void do_poly_ntt(void)
{
  poly_ntt(&tmp);
}

static void do_poly_invntt(void)
{
  poly_invntt_tomont(&tmp);
}

static void do_poly_compress_pk(void)
{
#ifdef PK_COMPRESS
  polyvec_compress_pk(pkbuf, &skpv);
#else
  polyvec_tobytes(pkbuf, &skpv);
#endif
}

static void do_poly_decompress_pk(void)
{
#ifdef PK_COMPRESS
  polyvec_decompress_pk(&skpv, pkbuf);
#else
  polyvec_frombytes(&skpv, pkbuf);
#endif
}

static void do_polyvec_compress_ct(void)
{
  polyvec_compress(ctbuf, &b);
}

static void do_polyvec_decompress_ct(void)
{
  polyvec_decompress(&b, ctbuf);
}

static void do_poly_compress_v(void)
{
  poly_compress(ctbuf + WEAVER_POLYVECCOMPRESSEDBYTES, &v);
}

static void do_poly_decompress_v(void)
{
  poly_decompress(&v, ctbuf + WEAVER_POLYVECCOMPRESSEDBYTES);
}

static void do_poly_frommsg(void)
{
  poly_frommsg(&k, msg);
}

static void do_poly_tomsg(void)
{
  poly_tomsg(msg, &v);
}

static void bench_kem_keypair(void)
{
  unsigned int i;
  uint8_t pk[CRYPTO_PUBLICKEYBYTES];
  uint8_t sk[CRYPTO_SECRETKEYBYTES];

  for(i = 0; i < NTESTS; i++) {
    t[i] = cpucycles();
    crypto_kem_keypair(pk, sk);
  }
  print_results_stats("kem_keypair: ", t, NTESTS);
}

static void bench_kem_enc(void)
{
  unsigned int i;
  uint8_t pk[CRYPTO_PUBLICKEYBYTES];
  uint8_t sk[CRYPTO_SECRETKEYBYTES];
  uint8_t ct[CRYPTO_CIPHERTEXTBYTES];
  uint8_t ss[CRYPTO_BYTES];

  crypto_kem_keypair(pk, sk);
  for(i = 0; i < NTESTS; i++) {
    t[i] = cpucycles();
    crypto_kem_enc(ct, ss, pk);
  }
  print_results_stats("kem_encaps: ", t, NTESTS);
}

static void bench_kem_dec(void)
{
  unsigned int i;
  uint8_t pk[CRYPTO_PUBLICKEYBYTES];
  uint8_t sk[CRYPTO_SECRETKEYBYTES];
  uint8_t ct[CRYPTO_CIPHERTEXTBYTES];
  uint8_t ss[CRYPTO_BYTES];
  uint8_t out[CRYPTO_BYTES];

  crypto_kem_keypair(pk, sk);
  crypto_kem_enc(ct, ss, pk);
  for(i = 0; i < NTESTS; i++) {
    t[i] = cpucycles();
    crypto_kem_dec(out, ct, sk);
  }
  print_results_stats("kem_decaps: ", t, NTESTS);
}

int main(void)
{
  unsigned int i;

  printf("========== component benchmark: %s (N=%d K=%d, NTESTS=%d) ==========\n",
         CRYPTO_ALGNAME, WEAVER_N, WEAVER_K, NTESTS);
#if WEAVER_MATRIX_NEEDS_NTTUNPACK
  printf("layout: Kyber-AVX packed matrix + poly_nttunpack\n");
#else
  printf("layout: standard coefficient order (no nttunpack)\n");
#endif

  randombytes(seed, sizeof(seed));
  randombytes(msg, sizeof(msg));

  memset(&skpv, 0, sizeof(skpv));
  memset(&sp, 0, sizeof(sp));
  memset(&b, 0, sizeof(b));
  memset(&v, 0, sizeof(v));
  memset(&tmp, 0, sizeof(tmp));

  bench_once("gen_matrix: ", do_gen_matrix);
  bench_once("gen_matrix^T: ", do_gen_matrix_t);

  gen_matrix(at, seed, 1);
  for(i = 0; i < WEAVER_K; i++)
    poly_getnoise_eta1(&skpv.vec[i], seed, (uint8_t)i);
  for(i = 0; i < WEAVER_K; i++)
    poly_getnoise_eta1(&sp.vec[i], seed, (uint8_t)(WEAVER_K + i));

  bench_once("polyvec_ntt (K polys): ", do_polyvec_ntt);
  polyvec_ntt(&skpv);
  polyvec_ntt(&sp);

  bench_once("poly_ntt (1 poly): ", do_poly_ntt);
  bench_once("polyvec_basemul_acc: ", do_polyvec_basemul_acc);

  polyvec_basemul_acc_montgomery(&v, &at[0], &sp);
  bench_once("poly_invntt_tomont (1 poly): ", do_poly_invntt);
  bench_once("polyvec_invntt_tomont (K polys): ", do_polyvec_invntt);

#ifdef PK_COMPRESS
  bench_once("polyvec_compress_pk: ", do_poly_compress_pk);
  polyvec_compress_pk(pkbuf, &skpv);
  bench_once("polyvec_decompress_pk: ", do_poly_decompress_pk);
#endif

  polyvec_compress(ctbuf, &b);
  bench_once("polyvec_compress (c1): ", do_polyvec_compress_ct);
  bench_once("polyvec_decompress (c1): ", do_polyvec_decompress_ct);
  bench_once("poly_compress (c2/v): ", do_poly_compress_v);
  bench_once("poly_decompress (c2/v): ", do_poly_decompress_v);

  bench_once("poly_frommsg: ", do_poly_frommsg);
  poly_frommsg(&k, msg);
  bench_once("poly_tomsg: ", do_poly_tomsg);

  printf("--- full KEM ---\n");
  bench_kem_keypair();
  bench_kem_enc();
  bench_kem_dec();

  return 0;
}
