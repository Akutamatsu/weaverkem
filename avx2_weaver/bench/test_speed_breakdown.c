/* Weaver AVX2 KEM breakdown: keypair, encapsulation, and decapsulation. */
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
#include "fips202.h"
#include "rng.h"
#include "verify.h"
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
#define MAX_SEG_EXPORT    32
#define MAX_REPORTS       3

typedef struct {
  uint64_t med;
  uint64_t avg;
  size_t valid;
} stat_val;

typedef struct {
  const char *title;
  const char *scope;
  stat_val api;
  stat_val wall;
  unsigned int nseg;
  uint64_t seg_med[MAX_SEG_EXPORT];
  const char * const *names;
} section_report;

static section_report g_reports[MAX_REPORTS];
static unsigned int g_nreports;
static char g_csv_path[512];

/* indcpa_enc body (also used for dec re-encryption) */
enum {
  E_SEG_INVQ = 0,
  E_SEG_MSGENC,
  E_SEG_GEN_MATRIX,
  E_SEG_CBD,
  E_SEG_NTT_BASEMUL,
  E_SEG_COMPRESS_PACK,
  E_SEG_BODY_COUNT
};

/* full encapsulation (+ RNG/hash wrapper) */
enum {
  ENC_SEG_RNG_HASH = 0,
  ENC_SEG_COUNT
};
#define ENC_SEG_TOTAL (ENC_SEG_COUNT + E_SEG_BODY_COUNT)

static const char *enc_seg_name[ENC_SEG_TOTAL] = {
  "randombytes + hash_h + shake256",
  "Inv_q (+ parse pk)",
  "BCH MsgEncode",
  "gen_matrix",
  "poly_getnoise (CBD)",
  "NTT + basemul",
  "compress + pack"
};

/* indcpa_keypair body */
enum {
  KP_SEG_HASH_SEED = 0,
  KP_SEG_GEN_MATRIX,
  KP_SEG_CBD,
  KP_SEG_NTT_SK,
  KP_SEG_NTT_BASEMUL,
  KP_SEG_INVNTT_REDUCE,
  KP_SEG_PACK,
  KP_SEG_BODY_COUNT
};

/* full keypair (+ KEM sk assembly + RNG) */
enum {
  KP_SEG_KEM_ASSEMBLE = 0,
  KP_SEG_RNG,
  KP_SEG_WRAP_COUNT
};
#define KP_SEG_TOTAL (KP_SEG_WRAP_COUNT + KP_SEG_BODY_COUNT)

static const char *kp_seg_name[KP_SEG_TOTAL] = {
  "memcpy pk + hash_h + z",
  "randombytes",
  "hash_g (seed expand)",
  "gen_matrix",
  "poly_getnoise (CBD)",
  "polyvec_ntt (sk)",
  "NTT + basemul (pk)",
  "invntt + reduce (pk)",
  "pack (pk, sk)"
};

/* decapsulation */
enum {
  D_SEG_DEC_UNPACK = 0,
  D_SEG_DEC_NTT_BASEMUL,
  D_SEG_DEC_TOMSG,
  D_SEG_SHAKE256,
  D_SEG_VERIFY,
  D_SEG_RKPRF,
  D_SEG_CMOV,
  D_SEG_KEM_COUNT
};
#define D_SEG_TOTAL (D_SEG_KEM_COUNT + E_SEG_BODY_COUNT)

static const char *dec_seg_name[D_SEG_TOTAL] = {
  "decompress (ct + sk)",
  "dec NTT + basemul",
  "poly_tomsg (BCH decode)",
  "shake256 (reconstruct kr)",
  "verify (ct vs re-enc)",
  "rkprf (reject key)",
  "cmov (select ss)",
  "re-enc: Inv_q (+ parse pk)",
  "re-enc: BCH MsgEncode",
  "re-enc: gen_matrix",
  "re-enc: poly_getnoise (CBD)",
  "re-enc: NTT + basemul",
  "re-enc: compress + pack"
};

