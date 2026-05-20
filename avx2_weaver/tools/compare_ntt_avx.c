#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "params.h"
#include "ntt.h"
#include "reduce.h"
#include "avx/consts.h"

#if WEAVER_MODE == 1
#define avx_ntt pqcrystals_weaver512_avx2_ntt_avx
#define avx_invntt pqcrystals_weaver512_avx2_invntt_avx
#define avx_basemul pqcrystals_weaver512_avx2_basemul_avx
#define avx_reduce pqcrystals_weaver512_avx2_reduce_avx
#define avx_tomont pqcrystals_weaver512_avx2_tomont_avx
#define avx_nttunpack pqcrystals_weaver512_avx2_nttunpack_avx
#define avx_qdata pqcrystals_weaver512_avx2_qdata
#elif WEAVER_MODE == 3
#define avx_ntt pqcrystals_weaver1024_avx2_ntt_avx
#define avx_invntt pqcrystals_weaver1024_avx2_invntt_avx
#define avx_basemul pqcrystals_weaver1024_avx2_basemul_avx
#define avx_reduce pqcrystals_weaver1024_avx2_reduce_avx
#define avx_tomont pqcrystals_weaver1024_avx2_tomont_avx
#define avx_nttunpack pqcrystals_weaver1024_avx2_nttunpack_avx
#define avx_qdata pqcrystals_weaver1024_avx2_qdata
#elif WEAVER_MODE == 5
#define avx_ntt pqcrystals_weaver2048_avx2_ntt512_avx
#define avx_invntt pqcrystals_weaver2048_avx2_invntt512_avx
#define avx_basemul pqcrystals_weaver2048_avx2_basemul512_avx
#define avx_reduce pqcrystals_weaver2048_avx2_reduce512_avx
#define avx_tomont pqcrystals_weaver2048_avx2_tomont512_avx
#define avx_qdata pqcrystals_weaver2048_avx2_qdata
#else
#error "compare_ntt_avx supports WEAVER_MODE 1, 3, or 5"
#endif

#if WEAVER_MODE == 5
extern void avx_ntt(int16_t *r);
extern void avx_invntt(int16_t *r);
extern void avx_basemul(int16_t *r, const int16_t *a, const int16_t *b);
extern int16_t avx_reduce(int16_t *r, const int16_t *qdata);
extern int16_t avx_tomont(int16_t *r, const int16_t *qdata);
extern const int16_t avx_qdata[];

static void call_avx_ntt(int16_t *r) { avx_ntt(r); }
static void call_avx_invntt(int16_t *r) { avx_invntt(r); }
static void call_avx_basemul(int16_t *r, const int16_t *a, const int16_t *b)
{
  avx_basemul(r, a, b);
}
#else
extern void avx_ntt(int16_t *r, const int16_t *qdata);
extern void avx_invntt(int16_t *r, const int16_t *qdata);
extern void avx_basemul(int16_t *r, const int16_t *a, const int16_t *b, const int16_t *qdata);
extern int16_t avx_reduce(int16_t *r, const int16_t *qdata);
extern int16_t avx_tomont(int16_t *r, const int16_t *qdata);
extern void avx_nttunpack(int16_t *r, const int16_t *qdata);
extern const int16_t avx_qdata[];

static void call_avx_ntt(int16_t *r) { avx_ntt(r, avx_qdata); }
static void call_avx_invntt(int16_t *r) { avx_invntt(r, avx_qdata); }
static void call_avx_basemul(int16_t *r, const int16_t *a, const int16_t *b)
{
  avx_basemul(r, a, b, avx_qdata);
}
#endif

#if WEAVER_MODE == 5
#define ref_ntt KYBER_NAMESPACE(_ntt)
#define ref_invntt KYBER_NAMESPACE(_invntt)
#define ref_zetas KYBER_NAMESPACE(_zetas)
#define ref_barrett_reduce KYBER_NAMESPACE(_barrett_reduce)
#define ref_montgomery_reduce KYBER_NAMESPACE(_montgomery_reduce)
#else
/* Build an in-process reference implementation under a separate namespace. */
#undef KYBER_NAMESPACE
#define KYBER_NAMESPACE(s) refcmp##s
#include "reduce.c"
#include "ntt.c"

