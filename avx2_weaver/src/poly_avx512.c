/* AVX2 NTT for Weaver n=512 (WEAVER-2048). Standard coefficient order (not Kyber shuffle).
 * All loops are secret-independent (fixed bounds, no branches on secret data). */
#include <stdint.h>
#include <immintrin.h>
#include "params.h"

#if defined(WEAVER_USE_AVX_NTT512) && (KYBER_N == 512)
#include "ntt.h"
#include "reduce.h"
#include "ntt_avx512.h"

extern const int16_t zetas[128];
extern const int16_t zetas_inv[128];

/* Montgomery multiply: 16 lanes of a * b in R (R=2^16), matches montgomery_reduce(a*b). */
static inline __m256i fqmul_avx2(__m256i a, __m256i b)
{
  const __m256i qinv_vec = _mm256_set1_epi16((int16_t)QINV);
  const __m256i q_vec = _mm256_set1_epi16(KYBER_Q);
  __m256i t = _mm256_mullo_epi16(a, b);
  __m256i u = _mm256_mullo_epi16(t, qinv_vec);
  __m256i v = _mm256_mulhi_epi16(u, q_vec);
  __m256i hi = _mm256_mulhi_epi16(a, b);
  return _mm256_sub_epi16(hi, v);
}

/* Montgomery multiply each lane by a single public zeta. */
static inline __m256i fqmul_zeta_ct(__m256i v, int16_t zeta)
{
  return fqmul_avx2(v, _mm256_set1_epi16(zeta));
}

/* Exact Barrett mod q on 16 lanes; matches barrett_reduce() per coefficient. */
static inline __m256i barrett_avx2(__m256i a)
{
  const __m256i v32 = _mm256_set1_epi32(20159);
  const __m256i bias = _mm256_set1_epi32(1 << 25);
  const __m256i q32 = _mm256_set1_epi32(KYBER_Q);
  __m128i a_lo128 = _mm256_castsi256_si128(a);
  __m128i a_hi128 = _mm256_extracti128_si256(a, 1);
  __m256i a_lo32 = _mm256_cvtepi16_epi32(a_lo128);
  __m256i a_hi32 = _mm256_cvtepi16_epi32(a_hi128);
  __m256i t_lo, t_hi, r_lo32, r_hi32, packed;

  t_lo = _mm256_srai_epi32(_mm256_add_epi32(_mm256_mullo_epi32(a_lo32, v32), bias), 26);
  t_hi = _mm256_srai_epi32(_mm256_add_epi32(_mm256_mullo_epi32(a_hi32, v32), bias), 26);
  r_lo32 = _mm256_sub_epi32(a_lo32, _mm256_mullo_epi32(t_lo, q32));
  r_hi32 = _mm256_sub_epi32(a_hi32, _mm256_mullo_epi32(t_hi, q32));
  packed = _mm256_packs_epi32(r_lo32, r_hi32);
  return _mm256_permute4x64_epi64(packed, _MM_SHUFFLE(3, 1, 2, 0));
}

static inline __m256i barrett_ct(__m256i v)
{
  return barrett_avx2(v);
}

static void ntt_layer8_avx(int16_t *r, unsigned int *k)
{
  unsigned int start;

  for(start = 0; start < KYBER_N; start += 16) {
    const __m256i zeta_vec = _mm256_set1_epi16(zetas[(*k)++]);
    __m256i uv = _mm256_load_si256((__m256i *)(r + start));
    __m128i u = _mm256_castsi256_si128(uv);
    __m128i v = _mm256_extracti128_si256(uv, 1);
    __m128i t = _mm256_castsi256_si128(fqmul_avx2(_mm256_castsi128_si256(v), zeta_vec));
    __m128i add = _mm_add_epi16(u, t);
    __m128i sub = _mm_sub_epi16(u, t);
    _mm256_store_si256((__m256i *)(r + start), _mm256_set_m128i(sub, add));
  }
}