static uint64_t t_api_enc[NTESTS], t_wall_enc[NTESTS], t_enc[ENC_SEG_TOTAL][NTESTS];
static uint64_t t_api_kp[NTESTS], t_wall_kp[NTESTS], t_kp[KP_SEG_TOTAL][NTESTS];
static uint64_t t_api_dec[NTESTS], t_wall_dec[NTESTS], t_dec[D_SEG_TOTAL][NTESTS];

static __attribute__((aligned(32))) polyvec prof_sp, prof_pkpv, prof_bp;
static __attribute__((aligned(32))) polyvec prof_at[4];
static __attribute__((aligned(32))) poly prof_v, prof_k;
static __attribute__((aligned(32))) uint8_t prof_buf[KYBER_INDCPA_MSGBYTES + KYBER_SYMBYTES];
static __attribute__((aligned(32))) uint8_t prof_kr[KYBER_SSBYTES + KYBER_SYMBYTES];
static __attribute__((aligned(32))) uint8_t prof_seed[KYBER_SYMBYTES];

static __attribute__((aligned(32))) polyvec kp_a[4], kp_pkpv, kp_skpv;
static __attribute__((aligned(32))) polyvec dec_b, dec_skpv;
static __attribute__((aligned(32))) poly dec_v, dec_mp;

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

  printf("%-40s median: %12" PRIu64 "  avg: %12" PRIu64,
         label, med, avg);
  if(used < n)
    printf("  (valid %zu/%zu)", used, n);
  printf("\n");
}

static void collect_stat(stat_val *out, uint64_t *d, size_t n)
{
  out->med = median_filtered(d, n, &out->valid);
  out->avg = average_filtered(d, n);
}

static void print_section(const char *title, const char *scope,
                        const char * const *names, unsigned int nseg,
                        uint64_t t_api[NTESTS], uint64_t t_wall[NTESTS],
                        uint64_t t_seg[][NTESTS])
{
  unsigned int s;
  size_t used;
  uint64_t med_sum = 0;
  section_report *rep;

  if(nseg > MAX_SEG_EXPORT || g_nreports >= MAX_REPORTS)
    return;

  rep = &g_reports[g_nreports++];
  rep->title = title;
  rep->scope = scope;
  rep->names = names;
  rep->nseg = nseg;

  collect_stat(&rep->api, t_api, NTESTS);
  collect_stat(&rep->wall, t_wall, NTESTS);

  printf("\n========================================\n");
  printf("=== %s (%s, NTESTS=%d) ===\n", title, CRYPTO_ALGNAME, NTESTS);
  printf("Scope: %s\n", scope);

  for(s = 0; s < nseg; s++) {
    rep->seg_med[s] = median_filtered(t_seg[s], NTESTS, &used);
    med_sum += rep->seg_med[s];
  }

  printf("\n--- Totals ---\n");
  print_delta_line("API call:", t_api, NTESTS);
  print_delta_line("profiled wall:", t_wall, NTESTS);

  printf("\n--- Per-component (median cycles) ---\n");
  printf("%-32s %12s\n", "Component", "Median");
  for(s = 0; s < nseg; s++)
    printf("%-32s %12" PRIu64 "\n", names[s], rep->seg_med[s]);
  printf("%-32s %12" PRIu64 "\n", "SUM(components)", med_sum);
}

static void csv_escape(const char *src, char *dst, size_t dstlen)
{
  size_t i, j;
  int need_quote = 0;

  if(!src || dstlen == 0)
    return;

  for(i = 0; src[i]; i++) {
    if(src[i] == ',' || src[i] == '"' || src[i] == '\n' || src[i] == '\r') {
      need_quote = 1;
      break;
    }
  }

  if(!need_quote) {
    strncpy(dst, src, dstlen - 1);
    dst[dstlen - 1] = '\0';
    return;
  }

  j = 0;
  if(j + 1 < dstlen)
    dst[j++] = '"';
  for(i = 0; src[i] && j + 2 < dstlen; i++) {
    if(src[i] == '"') {
      if(j + 2 < dstlen) {
        dst[j++] = '"';
        dst[j++] = '"';
      }
    } else {
      dst[j++] = src[i];
    }
  }
  if(j + 1 < dstlen)
    dst[j++] = '"';
  dst[j] = '\0';
}

