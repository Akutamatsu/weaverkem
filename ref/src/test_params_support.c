#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "params.h"
#include "poly.h"
#include "polyvec.h"
#include "msgenc.h"
#include "invq.h"

static uint16_t compress_q(uint16_t x, unsigned int d)
{
  return (uint16_t)(((((uint32_t)x << d) + WEAVER_Q / 2) / WEAVER_Q) & ((1u << d) - 1));
}

static unsigned int pk_bits(void)
{
  return (unsigned int)((WEAVER_PK_POLYVECBYTES * 8) / (WEAVER_K * WEAVER_N));
}

static unsigned int u_bits(void)
{
  return (unsigned int)((WEAVER_POLYVECCOMPRESSEDBYTES * 8) / (WEAVER_K * WEAVER_N));
}

static int check_poly_range(const poly *p, int eta, const char *name)
{
  unsigned int i;
  for(i = 0; i < WEAVER_N; i++) {
    if(p->coeffs[i] < -eta || p->coeffs[i] > eta) {
      printf("FAIL %s range at %u: %d not in [%d,%d]\n", name, i, p->coeffs[i], -eta, eta);
      return 1;
    }
  }
  return 0;
}

static void fill_poly(poly *p)
{
  unsigned int i;
  for(i = 0; i < WEAVER_N; i++)
    p->coeffs[i] = (int16_t)((17u * i + 23u) % WEAVER_Q);
}

static void fill_polyvec(polyvec *v)
{
  unsigned int i, j;
  for(i = 0; i < WEAVER_K; i++)
    for(j = 0; j < WEAVER_N; j++)
      v->vec[i].coeffs[j] = (int16_t)((31u * i + 17u * j + 19u) % WEAVER_Q);
}

static int check_poly_compress(void)
{
  poly a, b;
  uint8_t bytes[WEAVER_POLYCOMPRESSEDBYTES];
  unsigned int i;

  memset(bytes, 0, sizeof(bytes));
  memset(&b, 0, sizeof(b));
  fill_poly(&a);
  poly_compress(bytes, &a);
  poly_decompress(&b, bytes);

  for(i = 0; i < WEAVER_POLYCUT_DIMENSION; i++) {
    if(compress_q((uint16_t)b.coeffs[i], WEAVER_DV) != compress_q((uint16_t)a.coeffs[i], WEAVER_DV)) {
      printf("FAIL v compress roundtrip at %u\n", i);
      return 1;
    }
  }
  return 0;
}

static int check_polyvec_compress(void)
{
  polyvec a, b;
  uint8_t bytes[WEAVER_POLYVECCOMPRESSEDBYTES];
  unsigned int i, j;
  unsigned int d = u_bits();

  memset(bytes, 0, sizeof(bytes));
  memset(&b, 0, sizeof(b));
  fill_polyvec(&a);
  polyvec_compress(bytes, &a);
  polyvec_decompress(&b, bytes);

  for(i = 0; i < WEAVER_K; i++) {
    for(j = 0; j < WEAVER_N; j++) {
      if(compress_q((uint16_t)b.vec[i].coeffs[j], d) != compress_q((uint16_t)a.vec[i].coeffs[j], d)) {
        printf("FAIL u compress roundtrip at vec %u coeff %u\n", i, j);
        return 1;
      }
    }
  }
  return 0;
}

#ifdef PK_COMPRESS
static int check_pk_compress_and_invq(void)
{
  polyvec a, b, raw, lifted;
  uint8_t pkbytes[WEAVER_PK_POLYVECBYTES];
  uint8_t seed[WEAVER_SYMBYTES];
  unsigned int i, j;
  unsigned int d = pk_bits();

  memset(pkbytes, 0, sizeof(pkbytes));
  memset(&b, 0, sizeof(b));
  memset(&raw, 0, sizeof(raw));
  memset(&lifted, 0, sizeof(lifted));
  memset(seed, 7, sizeof(seed));
  fill_polyvec(&a);

  polyvec_compress_pk(pkbytes, &a);
  polyvec_decompress_pk(&b, pkbytes);
  polyvec_fromcompressed_pk(&raw, pkbytes);
  lifted = raw;
  polyvec_invq(&lifted, seed, 0);

  for(i = 0; i < WEAVER_K; i++) {
    for(j = 0; j < WEAVER_N; j++) {
      uint16_t want = compress_q((uint16_t)a.vec[i].coeffs[j], d);
      if(compress_q((uint16_t)b.vec[i].coeffs[j], d) != want) {
        printf("FAIL pk compress roundtrip at vec %u coeff %u\n", i, j);
        return 1;
      }
      if(((uint16_t)raw.vec[i].coeffs[j] & ((1u << d) - 1)) != want) {
        printf("FAIL pk raw bucket at vec %u coeff %u\n", i, j);
        return 1;
      }
      if(compress_q((uint16_t)lifted.vec[i].coeffs[j], d) != want) {
        printf("FAIL invq bucket at vec %u coeff %u\n", i, j);
        return 1;
      }
    }
  }
  return 0;
}
#endif

int main(void)
{
  uint8_t seed[WEAVER_SYMBYTES];
  poly e1, e2;
  unsigned int i;

  for(i = 0; i < WEAVER_SYMBYTES; i++)
    seed[i] = (uint8_t)(i * 13u + 1u);

  poly_getnoise_eta1(&e1, seed, 0);
  poly_getnoise_eta2(&e2, seed, 1);

  printf("WEAVER_MODE=%d N=%d K=%d eta1=%d eta2=%d dt=%u du=%u dv=%d\n",
         WEAVER_MODE, WEAVER_N, WEAVER_K, WEAVER_ETA1, WEAVER_ETA2,
         pk_bits(), u_bits(), WEAVER_DV);
  printf("sizes: pk=%d sk=%d ct=%d vbytes=%d ubytes=%d\n",
         WEAVER_PUBLICKEYBYTES, WEAVER_SECRETKEYBYTES, WEAVER_CIPHERTEXTBYTES,
         WEAVER_POLYCOMPRESSEDBYTES, WEAVER_POLYVECCOMPRESSEDBYTES);

  if(check_poly_range(&e1, WEAVER_ETA1, "eta1")) return 1;
  if(check_poly_range(&e2, WEAVER_ETA2, "eta2")) return 1;
  if(check_poly_compress()) return 1;
  if(check_polyvec_compress()) return 1;
#ifdef PK_COMPRESS
  if(check_pk_compress_and_invq()) return 1;
#endif

  printf("PASS parameter support self-test\n");
  return 0;
}