static void ntt_layer4_block128(int16_t *p, int16_t zeta)
{
  const __m128i mask_u = _mm_set_epi64x(0, -1);
  __m128i x = _mm_load_si128((__m128i *)p);
  __m128i u = _mm_and_si128(x, mask_u);
  __m128i v = _mm_srli_si128(x, 8);
  __m128i t = _mm256_castsi256_si128(
      fqmul_avx2(_mm256_castsi128_si256(v), _mm256_set1_epi16(zeta)));
  __m128i add = _mm_add_epi16(u, t);
  __m128i sub = _mm_sub_epi16(u, t);
  _mm_store_si128((__m128i *)p, _mm_unpacklo_epi64(add, sub));
}

static void ntt_layer4_avx(int16_t *r, unsigned int *k)
{
  unsigned int start;

  for(start = 0; start < KYBER_N; start += 16) {
    ntt_layer4_block128(r + start, zetas[(*k)++]);
    ntt_layer4_block128(r + start + 8, zetas[(*k)++]);
  }
}

static void invntt_layer4_block128(int16_t *p, int16_t zeta)
{
  const __m128i mask_u = _mm_set_epi64x(0, -1);
  __m128i x = _mm_load_si128((__m128i *)p);
  __m128i u = _mm_and_si128(x, mask_u);
  __m128i v = _mm_srli_si128(x, 8);
  __m128i sum = _mm256_castsi256_si128(
      barrett_avx2(_mm256_castsi128_si256(_mm_add_epi16(u, v))));
  __m128i diff = _mm256_castsi256_si128(
      fqmul_avx2(_mm256_castsi128_si256(_mm_sub_epi16(u, v)),
                 _mm256_set1_epi16(zeta)));
  _mm_store_si128((__m128i *)p, _mm_unpacklo_epi64(sum, diff));
}

static void invntt_layer8_avx(int16_t *r, unsigned int *k)
{
  unsigned int start;

  for(start = 0; start < KYBER_N; start += 16) {
    const __m256i zeta_vec = _mm256_set1_epi16(zetas_inv[(*k)++]);
    __m256i uv = _mm256_load_si256((__m256i *)(r + start));
    __m128i u = _mm256_castsi256_si128(uv);
    __m128i v = _mm256_extracti128_si256(uv, 1);
    __m256i sum = barrett_avx2(_mm256_add_epi16(_mm256_castsi128_si256(u),
                                               _mm256_castsi128_si256(v)));
    __m128i diff = _mm256_castsi256_si128(
        fqmul_avx2(_mm256_castsi128_si256(_mm_sub_epi16(u, v)), zeta_vec));
    _mm256_store_si256((__m256i *)(r + start),
                        _mm256_set_m128i(diff, _mm256_castsi256_si128(sum)));
  }
}

static void invntt_layer4_avx(int16_t *r, unsigned int *k)
{
  unsigned int start;

  for(start = 0; start < KYBER_N; start += 16) {
    invntt_layer4_block128(r + start, zetas_inv[(*k)++]);
    invntt_layer4_block128(r + start + 8, zetas_inv[(*k)++]);
  }
}

/* Gather coefficient k from four degree-4 blocks (16 coeffs). */
static inline __m256i coeff4(const int16_t *p, int k)
{
  return _mm256_set_epi16(
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      p[12 + k], p[8 + k], p[4 + k], p[k]);
}