static void csv_write_row(FILE *fp, const char *c1, const char *c2, const char *c3,
                          const char *c4, const char *c5,
                          uint64_t med, uint64_t avg, size_t valid, size_t total)
{
  char e1[256], e2[256], e3[256], e4[256], e5[256];

  csv_escape(c1, e1, sizeof(e1));
  csv_escape(c2, e2, sizeof(e2));
  csv_escape(c3, e3, sizeof(e3));
  csv_escape(c4, e4, sizeof(e4));
  csv_escape(c5, e5, sizeof(e5));

  fprintf(fp, "%s,%s,%s,%s,%s,%" PRIu64 ",%" PRIu64 ",%zu,%zu\n",
          e1, e2, e3, e4, e5, med, avg, valid, total);
}

static int export_breakdown_csv(const char *path)
{
  FILE *fp;
  unsigned int r, s;
  uint64_t sum_seg;
  const char *pk_path;

  fp = fopen(path, "wb");
  if(!fp) {
    fprintf(stderr, "Cannot write CSV: %s\n", path);
    return -1;
  }

  /* UTF-8 BOM so Excel opens encoding correctly */
  fputs("\xEF\xBB\xBF", fp);

  fputs("Algorithm,Operation,Scope,RowType,Component,Median_cycles,Avg_cycles,"
        "Valid_samples,Total_samples\n", fp);

#ifdef INV_Q_LIFTING
  pk_path = "Inv_q lifting";
#else
  pk_path = "unpack_pk (+ NTT if PK_COMPRESS)";
#endif

  csv_write_row(fp, CRYPTO_ALGNAME, "META", "", "NTESTS", "",
                NTESTS, NTESTS, NTESTS, NTESTS);
  csv_write_row(fp, CRYPTO_ALGNAME, "META", "", "PK_path", pk_path,
                0, 0, 0, NTESTS);

  for(r = 0; r < g_nreports; r++) {
    const section_report *rep = &g_reports[r];

    csv_write_row(fp, CRYPTO_ALGNAME, rep->title, rep->scope, "API_total",
                  "crypto API", rep->api.med, rep->api.avg,
                  rep->api.valid, NTESTS);
    csv_write_row(fp, CRYPTO_ALGNAME, rep->title, rep->scope, "profiled_wall",
                  "profiled wall", rep->wall.med, rep->wall.avg,
                  rep->wall.valid, NTESTS);

    sum_seg = 0;
    for(s = 0; s < rep->nseg; s++) {
      sum_seg += rep->seg_med[s];
      csv_write_row(fp, CRYPTO_ALGNAME, rep->title, rep->scope, "component",
                    rep->names[s], rep->seg_med[s], rep->seg_med[s],
                    rep->api.valid, NTESTS);
    }

    csv_write_row(fp, CRYPTO_ALGNAME, rep->title, rep->scope, "sum_components",
                  "SUM(components)", sum_seg, sum_seg, rep->api.valid, NTESTS);
  }

  fclose(fp);
  return 0;
}

static void default_csv_path(char *out, size_t outlen)
{
  snprintf(out, outlen, "speed_breakdown_%s.csv", CRYPTO_ALGNAME);
}

