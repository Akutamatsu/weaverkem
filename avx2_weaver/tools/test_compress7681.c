/* Differential test: q=7681 compress scalar vs AVX (modes 3/5). */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "params.h"
#include "poly.h"
#include "polyvec.h"
#include "poly_compress_avx.h"

#if WEAVER_MODE == 3
#define POLY_PER_POLY_BYTES ((WEAVER_N * 10) / 8)
#define POLYVEC_BYTES WEAVER_POLYVECCOMPRESSEDBYTES
#elif WEAVER_MODE == 5
#define POLY_PER_POLY_BYTES ((WEAVER_N * 11) / 8)
#define POLYVEC_BYTES WEAVER_POLYVECCOMPRESSEDBYTES
#else
#error "test_compress7681 requires WEAVER_MODE 3 or 5"
#endif

static void poly_compress_scalar(uint8_t *r, const poly *a)
{
  unsigned i, j, k;
  int16_t u;

#if WEAVER_MODE == 3
  for(i = 0; i < WEAVER_N / 4; i++) {
    uint16_t t[4];
    for(k = 0; k < 4; k++) {
      u = a->coeffs[4 * i + k];
      u += ((int16_t)u >> 15) & WEAVER_Q;
      t[k] = (uint16_t)((((uint32_t)u << 10) + WEAVER_Q / 2) / WEAVER_Q) & 0x3ff;
    }
    r[5 * i + 0] = (uint8_t)(t[0] >> 0);
    r[5 * i + 1] = (uint8_t)((t[0] >> 8) | (t[1] << 2));
    r[5 * i + 2] = (uint8_t)((t[1] >> 6) | (t[2] << 4));
    r[5 * i + 3] = (uint8_t)((t[2] >> 4) | (t[3] << 6));
    r[5 * i + 4] = (uint8_t)(t[3] >> 2);
  }
#elif WEAVER_MODE == 5
  for(i = 0; i < WEAVER_N / 8; i++) {
    uint16_t t[8];
    for(k = 0; k < 8; k++) {
      u = a->coeffs[8 * i + k];
      u += ((int16_t)u >> 15) & WEAVER_Q;
      t[k] = (uint16_t)((((uint32_t)u << 11) + WEAVER_Q / 2) / WEAVER_Q) & 0x7ff;
    }
    r[11 * i +  0] = (uint8_t)(t[0] >> 0);
    r[11 * i +  1] = (uint8_t)((t[0] >> 8) | (t[1] << 3));
    r[11 * i +  2] = (uint8_t)((t[1] >> 5) | (t[2] << 6));
    r[11 * i +  3] = (uint8_t)(t[2] >> 2);
    r[11 * i +  4] = (uint8_t)((t[2] >> 10) | (t[3] << 1));
    r[11 * i +  5] = (uint8_t)((t[3] >> 7) | (t[4] << 4));
    r[11 * i +  6] = (uint8_t)((t[4] >> 4) | (t[5] << 7));
    r[11 * i +  7] = (uint8_t)(t[5] >> 1);
    r[11 * i +  8] = (uint8_t)((t[5] >> 9) | (t[6] << 2));
    r[11 * i +  9] = (uint8_t)((t[6] >> 6) | (t[7] << 5));
    r[11 * i + 10] = (uint8_t)(t[7] >> 3);
  }
#endif
}

static void poly_decompress_scalar(poly *r, const uint8_t *a)
{
  unsigned i, j, k;

#if WEAVER_MODE == 3
  for(i = 0; i < WEAVER_N / 4; i++) {
    uint16_t t[4];
    t[0] = (uint16_t)a[0] | ((uint16_t)a[1] << 8);
    t[1] = (uint16_t)(a[1] >> 2) | ((uint16_t)a[2] << 6);
    t[2] = (uint16_t)(a[2] >> 4) | ((uint16_t)a[3] << 4);
    t[3] = (uint16_t)(a[3] >> 6) | ((uint16_t)a[4] << 2);
    a += 5;
    for(k = 0; k < 4; k++)
      r->coeffs[4 * i + k] = (int16_t)(((uint32_t)(t[k] & 0x3ff) * WEAVER_Q + 512) >> 10);
  }
#elif WEAVER_MODE == 5
  for(i = 0; i < WEAVER_N / 8; i++) {
    uint16_t t[8];
    t[0] = (uint16_t)a[0] | ((uint16_t)a[1] << 8);
    t[1] = (uint16_t)(a[1] >> 3) | ((uint16_t)a[2] << 5);
    t[2] = (uint16_t)(a[2] >> 6) | ((uint16_t)a[3] << 2) | ((uint16_t)a[4] << 10);
    t[3] = (uint16_t)(a[4] >> 1) | ((uint16_t)a[5] << 7);
    t[4] = (uint16_t)(a[5] >> 4) | ((uint16_t)a[6] << 4);
    t[5] = (uint16_t)(a[6] >> 7) | ((uint16_t)a[7] << 1) | ((uint16_t)a[8] << 9);
    t[6] = (uint16_t)(a[8] >> 2) | ((uint16_t)a[9] << 6);
    t[7] = (uint16_t)(a[9] >> 5) | ((uint16_t)a[10] << 3);
    a += 11;
    for(k = 0; k < 8; k++)
      r->coeffs[8 * i + k] = (int16_t)(((uint32_t)(t[k] & 0x7ff) * WEAVER_Q + 1024) >> 11);
  }
#endif
  (void)j;
}