static inline void scatter_degree4_x4(int16_t *r,
                                      __m256i r0, __m256i r1,
                                      __m256i r2, __m256i r3)
{
  int16_t o0[16], o1[16], o2[16], o3[16];

  _mm256_store_si256((__m256i *)o0, r0);
  _mm256_store_si256((__m256i *)o1, r1);
  _mm256_store_si256((__m256i *)o2, r2);
  _mm256_store_si256((__m256i *)o3, r3);
  r[0] = o0[0];
  r[4] = o0[1];
  r[8] = o0[2];
  r[12] = o0[3];
  r[1] = o1[0];
  r[5] = o1[1];
  r[9] = o1[2];
  r[13] = o1[3];
  r[2] = o2[0];
  r[6] = o2[1];
  r[10] = o2[2];
  r[14] = o2[3];
  r[3] = o3[0];
  r[7] = o3[1];
  r[11] = o3[2];
  r[15] = o3[3];
}

static void basemul_degree4_x4_avx(int16_t *r, const int16_t *a, const int16_t *b,
                                   int16_t z_lo, int16_t z_hi)
{
  /* lane i uses zeta for block i: +z_lo, -z_lo, +z_hi, -z_hi */
  const __m256i zeta_v = _mm256_set_epi16(
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
      -z_hi, z_hi, -z_lo, z_lo);
  __m256i a0, a1, a2, a3, b0, b1, b2, b3;
  __m256i t0, t1, t2, r0, r1, r2, r3;

  a0 = coeff4(a, 0);
  a1 = coeff4(a, 1);
  a2 = coeff4(a, 2);
  a3 = coeff4(a, 3);
  b0 = coeff4(b, 0);
  b1 = coeff4(b, 1);
  b2 = coeff4(b, 2);
  b3 = coeff4(b, 3);

  t0 = fqmul_avx2(a1, b3);
  t0 = _mm256_add_epi16(t0, fqmul_avx2(a2, b2));
  t0 = _mm256_add_epi16(t0, fqmul_avx2(a3, b1));

  t1 = fqmul_avx2(a2, b3);
  t1 = _mm256_add_epi16(t1, fqmul_avx2(a3, b2));

  t2 = fqmul_avx2(a3, b3);

  r0 = fqmul_avx2(t0, zeta_v);
  r0 = _mm256_add_epi16(r0, fqmul_avx2(a0, b0));

  r1 = fqmul_avx2(t1, zeta_v);
  r1 = _mm256_add_epi16(r1, fqmul_avx2(a0, b1));
  r1 = _mm256_add_epi16(r1, fqmul_avx2(a1, b0));

  r2 = fqmul_avx2(t2, zeta_v);
  r2 = _mm256_add_epi16(r2, fqmul_avx2(a0, b2));
  r2 = _mm256_add_epi16(r2, fqmul_avx2(a1, b1));
  r2 = _mm256_add_epi16(r2, fqmul_avx2(a2, b0));

  r3 = fqmul_avx2(a0, b3);
  r3 = _mm256_add_epi16(r3, fqmul_avx2(a1, b2));
  r3 = _mm256_add_epi16(r3, fqmul_avx2(a2, b1));
  r3 = _mm256_add_epi16(r3, fqmul_avx2(a3, b0));

  scatter_degree4_x4(r, r0, r1, r2, r3);
}

/* Cooley-Tukey butterfly: (u,v) -> (u+t, u-t) with t = fqmul(zeta, v). */
static void butterfly_block(int16_t *r, unsigned start, unsigned len, int16_t zeta)
{
  unsigned int j;

  if(len >= 16) {
    for(j = start; j < start + len; j += 16) {
      __m256i u = _mm256_load_si256((__m256i *)&r[j]);
      __m256i v = _mm256_load_si256((__m256i *)&r[j + len]);
      __m256i t = fqmul_zeta_ct(v, zeta);
      _mm256_store_si256((__m256i *)&r[j], _mm256_add_epi16(u, t));
      _mm256_store_si256((__m256i *)&r[j + len], _mm256_sub_epi16(u, t));
    }
    for(; j < start + len; j++) {
      int16_t t = montgomery_reduce((int32_t)zeta * r[j + len]);
      int16_t u = r[j];
      r[j] = u + t;
      r[j + len] = u - t;
    }
  } else {
    for(j = start; j < start + len; j++) {
      int16_t t = montgomery_reduce((int32_t)zeta * r[j + len]);
      int16_t u = r[j];
      r[j] = u + t;
      r[j + len] = u - t;
    }
  }
}