static int parse_output_path(int argc, char **argv)
{
  int i;

  if(argc >= 3 && strcmp(argv[1], "-o") == 0) {
    strncpy(g_csv_path, argv[2], sizeof(g_csv_path) - 1);
    g_csv_path[sizeof(g_csv_path) - 1] = '\0';
    return 0;
  }

  {
    const char *env = getenv("BREAKDOWN_CSV");
    if(env && env[0]) {
      strncpy(g_csv_path, env, sizeof(g_csv_path) - 1);
      g_csv_path[sizeof(g_csv_path) - 1] = '\0';
      return 0;
    }
  }

  for(i = 1; i < argc; i++) {
    if(strncmp(argv[i], "-o", 2) == 0) {
      const char *p = argv[i] + 2;
      if(*p == '=') {
        strncpy(g_csv_path, p + 1, sizeof(g_csv_path) - 1);
        g_csv_path[sizeof(g_csv_path) - 1] = '\0';
        return 0;
      }
    }
  }

  default_csv_path(g_csv_path, sizeof(g_csv_path));
  return 0;
}

static void keypair_noise(polyvec *skpv, const uint8_t noiseseed[KYBER_SYMBYTES])
{
  unsigned int i;
  uint8_t nonce = 0;

#if (KYBER_K == 2)
  poly_getnoise_eta1(&skpv->vec[0], noiseseed, nonce++);
  poly_getnoise_eta1(&skpv->vec[1], noiseseed, nonce++);
#elif (KYBER_K == 4)
  poly_getnoise_eta1(&skpv->vec[0], noiseseed, nonce++);
  poly_getnoise_eta1(&skpv->vec[1], noiseseed, nonce++);
  poly_getnoise_eta1(&skpv->vec[2], noiseseed, nonce++);
  poly_getnoise_eta1(&skpv->vec[3], noiseseed, nonce++);
#else
  for(i = 0; i < KYBER_K; i++)
    poly_getnoise_eta1(&skpv->vec[i], noiseseed, nonce++);
#endif
  (void)i;
}

static void keypair_basemul(polyvec *pkpv, polyvec *a, const polyvec *skpv)
{
#if (KYBER_K == 2)
  polyvec_basemul_acc_montgomery(&pkpv->vec[0], &a[0], skpv);
  polyvec_basemul_acc_montgomery(&pkpv->vec[1], &a[1], skpv);
#elif (KYBER_K == 4)
  polyvec_basemul_acc_montgomery(&pkpv->vec[0], &a[0], skpv);
  polyvec_basemul_acc_montgomery(&pkpv->vec[1], &a[1], skpv);
  polyvec_basemul_acc_montgomery(&pkpv->vec[2], &a[2], skpv);
  polyvec_basemul_acc_montgomery(&pkpv->vec[3], &a[3], skpv);
#else
  unsigned int i;
  for(i = 0; i < KYBER_K; i++)
    polyvec_basemul_acc_montgomery(&pkpv->vec[i], &a[i], skpv);
#endif
}

static void keypair_pack(uint8_t pk[KYBER_INDCPA_PUBLICKEYBYTES],
                         uint8_t sk[KYBER_INDCPA_SECRETKEYBYTES],
                         const polyvec *pkpv, const polyvec *skpv,
                         const uint8_t publicseed[KYBER_SYMBYTES])
{
#ifdef PK_COMPRESS
  polyvec_compress_pk(pk, pkpv);
  memcpy(pk + KYBER_PK_POLYVECBYTES, publicseed, KYBER_SYMBYTES);
#else
  polyvec_tobytes(pk, pkpv);
  memcpy(pk + KYBER_POLYVECBYTES, publicseed, KYBER_SYMBYTES);
#endif
  polyvec_tobytes(sk, skpv);
}