static int test_poly_roundtrip(void)
{
  unsigned trial;
  uint8_t buf_s[POLY_PER_POLY_BYTES];
  uint8_t buf_a[POLY_PER_POLY_BYTES];
  poly a, r_s, r_a;

  for(trial = 0; trial < 5000; trial++) {
    unsigned i;
    for(i = 0; i < WEAVER_N; i++)
      a.coeffs[i] = (int16_t)((trial * 17 + i * 91) % (2 * WEAVER_Q) - WEAVER_Q);

    poly_compress_scalar(buf_s, &a);
#if WEAVER_MODE == 3
    poly_compress10_avx(buf_a, &a);
    poly_decompress_scalar(&r_s, buf_s);
    poly_decompress10_avx(&r_a, buf_a);
#elif WEAVER_MODE == 5
    poly_compress11_avx(buf_a, &a);
    poly_decompress_scalar(&r_s, buf_s);
    poly_decompress11_avx(&r_a, buf_a);
#endif

    if(memcmp(buf_s, buf_a, sizeof(buf_s)) != 0) {
      fprintf(stderr, "compress mismatch trial %u\n", trial);
      return 1;
    }
    if(memcmp(&r_s, &r_a, sizeof(r_s)) != 0) {
      fprintf(stderr, "decompress mismatch trial %u\n", trial);
      return 1;
    }
  }
  return 0;
}

static int test_polyvec_roundtrip(void)
{
  unsigned trial;
  polyvec v, r_s, r_a;
  uint8_t buf_s[POLYVEC_BYTES];
  uint8_t buf_a[POLYVEC_BYTES];

  for(trial = 0; trial < 1000; trial++) {
    unsigned i, j;
    for(i = 0; i < WEAVER_K; i++)
      for(j = 0; j < WEAVER_N; j++)
        v.vec[i].coeffs[j] = (int16_t)((trial * 31 + i * 97 + j * 13) % (2 * WEAVER_Q) - WEAVER_Q);

#if WEAVER_MODE == 3
    polyvec_compress(buf_a, &v);
    for(i = 0; i < WEAVER_K; i++)
      poly_compress_scalar(buf_s + i * POLY_PER_POLY_BYTES, &v.vec[i]);
    polyvec_decompress(&r_a, buf_a);
    for(i = 0; i < WEAVER_K; i++)
      poly_decompress_scalar(&r_s.vec[i], buf_s + i * POLY_PER_POLY_BYTES);
#elif WEAVER_MODE == 5
    polyvec_compress(buf_a, &v);
    for(i = 0; i < WEAVER_K; i++)
      poly_compress_scalar(buf_s + i * POLY_PER_POLY_BYTES, &v.vec[i]);
    polyvec_decompress(&r_a, buf_a);
    for(i = 0; i < WEAVER_K; i++)
      poly_decompress_scalar(&r_s.vec[i], buf_s + i * POLY_PER_POLY_BYTES);
#endif

    if(memcmp(buf_s, buf_a, sizeof(buf_s)) != 0) {
      fprintf(stderr, "polyvec compress mismatch trial %u\n", trial);
      return 1;
    }
    if(memcmp(&r_s, &r_a, sizeof(r_s)) != 0) {
      fprintf(stderr, "polyvec decompress mismatch trial %u\n", trial);
      return 1;
    }
  }
  return 0;
}

int main(void)
{
  if(test_poly_roundtrip() != 0)
    return 1;
  if(test_polyvec_roundtrip() != 0)
    return 1;
  printf("PASS: compress7681 scalar == AVX (mode %d)\n", WEAVER_MODE);
  return 0;
}
