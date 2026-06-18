#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "params.h"
#include "poly.h"
#include "polyvec.h"
#include "msgenc.h"

static uint16_t compress_q(int16_t x, unsigned int d)
{
  uint32_t ux = (uint32_t)((x % WEAVER_Q + WEAVER_Q) % WEAVER_Q);
  return (uint16_t)((((ux << d) + WEAVER_Q / 2) / WEAVER_Q) & ((1u << d) - 1));
}

static int check_poly_roundtrip(void)
{
  poly a, b;
  uint8_t bytes[WEAVER_POLYCOMPRESSEDBYTES];
  unsigned int i;
  unsigned int max_err = 0;
  unsigned int bound = (WEAVER_Q + (1u << WEAVER_DV)) / (1u << (WEAVER_DV + 1));

  memset(&a, 0, sizeof(a));
  memset(&b, 0, sizeof(b));
  memset(bytes, 0, sizeof(bytes));

  for(i = 0; i < WEAVER_N; i++)
    a.coeffs[i] = (int16_t)((97u * i + 123u) % WEAVER_Q);

  poly_compress(bytes, &a);
  poly_decompress(&b, bytes);

  for(i = 0; i < WEAVER_N; i++) {
    unsigned int got = compress_q(b.coeffs[i], WEAVER_DV);
    unsigned int want = compress_q(a.coeffs[i], WEAVER_DV);
    unsigned int err;

    if(got != want) {
      printf("FAIL poly compressed bucket mismatch at %u: got=%u want=%u\n", i, got, want);
      return 0;
    }

    err = (unsigned int)((a.coeffs[i] >= b.coeffs[i]) ? (a.coeffs[i] - b.coeffs[i]) : (b.coeffs[i] - a.coeffs[i]));
    if(err > WEAVER_Q / 2)
      err = WEAVER_Q - err;
    if(err > max_err)
      max_err = err;
  }

  printf("poly compress/decompress: dv=%d max_err=%u bound=%u\n", WEAVER_DV, max_err, bound);
  return 1;
}

static int check_polyvec_roundtrip(void)
{
  polyvec a, b;
  uint8_t bytes[WEAVER_POLYVECCOMPRESSEDBYTES];
  unsigned int i, j;
  unsigned int du = (unsigned int)((WEAVER_POLYVECCOMPRESSEDBYTES * 8) / (WEAVER_K * WEAVER_N));

  memset(&a, 0, sizeof(a));
  memset(&b, 0, sizeof(b));
  memset(bytes, 0, sizeof(bytes));

  for(i = 0; i < WEAVER_K; i++)
    for(j = 0; j < WEAVER_N; j++)
      a.vec[i].coeffs[j] = (int16_t)((211u * i + 17u * j + 29u) % WEAVER_Q);

  polyvec_compress(bytes, &a);
  polyvec_decompress(&b, bytes);

  for(i = 0; i < WEAVER_K; i++) {
    for(j = 0; j < WEAVER_N; j++) {
      unsigned int got = compress_q(b.vec[i].coeffs[j], du);
      unsigned int want = compress_q(a.vec[i].coeffs[j], du);
      if(got != want) {
        printf("FAIL polyvec compressed bucket mismatch at vec=%u coeff=%u: got=%u want=%u\n", i, j, got, want);
        return 0;
      }
    }
  }

  printf("polyvec compress/decompress: du=%u OK\n", du);
  return 1;
}

int main(void)
{
  printf("mode=%d N=%d K=%d q=%d dv=%d polybytes=%d ct_poly_bytes=%d vec_ct_bytes=%d\n",
         WEAVER_MODE, WEAVER_N, WEAVER_K, WEAVER_Q, WEAVER_DV,
         WEAVER_POLYBYTES, WEAVER_POLYCOMPRESSEDBYTES, WEAVER_POLYVECCOMPRESSEDBYTES);

  if(!check_poly_roundtrip())
    return 1;
  if(!check_polyvec_roundtrip())
    return 1;

  printf("PASS compress parameter test\n");
  return 0;
}