static void indcpa_enc_body_profiled(uint8_t *ct,
                                     const uint8_t m[KYBER_INDCPA_MSGBYTES],
                                     const uint8_t pk[KYBER_INDCPA_PUBLICKEYBYTES],
                                     const uint8_t coins[KYBER_SYMBYTES],
                                     uint64_t seg[E_SEG_BODY_COUNT])
{
  unsigned int i;
  uint64_t t0, t1;
  uint8_t nonce = 0;

  memset(seg, 0, E_SEG_BODY_COUNT * sizeof(uint64_t));

#ifdef INV_Q_LIFTING
  t0 = cpucycles();
  polyvec_fromcompressed_pk(&prof_pkpv, pk);
  memcpy(prof_seed, pk + KYBER_PK_POLYVECBYTES, KYBER_SYMBYTES);
  polyvec_invq(&prof_pkpv, coins, nonce++);
  polyvec_ntt(&prof_pkpv);
  t1 = cpucycles();
  seg[E_SEG_INVQ] = cycles_span(t0, t1);
#else
  t0 = cpucycles();
  unpack_pk(&prof_pkpv, prof_seed, pk);
#ifdef PK_COMPRESS
  polyvec_ntt(&prof_pkpv);
#endif
  t1 = cpucycles();
  seg[E_SEG_INVQ] = cycles_span(t0, t1);
#endif

  t0 = cpucycles();
  poly_frommsg(&prof_k, m);
  t1 = cpucycles();
  seg[E_SEG_MSGENC] = cycles_span(t0, t1);

  t0 = cpucycles();
  gen_matrix(prof_at, prof_seed, 1);
  t1 = cpucycles();
  seg[E_SEG_GEN_MATRIX] = cycles_span(t0, t1);

  t0 = cpucycles();
#if (KYBER_K == 2)
  poly_getnoise_eta1(&prof_sp.vec[0], coins, nonce++);
  poly_getnoise_eta1(&prof_sp.vec[1], coins, nonce++);
#elif (KYBER_K == 4)
  poly_getnoise_eta1(&prof_sp.vec[0], coins, nonce++);
  poly_getnoise_eta1(&prof_sp.vec[1], coins, nonce++);
  poly_getnoise_eta1(&prof_sp.vec[2], coins, nonce++);
  poly_getnoise_eta1(&prof_sp.vec[3], coins, nonce++);
#else
  for(i = 0; i < KYBER_K; i++)
    poly_getnoise_eta1(&prof_sp.vec[i], coins, nonce++);
#endif
  t1 = cpucycles();
  seg[E_SEG_CBD] = cycles_span(t0, t1);

  t0 = cpucycles();
  polyvec_ntt(&prof_sp);
  for(i = 0; i < KYBER_K; i++)
    polyvec_basemul_acc_montgomery(&prof_bp.vec[i], &prof_at[i], &prof_sp);
  polyvec_basemul_acc_montgomery(&prof_v, &prof_pkpv, &prof_sp);
  polyvec_invntt_tomont(&prof_bp);
  poly_invntt_tomont(&prof_v);
  poly_add(&prof_v, &prof_v, &prof_k);
  polyvec_reduce(&prof_bp);
  poly_reduce(&prof_v);
  t1 = cpucycles();
  seg[E_SEG_NTT_BASEMUL] = cycles_span(t0, t1);

  t0 = cpucycles();
  polyvec_compress(ct, &prof_bp);
  poly_compress(ct + KYBER_POLYVECCOMPRESSEDBYTES, &prof_v);
  t1 = cpucycles();
  seg[E_SEG_COMPRESS_PACK] = cycles_span(t0, t1);
}

static void kem_enc_profiled(uint8_t *ct, uint8_t *ss, const uint8_t *pk,
                             uint64_t out[ENC_SEG_TOTAL])
{
  uint64_t body[E_SEG_BODY_COUNT];
  uint64_t t0, t1;
  uint8_t coins[KYBER_INDCPA_MSGBYTES];

  memset(out, 0, ENC_SEG_TOTAL * sizeof(uint64_t));

  t0 = cpucycles();
  randombytes(coins, sizeof(coins));
  memcpy(prof_buf, coins, KYBER_INDCPA_MSGBYTES);
  hash_h(prof_buf + KYBER_INDCPA_MSGBYTES, pk, KYBER_PUBLICKEYBYTES);
  shake256(prof_kr, sizeof(prof_kr), prof_buf, sizeof(prof_buf));
  t1 = cpucycles();
  out[ENC_SEG_RNG_HASH] = cycles_span(t0, t1);

  indcpa_enc_body_profiled(ct, prof_buf, pk, prof_kr + KYBER_SSBYTES, body);
  memcpy(out + ENC_SEG_COUNT, body, sizeof(body));

  memcpy(ss, prof_kr, KYBER_SSBYTES);
}