#define ref_ntt KYBER_NAMESPACE(_ntt)
#define ref_invntt KYBER_NAMESPACE(_invntt)
#define ref_basemul KYBER_NAMESPACE(_basemul)
#define ref_zetas KYBER_NAMESPACE(_zetas)
#define ref_barrett_reduce KYBER_NAMESPACE(_barrett_reduce)
#define ref_montgomery_reduce KYBER_NAMESPACE(_montgomery_reduce)
#endif

static uint32_t rng_state = 1;

static uint32_t xorshift32(void)
{
  uint32_t x = rng_state;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  rng_state = x;
  return x;
}

static int first_diff(const int16_t *a, const int16_t *b, size_t n)
{
  size_t i;
  for(i = 0; i < n; i++) {
    if(a[i] != b[i]) return (int)i;
  }
  return -1;
}

static int modq(int16_t x)
{
  int v = (int)x % KYBER_Q;
  if(v < 0) v += KYBER_Q;
  return v;
}

static int first_diff_modq(const int16_t *a, const int16_t *b, size_t n)
{
  size_t i;
  for(i = 0; i < n; i++) {
    if(modq(a[i]) != modq(b[i])) return (int)i;
  }
  return -1;
}

static void fill_poly(int16_t *r)
{
  size_t i;
  for(i = 0; i < KYBER_N; i++) {
    r[i] = (int16_t)(xorshift32() % KYBER_Q);
  }
}