/* Gentleman-Sande butterfly for inverse NTT. */
static void inv_butterfly_block(int16_t *r, unsigned start, unsigned len, int16_t zeta)
{
  unsigned int j;

  if(len >= 16) {
    for(j = start; j < start + len; j += 16) {
      __m256i u = _mm256_load_si256((__m256i *)&r[j]);
      __m256i v = _mm256_load_si256((__m256i *)&r[j + len]);
      __m256i sum = _mm256_add_epi16(u, v);
      __m256i diff = _mm256_sub_epi16(u, v);
      _mm256_store_si256((__m256i *)&r[j], barrett_ct(sum));
      _mm256_store_si256((__m256i *)&r[j + len], fqmul_zeta_ct(diff, zeta));
    }
    for(; j < start + len; j++) {
      int16_t u = r[j];
      int16_t v = r[j + len];
      r[j] = barrett_reduce(u + v);
      r[j + len] = montgomery_reduce((int32_t)zeta * (u - v));
    }
  } else {
    for(j = start; j < start + len; j++) {
      int16_t u = r[j];
      int16_t v = r[j + len];
      r[j] = barrett_reduce(u + v);
      r[j + len] = montgomery_reduce((int32_t)zeta * (u - v));
    }
  }
}

void ntt512_avx(int16_t *r)
{
  unsigned int len, start, k;

  k = 1;
  for(len = 256; len >= 16; len >>= 1) {
    for(start = 0; start < KYBER_N; start += 2 * len)
      butterfly_block(r, start, len, zetas[k++]);
  }

  ntt_layer8_avx(r, &k);
  ntt_layer4_avx(r, &k);
}

void invntt512_avx(int16_t *r)
{
  unsigned int len, start, k, i;

  k = 0;
  invntt_layer4_avx(r, &k);
  invntt_layer8_avx(r, &k);

  for(len = 16; len <= 256; len <<= 1) {
    for(start = 0; start < KYBER_N; start += 2 * len)
      inv_butterfly_block(r, start, len, zetas_inv[k++]);
  }

  for(i = 0; i < KYBER_N; i += 16) {
    __m256i v = _mm256_load_si256((__m256i *)&r[i]);
    _mm256_store_si256((__m256i *)&r[i], fqmul_zeta_ct(v, zetas_inv[127]));
  }
}

void basemul512_avx(int16_t *r, const int16_t *a, const int16_t *b)
{
  unsigned int i;

  for(i = 0; i < KYBER_N / 16; i++) {
    basemul_degree4_x4_avx(&r[16 * i], &a[16 * i], &b[16 * i],
                           zetas[64 + 2 * i], zetas[64 + 2 * i + 1]);
  }
}

void poly_add512_avx(int16_t *r, const int16_t *a, const int16_t *b)
{
  unsigned int i;
  for(i = 0; i < KYBER_N; i += 16) {
    __m256i f0 = _mm256_load_si256((__m256i *)&a[i]);
    __m256i f1 = _mm256_load_si256((__m256i *)&b[i]);
    _mm256_store_si256((__m256i *)&r[i], _mm256_add_epi16(f0, f1));
  }
}

void poly_sub512_avx(int16_t *r, const int16_t *a, const int16_t *b)
{
  unsigned int i;
  for(i = 0; i < KYBER_N; i += 16) {
    __m256i f0 = _mm256_load_si256((__m256i *)&a[i]);
    __m256i f1 = _mm256_load_si256((__m256i *)&b[i]);
    _mm256_store_si256((__m256i *)&r[i], _mm256_sub_epi16(f0, f1));
  }
}

#endif /* WEAVER_USE_AVX_NTT512 */