static void indcpa_keypair_body_profiled(uint8_t pk[KYBER_INDCPA_PUBLICKEYBYTES],
                                         uint8_t sk[KYBER_INDCPA_SECRETKEYBYTES],
                                         const uint8_t coins[KYBER_SYMBYTES],
                                         uint64_t seg[KP_SEG_BODY_COUNT])
{
  uint64_t t0, t1;
  __attribute__((aligned(32)))
  uint8_t buf[2 * KYBER_SYMBYTES];
  const uint8_t *publicseed = buf;
  const uint8_t *noiseseed = buf + KYBER_SYMBYTES;

  memset(seg, 0, KP_SEG_BODY_COUNT * sizeof(uint64_t));

  t0 = cpucycles();
  memcpy(buf, coins, KYBER_SYMBYTES);
  buf[KYBER_SYMBYTES] = KYBER_K;
  hash_g(buf, buf, KYBER_SYMBYTES + 1);
  t1 = cpucycles();
  seg[KP_SEG_HASH_SEED] = cycles_span(t0, t1);

  t0 = cpucycles();
  gen_matrix(kp_a, publicseed, 0);
  t1 = cpucycles();
  seg[KP_SEG_GEN_MATRIX] = cycles_span(t0, t1);

  t0 = cpucycles();
  keypair_noise(&kp_skpv, noiseseed);
  t1 = cpucycles();
  seg[KP_SEG_CBD] = cycles_span(t0, t1);

  t0 = cpucycles();
  polyvec_ntt(&kp_skpv);
  t1 = cpucycles();
  seg[KP_SEG_NTT_SK] = cycles_span(t0, t1);

  t0 = cpucycles();
  keypair_basemul(&kp_pkpv, kp_a, &kp_skpv);
  t1 = cpucycles();
  seg[KP_SEG_NTT_BASEMUL] = cycles_span(t0, t1);

#ifdef PK_COMPRESS
  t0 = cpucycles();
  polyvec_invntt_tomont(&kp_pkpv);
  polyvec_reduce(&kp_pkpv);
  t1 = cpucycles();
  seg[KP_SEG_INVNTT_REDUCE] = cycles_span(t0, t1);
#else
  t0 = cpucycles();
  polyvec_reduce(&kp_pkpv);
  t1 = cpucycles();
  seg[KP_SEG_INVNTT_REDUCE] = cycles_span(t0, t1);
#endif

  t0 = cpucycles();
  keypair_pack(pk, sk, &kp_pkpv, &kp_skpv, publicseed);
  t1 = cpucycles();
  seg[KP_SEG_PACK] = cycles_span(t0, t1);
}

static void kem_keypair_profiled(uint8_t *pk, uint8_t *sk,
                                 const uint8_t coins[2 * KYBER_SYMBYTES],
                                 uint64_t out[KP_SEG_TOTAL])
{
  uint64_t body[KP_SEG_BODY_COUNT];
  uint64_t t0, t1;

  indcpa_keypair_body_profiled(pk, sk, coins, body);
  memcpy(out + KP_SEG_WRAP_COUNT, body, sizeof(body));

  t0 = cpucycles();
  memcpy(sk + KYBER_INDCPA_SECRETKEYBYTES, pk, KYBER_PUBLICKEYBYTES);
  hash_h(sk + KYBER_SECRETKEYBYTES - 2 * KYBER_SYMBYTES, pk, KYBER_PUBLICKEYBYTES);
  memcpy(sk + KYBER_SECRETKEYBYTES - KYBER_SYMBYTES,
         coins + KYBER_SYMBYTES, KYBER_SYMBYTES);
  t1 = cpucycles();
  out[KP_SEG_KEM_ASSEMBLE] = cycles_span(t0, t1);
}