int main(void)
{
  unsigned int t;
  int idx;
  int errors = 0;
  int fail_invntt = 0, fail_acc = 0, fail_fullmul = 0, fail_reduce = 0, fail_tomont = 0;
  int16_t a_ref[KYBER_N] __attribute__((aligned(32)));
  int16_t a_avx[KYBER_N] __attribute__((aligned(32)));
  int16_t b_ref[KYBER_N] __attribute__((aligned(32)));
  int16_t b_avx[KYBER_N] __attribute__((aligned(32)));
  int16_t r_ref[KYBER_N] __attribute__((aligned(32)));
  int16_t r_avx[KYBER_N] __attribute__((aligned(32)));
  const int16_t f = (1ULL << 32) % KYBER_Q;

  printf("compare_ntt_avx: mode=%d n=%d k=%d\n", WEAVER_MODE, KYBER_N, KYBER_K);

#if WEAVER_MODE != 5
  printf("SKIP ntt (per-coeff): AVX output is shuffled; use acc_montgomery / full_mul instead\n");
#endif

  for(t = 0; t < 2000; t++) {
    fill_poly(a_ref);
    memcpy(a_avx, a_ref, sizeof(a_ref));

    ref_ntt(a_ref);
    call_avx_ntt(a_avx);
    ref_invntt(a_ref);
    call_avx_invntt(a_avx);
    idx = first_diff_modq(a_ref, a_avx, KYBER_N);
    if(idx >= 0) {
      printf("FAIL invntt at test %u idx %d: ref=%d avx=%d\n", t, idx, a_ref[idx], a_avx[idx]);
      errors++;
      fail_invntt = 1;
      break;
    }
  }
  if(!fail_invntt) printf("PASS invntt (mod q)\n");

#if WEAVER_MODE != 5
  printf("SKIP basemul (both NTT): operands must use gen_matrix layout (nttunpack x NTT)\n");

  for(t = 0; t < 1000; t++) {
    fill_poly(a_ref);
    fill_poly(b_ref);
    memcpy(a_avx, a_ref, sizeof(a_ref));
    memcpy(b_avx, b_ref, sizeof(b_ref));

    ref_ntt(b_ref);
    call_avx_ntt(b_avx);
    avx_nttunpack(a_avx, avx_qdata);

    for(unsigned int i = 0; i < KYBER_N / 4; i++) {
      ref_basemul(&r_ref[4 * i], &a_ref[4 * i], &b_ref[4 * i], ref_zetas[64 + i]);
      ref_basemul(&r_ref[4 * i + 2], &a_ref[4 * i + 2], &b_ref[4 * i + 2], -ref_zetas[64 + i]);
    }
    ref_invntt(r_ref);

    call_avx_basemul(r_avx, a_avx, b_avx);
    call_avx_invntt(r_avx);

    idx = first_diff_modq(r_ref, r_avx, KYBER_N);
    if(idx >= 0) {
      printf("FAIL acc_montgomery at test %u idx %d: ref=%d avx=%d\n",
             t, idx, r_ref[idx], r_avx[idx]);
      errors++;
      fail_acc = 1;
      break;
    }
  }
  if(!fail_acc) printf("PASS acc_montgomery (time-domain A + nttunpack vs ref)\n");
#else
  printf("SKIP acc_montgomery (n=512 standard layout; use full_mul)\n");
#endif

  for(t = 0; t < 1000; t++) {
    fill_poly(a_ref);
    fill_poly(b_ref);
    memcpy(a_avx, a_ref, sizeof(a_ref));
    memcpy(b_avx, b_ref, sizeof(b_ref));

    ref_ntt(a_ref);
    ref_ntt(b_ref);
#if WEAVER_MODE == 5
    for(unsigned int i = 0; i < KYBER_N / 8; i++) {
      basemul_degree4(&r_ref[8 * i], &a_ref[8 * i], &b_ref[8 * i], ref_zetas[64 + i]);
      basemul_degree4(&r_ref[8 * i + 4], &a_ref[8 * i + 4], &b_ref[8 * i + 4], -ref_zetas[64 + i]);
    }
#else
    for(unsigned int i = 0; i < KYBER_N / 4; i++) {
      ref_basemul(&r_ref[4 * i], &a_ref[4 * i], &b_ref[4 * i], ref_zetas[64 + i]);
      ref_basemul(&r_ref[4 * i + 2], &a_ref[4 * i + 2], &b_ref[4 * i + 2], -ref_zetas[64 + i]);
    }
#endif
    ref_invntt(r_ref);

    call_avx_ntt(a_avx);
    call_avx_ntt(b_avx);
    call_avx_basemul(r_avx, a_avx, b_avx);
    call_avx_invntt(r_avx);

    idx = first_diff_modq(r_ref, r_avx, KYBER_N);
    if(idx >= 0) {
      printf("FAIL full_mul at test %u idx %d: ref=%d avx=%d\n",
             t, idx, r_ref[idx], r_avx[idx]);
      errors++;
      fail_fullmul = 1;
      break;
    }
  }
  if(!fail_fullmul) printf("PASS full_mul (mod q)\n");

  for(t = 0; t < 2000; t++) {
    fill_poly(a_ref);
    memcpy(a_avx, a_ref, sizeof(a_ref));
    for(unsigned int i = 0; i < KYBER_N; i++)
      a_ref[i] = ref_barrett_reduce(a_ref[i]);
    avx_reduce(a_avx, avx_qdata);
    idx = first_diff_modq(a_ref, a_avx, KYBER_N);
    if(idx >= 0) {
      printf("FAIL reduce at test %u idx %d: ref=%d avx=%d\n", t, idx, a_ref[idx], a_avx[idx]);
      errors++;
      fail_reduce = 1;
      break;
    }
  }
  if(!fail_reduce) printf("PASS reduce (mod q)\n");

  for(t = 0; t < 2000; t++) {
    fill_poly(a_ref);
    memcpy(a_avx, a_ref, sizeof(a_ref));
    for(unsigned int i = 0; i < KYBER_N; i++)
      a_ref[i] = ref_montgomery_reduce((int32_t)a_ref[i] * f);
    avx_tomont(a_avx, avx_qdata);
    idx = first_diff_modq(a_ref, a_avx, KYBER_N);
    if(idx >= 0) {
      printf("FAIL tomont at test %u idx %d: ref=%d avx=%d\n", t, idx, a_ref[idx], a_avx[idx]);
      errors++;
      fail_tomont = 1;
      break;
    }
  }
  if(!fail_tomont) printf("PASS tomont (mod q)\n");

  if(errors == 0) {
    printf("PASS all meaningful primitive checks (invntt/acc_montgomery/full_mul/reduce/tomont)\n");
    return 0;
  }

  printf("primitive mismatches detected: %d\n", errors);
  return 1;
}