static void kem_keypair_full_profiled(uint8_t *pk, uint8_t *sk,
                                      uint64_t out[KP_SEG_TOTAL])
{
  uint8_t coins[2 * KYBER_SYMBYTES];
  uint64_t t0, t1;

  memset(out, 0, KP_SEG_TOTAL * sizeof(uint64_t));

  t0 = cpucycles();
  randombytes(coins, 2 * KYBER_SYMBYTES);
  t1 = cpucycles();
  out[KP_SEG_RNG] = cycles_span(t0, t1);

  kem_keypair_profiled(pk, sk, coins, out);
}

static void kem_dec_profiled(uint8_t *ss, const uint8_t *ct, const uint8_t *sk,
                             uint64_t out[D_SEG_TOTAL])
{
  const uint8_t *pk = sk + KYBER_INDCPA_SECRETKEYBYTES;
  uint64_t reenc[E_SEG_BODY_COUNT];
  uint64_t t0, t1;
  int fail;
  uint8_t cmp[KYBER_CIPHERTEXTBYTES];

  memset(out, 0, D_SEG_TOTAL * sizeof(uint64_t));

  t0 = cpucycles();
  polyvec_decompress(&dec_b, ct);
  poly_decompress(&dec_v, ct + KYBER_POLYVECCOMPRESSEDBYTES);
  polyvec_frombytes(&dec_skpv, sk);
  t1 = cpucycles();
  out[D_SEG_DEC_UNPACK] = cycles_span(t0, t1);

  t0 = cpucycles();
  polyvec_ntt(&dec_b);
  polyvec_basemul_acc_montgomery(&dec_mp, &dec_skpv, &dec_b);
  poly_invntt_tomont(&dec_mp);
  poly_sub(&dec_mp, &dec_v, &dec_mp);
  poly_reduce(&dec_mp);
  t1 = cpucycles();
  out[D_SEG_DEC_NTT_BASEMUL] = cycles_span(t0, t1);

  t0 = cpucycles();
  poly_tomsg(prof_buf, &dec_mp);
  t1 = cpucycles();
  out[D_SEG_DEC_TOMSG] = cycles_span(t0, t1);

  t0 = cpucycles();
  memcpy(prof_buf + KYBER_INDCPA_MSGBYTES,
         sk + KYBER_SECRETKEYBYTES - 2 * KYBER_SYMBYTES, KYBER_SYMBYTES);
  shake256(prof_kr, sizeof(prof_kr), prof_buf, sizeof(prof_buf));
  t1 = cpucycles();
  out[D_SEG_SHAKE256] = cycles_span(t0, t1);

  indcpa_enc_body_profiled(cmp, prof_buf, pk, prof_kr + KYBER_SSBYTES, reenc);
  memcpy(out + D_SEG_KEM_COUNT, reenc, sizeof(reenc));

  t0 = cpucycles();
  fail = verify(ct, cmp, KYBER_CIPHERTEXTBYTES);
  t1 = cpucycles();
  out[D_SEG_VERIFY] = cycles_span(t0, t1);

  t0 = cpucycles();
  rkprf(ss, sk + KYBER_SECRETKEYBYTES - KYBER_SYMBYTES, ct);
  t1 = cpucycles();
  out[D_SEG_RKPRF] = cycles_span(t0, t1);

  t0 = cpucycles();
  cmov(ss, prof_kr, KYBER_SSBYTES, (uint8_t)!fail);
  t1 = cpucycles();
  out[D_SEG_CMOV] = cycles_span(t0, t1);
}

static void bench_enc(const uint8_t *pk)
{
  unsigned int i, s;
  uint8_t ct[CRYPTO_CIPHERTEXTBYTES];
  uint8_t ss[CRYPTO_BYTES];
  uint64_t seg[ENC_SEG_TOTAL];
  uint64_t t0, t1;

  for(i = 0; i < NTESTS; i++) {
    t0 = cpucycles();
    kem_enc_profiled(ct, ss, pk, seg);
    t1 = cpucycles();
    t_wall_enc[i] = cycles_span(t0, t1);

    for(s = 0; s < ENC_SEG_TOTAL; s++)
      t_enc[s][i] = seg[s];

    t0 = cpucycles();
    crypto_kem_enc(ct, ss, pk);
    t1 = cpucycles();
    t_api_enc[i] = cycles_span(t0, t1);
  }
}

static void bench_keypair(void)
{
  unsigned int i, s;
  uint8_t pk[CRYPTO_PUBLICKEYBYTES];
  uint8_t sk[CRYPTO_SECRETKEYBYTES];
  uint64_t seg[KP_SEG_TOTAL];
  uint64_t t0, t1;

  for(i = 0; i < NTESTS; i++) {
    t0 = cpucycles();
    kem_keypair_full_profiled(pk, sk, seg);
    t1 = cpucycles();
    t_wall_kp[i] = cycles_span(t0, t1);

    for(s = 0; s < KP_SEG_TOTAL; s++)
      t_kp[s][i] = seg[s];

    t0 = cpucycles();
    crypto_kem_keypair(pk, sk);
    t1 = cpucycles();
    t_api_kp[i] = cycles_span(t0, t1);
  }
}

static void bench_dec(const uint8_t *pk, const uint8_t *sk)
{
  unsigned int i, s;
  uint8_t ct[CRYPTO_CIPHERTEXTBYTES];
  uint8_t ss[CRYPTO_BYTES];
  uint64_t seg[D_SEG_TOTAL];
  uint64_t t0, t1;

  crypto_kem_enc(ct, ss, pk);

  for(i = 0; i < NTESTS; i++) {
    t0 = cpucycles();
    kem_dec_profiled(ss, ct, sk, seg);
    t1 = cpucycles();
    t_wall_dec[i] = cycles_span(t0, t1);

    for(s = 0; s < D_SEG_TOTAL; s++)
      t_dec[s][i] = seg[s];

    t0 = cpucycles();
    crypto_kem_dec(ss, ct, sk);
    t1 = cpucycles();
    t_api_dec[i] = cycles_span(t0, t1);
  }
}

int main(int argc, char **argv)
{
  uint8_t pk[CRYPTO_PUBLICKEYBYTES];
  uint8_t sk[CRYPTO_SECRETKEYBYTES];
  uint8_t entropy[48];

  g_nreports = 0;
  parse_output_path(argc, argv);

  memset(entropy, 0x42, sizeof(entropy));
  randombytes_init(entropy, NULL, 256);

  printf("Weaver AVX2 KEM operation breakdown\n");
  printf("CSV export: %s (open with Excel)\n", g_csv_path);
#ifdef INV_Q_LIFTING
  printf("PK path: Inv_q lifting\n");
#else
  printf("PK path: unpack_pk (+ NTT if PK_COMPRESS)\n");
#endif

  bench_keypair();
  print_section("Keypair", "crypto_kem_keypair",
                kp_seg_name, KP_SEG_TOTAL, t_api_kp, t_wall_kp, t_kp);

  crypto_kem_keypair(pk, sk);

  bench_enc(pk);
  print_section("Encapsulation", "crypto_kem_enc",
                enc_seg_name, ENC_SEG_TOTAL, t_api_enc, t_wall_enc, t_enc);

  bench_dec(pk, sk);
  print_section("Decapsulation", "crypto_kem_dec",
                dec_seg_name, D_SEG_TOTAL, t_api_dec, t_wall_dec, t_dec);

  if(export_breakdown_csv(g_csv_path) == 0)
    printf("\nWrote spreadsheet table to: %s\n", g_csv_path);
  else
    return 1;

  return 0;
}
